#include "includes.h"

CSGO	   g_csgo{ };;
WinAPI	   g_winapi{ };;
Netvars	   g_netvars{ };;
menu_instance g_menu{ };
EntOffsets g_entoffsets{};;

// its cleaner like this
void CSGO::init_family_signatures( )
{
    // wait for our family dll. Its module name is whatever file the injector
    // loaded (hack.dll, 25.dll, ...): known names first, then any loaded
    // module that contains familyhook's StartMove -- the waiting used to be
    // silent and endless when the file was not named hack.dll.
    const auto say = [ & ]( const std::string& s ) {
        if ( m_cvar ) {
            m_cvar->ConsoleColorPrintf( Color( 112, 128, 144 ), "[addon] " );
            m_cvar->ConsoleColorPrintf( Color( 220, 220, 225 ), "%s\n", s.c_str( ) );
        }
    };
    const auto find_family = [ ]( std::string& found_name ) -> PE::Module {
        for ( const char* name : { "hack.dll", "25.dll", "familyhook.dll", "familyhook 2024.dll" } ) {
            PE::Module m = PE::GetModule( std::string( name ) );
            if ( m ) { found_name = name; return m; }
        }

        PE::modules_t modules;
        if ( !PE::GetAllModules( modules ) )
            return {};
        for ( const auto& m : modules ) {
            // skip the game's and Windows' own modules (cheap name test first)
            const std::string name = m.GetName( );
            if ( name == "csgo.exe" || name.find( ".exe" ) != std::string::npos )
                continue;
            // familyhook is a ~1.8 MB image; skip what cannot be it so the
            // 250 ms wait loop stays cheap
            const size_t size = m.GetImageSize( );
            if ( size < 0x80000 || size > 0x1000000 )
                continue;
            wchar_t path[ MAX_PATH ] = { };
            GetModuleFileNameW( ( HMODULE )m.GetBase( ).as< uintptr_t >( ), path, MAX_PATH );
            std::wstring lower = path;
            std::transform( lower.begin( ), lower.end( ), lower.begin( ), ::towlower );
            if ( lower.find( L"\\windows\\" ) != std::wstring::npos || lower.find( L"\\bin\\" ) != std::wstring::npos )
                continue;
            if ( pattern::find( m, family_layouts::fh2024( ).sig_start_move )
                || pattern::find( m, family_layouts::fh25( ).sig_start_move ) ) {
                found_name = name;
                return m;
            }
        }
        return {};
    };

    bool announced = false;
    while ( !m_family_dll )
    {
        std::string name;
        m_family_dll = find_family( name );

        if ( !m_family_dll )
        {
            if ( !announced ) {
                say( "waiting for familyhook to be injected..." );
                announced = true;
            }
            std::this_thread::sleep_for( std::chrono::milliseconds( 250 ) );
            continue;
        }

        say( "found familyhook as " + name );

        // give it some time to initialize
        std::this_thread::sleep_for( std::chrono::milliseconds( 2100 ) );
    }

	// which build: its StartMove pattern decides the layout used everywhere
	if ( pattern::find( m_family_dll, family_layouts::fh2024( ).sig_start_move ) )
		g_fam = family_layouts::fh2024( );
	else if ( pattern::find( m_family_dll, family_layouts::fh25( ).sig_start_move ) )
		g_fam = family_layouts::fh25( );
	say( std::string( "familyhook build: " ) + g_fam.name );
	if ( g_fam.build == FamilyBuild::unknown ) {
		say( "unknown familyhook build: the helper cannot hook it" );
		return;
	}

	// familyhook's menu: the helper looks up its "stop" / "straight nade"
	// checkboxes there (switched off while a lineup runs).
	if ( g_fam.build == FamilyBuild::fh2024 ) {
		// 2024 loads MainForm through esi: call; mov esi,MainForm; mov ecx,esi
		MainForm = pattern::find( m_family_dll, XOR( "E8 ? ? ? ? BE ? ? ? ? 8B CE E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 56" ) ).at< void* >( 0x6 );
	}
	else {
		MainForm = pattern::find( m_family_dll, XOR( "B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 8B 8B" ) ).at< void* >( 0x1 );
	}
	Tabs = MainForm ? *( uintptr_t* )( ( uintptr_t )MainForm + 0x28 ) : 0;
	say( tfm::format( "menu: form %p, tabs %p", ( void* )MainForm.as< uintptr_t >( ), ( void* )Tabs.as< uintptr_t >( ) ) );
}

bool CSGO::init( ) {
	m_done = false;

	if( m_done )
		return false;

	// wait for the game to init.
	// "serverbrowser.dll" is the last module to be loaded.
	// if it gets loaded we can be ensured that the entire game done loading.

	while( !m_serverbrowser_dll ) {
		m_serverbrowser_dll = PE::GetModule( HASH( "serverbrowser.dll" ) );
		if( !m_serverbrowser_dll )
			std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
	}

	// grab some modules.
	m_kernel32_dll = PE::GetModule( HASH( "kernel32.dll" ) );
	m_user32_dll   = PE::GetModule( HASH( "user32.dll" ) );
	m_shell32_dll  = PE::GetModule( HASH( "shell32.dll" ) );
	m_shlwapi_dll  = PE::GetModule( HASH( "shlwapi.dll" ) );
	m_client_dll   = PE::GetModule( HASH( "client.dll" ) );
	m_engine_dll   = PE::GetModule( HASH( "engine.dll" ) );
	m_vstdlib_dll  = PE::GetModule( HASH( "vstdlib.dll" ) );
	m_tier0_dll    = PE::GetModule( HASH( "tier0.dll" ) );

	// import winapi functions.
	g_winapi.WideCharToMultiByte = PE::GetExport( m_kernel32_dll, HASH( "WideCharToMultiByte" ) ).as< WinAPI::WideCharToMultiByte_t >( );
	g_winapi.MultiByteToWideChar = PE::GetExport( m_kernel32_dll, HASH( "MultiByteToWideChar" ) ).as< WinAPI::MultiByteToWideChar_t >( );
	g_winapi.GetTickCount        = PE::GetExport( m_kernel32_dll, HASH( "GetTickCount" ) ).as< WinAPI::GetTickCount_t >( );
	g_winapi.VirtualProtect      = PE::GetExport( m_kernel32_dll, HASH( "VirtualProtect" ) ).as< WinAPI::VirtualProtect_t >( );
	g_winapi.VirtualQuery        = PE::GetExport( m_kernel32_dll, HASH( "VirtualQuery" ) ).as< WinAPI::VirtualQuery_t >( );
	g_winapi.CreateDirectoryA    = PE::GetExport( m_kernel32_dll, HASH( "CreateDirectoryA" ) ).as< WinAPI::CreateDirectoryA_t >( );
	g_winapi.SetWindowLongA      = PE::GetExport( m_user32_dll, HASH( "SetWindowLongA" ) ).as< WinAPI::SetWindowLongA_t >( );
	g_winapi.CallWindowProcA     = PE::GetExport( m_user32_dll, HASH( "CallWindowProcA" ) ).as< WinAPI::CallWindowProcA_t >( );
	g_winapi.SHGetFolderPathA    = PE::GetExport( m_shell32_dll, HASH( "SHGetFolderPathA" ) ).as< WinAPI::SHGetFolderPathA_t >( );
	g_winapi.PathAppendA         = PE::GetExport( m_shlwapi_dll, HASH( "PathAppendA" ) ).as< WinAPI::PathAppendA_t >( );

	// run interface collection code.
	Interfaces interfaces{ };

	// get interface pointers.
	m_client             = interfaces.get< CHLClient* >( HASH( "VClient" ) );
	m_cvar               = interfaces.get< ICvar* >( HASH( "VEngineCvar" ) );
	m_engine             = interfaces.get< IVEngineClient* >( HASH( "VEngineClient" ) );
	m_entlist            = interfaces.get< IClientEntityList* >( HASH( "VClientEntityList" ) );
	m_engine_trace       = interfaces.get< IEngineTrace* >( HASH( "EngineTraceClient" ) );
	m_input_system       = interfaces.get< IInputSystem* >( HASH( "InputSystemVersion" ) );
	m_surface            = interfaces.get< ISurface* >( HASH( "VGUI_Surface" ) );
	m_panel              = interfaces.get< IPanel* >( HASH( "VGUI_Panel" ) );
	m_engine_vgui        = interfaces.get< IEngineVGui* >( HASH( "VEngineVGui" ) );
	m_render_view        = interfaces.get< IVRenderView* >( HASH( "VEngineRenderView" ) );
	m_model_render       = interfaces.get< IVModelRender* >( HASH( "VEngineModel" ) );
	m_material_system    = interfaces.get< IMaterialSystem* >( HASH( "VMaterialSystem" ) );
	m_studio_render      = interfaces.get< CStudioRenderContext* >( HASH( "VStudioRender" ) );
	m_model_info         = interfaces.get< IVModelInfo* >( HASH( "VModelInfoClient" ) );
	m_debug_overlay      = interfaces.get< IVDebugOverlay* >( HASH( "VDebugOverlay" ) );
	m_localize           = interfaces.get< ILocalize* >( HASH( "Localize_" ) );
    m_networkstringtable = interfaces.get< INetworkStringTableContainer* >( HASH( "VEngineClientStringTable" ) );
	m_phys_props         = interfaces.get< IPhysicsSurfaceProps* >( HASH( "VPhysicsSurfaceProps" ) );
	m_game_events        = interfaces.get< IGameEventManager2* >( HASH( "GAMEEVENTSMANAGER" ), 1 );

	// convars.
	clear                                   = m_cvar->FindVar( HASH( "clear" ) );
	sv_maxunlag                             = m_cvar->FindVar( HASH( "sv_maxunlag" ) );
	cl_csm_shadows							= m_cvar->FindVar( HASH( "cl_csm_shadows" ) );
	mat_disable_fancy_blending				= m_cvar->FindVar( HASH( "mat_disable_fancy_blending" ) );
	mat_detail_tex							= m_cvar->FindVar( HASH( "mat_detail_tex" ) );
	mp_teammates_are_enemies                = m_cvar->FindVar( HASH( "mp_teammates_are_enemies" ) );
	sv_friction                             = m_cvar->FindVar( HASH( "sv_friction" ) );
	sv_penetration_type                     = m_cvar->FindVar( HASH( "sv_penetration_type" ) );
	ff_damage_reduction_bullets             = m_cvar->FindVar( HASH( "ff_damage_reduction_bullets" ) );
	ff_damage_bullet_penetration            = m_cvar->FindVar( HASH( "ff_damage_bullet_penetration" ) );

	// classes by sig.
    m_cl                 = **reinterpret_cast< CClientState *** > ( ( *reinterpret_cast< uintptr_t ** > ( m_engine ) )[ 12 ] + 0x10 );
	m_game               = pattern::find( m_engine_dll, XOR( "A1 ? ? ? ? B9 ? ? ? ? FF 75 08 FF 50 34" ) ).add( 1 ).get< CGame* >( );
	m_render             = pattern::find( m_engine_dll, XOR( "A1 ? ? ? ? B9 ? ? ? ? FF 75 0C FF 75 08 FF 50 0C" ) ).add( 1 ).get< CRender* >( );
	m_shadow_mgr         = pattern::find( m_client_dll, XOR( "A1 ? ? ? ? FF 90 ? ? ? ? 6A 00" ) ).add( 1 ).get( ).as< IClientShadowMgr* >( );
	m_view_render        = pattern::find( m_client_dll, XOR( "8B 0D ? ? ? ? 8B 01 FF 50 4C 8B 06" ) ).add( 2 ).get< CViewRender* >( 2 );
	m_gamerules          = pattern::find( m_client_dll, XOR( "8B 0D ? ? ? ? E8 ? ? ? ? 84 C0 75 6B" ) ).add( 0x2 ).get< C_CSGameRules* >( );
	m_mem_alloc          = PE::GetExport( m_tier0_dll, HASH( "g_pMemAlloc" ) ).get< IMemAlloc* >( );
	m_hookable_cl        = reinterpret_cast< void * >( reinterpret_cast< uintptr_t >( m_cl ) + 0x8 );

	// classes by offset from virtual.
	m_globals     = util::get_method( m_client, CHLClient::INIT ).add( 0x1b ).get< CGlobalVarsBase* >( 2 );
	m_input       = util::get_method( m_client, CHLClient::INACTIVATEMOUSE ).at< CInput* >( 0x1 );

	// functions.
	MD5_PseudoRandom                = pattern::find( m_client_dll, XOR( "55 8B EC 83 E4 F8 83 EC 70 6A 58" ) ).as< MD5_PseudoRandom_t >( );
	SetAbsAngles                    = pattern::find( m_client_dll, XOR( "55 8B EC 83 E4 F8 83 EC 64 53 56 57 8B F1 E8" ) );
	InvalidateBoneCache             = pattern::find( m_client_dll, XOR( "80 3D ? ? ? ? ? 74 16 A1 ? ? ? ? 48 C7 81" ) );
	LockStudioHdr                   = pattern::find( m_client_dll, XOR( "55 8B EC 51 53 8B D9 56 57 8D B3" ) );
	SetAbsOrigin                    = pattern::find( m_client_dll, XOR( "55 8B EC 83 E4 F8 51 53 56 57 8B F1" ) );
	SetAbsVelocity                  = pattern::find( m_client_dll, XOR( "55 8B EC 83 E4 F8 83 EC 0C 53 56 57 8B 7D 08 8B F1" ) );
	AngleMatrix                     = pattern::find( m_client_dll, XOR( "E8 ? ? ? ? 8B 07 89 46 0C" ) ).rel32( 0x1 ).as< AngleMatrix_t >( );
	ComputeHitboxSurroundingBox     = pattern::find( m_client_dll, XOR( "E9 ? ? ? ? 32 C0 5D" ) ).rel32( 0x1 );
	GetSequenceActivity             = pattern::find( m_client_dll, XOR( "53 56 8B F1 8B DA 85 F6 74 55" ) );
	ServerRankRevealAll             = pattern::find( m_client_dll, XOR( "55 8B EC 8B 0D ? ? ? ? 68" ) ).as< ServerRankRevealAll_t >( );
	HasC4						    = pattern::find( m_client_dll, XOR( "E8 ? ? ? ? 38 83" ) ).rel32( 0x1 );
	InvalidatePhysicsRecursive      = pattern::find( m_client_dll, XOR( "E8 ? ? ? ? 89 5E 18" ) ).rel32( 0x1 );
	IsReady                         = pattern::find( m_client_dll, XOR( "E8 ? ? ? ? 59 C2 08 00 51 E8" ) ).rel32( 0x1 ).as< IsReady_t >( );
	GetEconItemView                 = pattern::find( m_client_dll, XOR( "8B 81 ? ? ? ? 81 C1 ? ? ? ? FF 50 04 83 C0 40 C3" ) ).as< GetEconItemView_t >( );
	GetStaticData                   = pattern::find( m_client_dll, XOR( "55 8B EC 51 56 57 8B F1 E8 ? ? ? ? 0F B7 8E" ) ).as< GetStaticData_t >( );
	TEFireBullets                   = pattern::find( m_client_dll, XOR( "C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 66 A3" ) ).add( 0x2 ).to( );

    GetShotgunSpread                = pattern::find( m_client_dll, XOR( "E8 ? ? ? ? EB 38 83 EC 08" ) ).rel32< GetShotgunSpread_t >( 1 );
	BoneAccessor                    = pattern::find( m_client_dll, XOR( "8D 81 ? ? ? ? 50 8D 84 24" ) ).add( 2 ).to< size_t >( );
	AnimOverlay                     = pattern::find( m_client_dll, XOR( "8B 80 ? ? ? ? 8D 34 C8" ) ).add( 2 ).to< size_t >( );
	SpawnTime                       = pattern::find( m_client_dll, XOR( "F3 0F 5C 88 ? ? ? ? 0F" ) ).add( 4 ).to< size_t >( );
	IsLocalPlayer                   = pattern::find( m_client_dll, XOR( "74 ? 8A 83 ? ? ? ? 88" ) ).add( 4 ).to< size_t >( );
	PlayerAnimState                 = pattern::find( m_client_dll, XOR( "8B 8E ? ? ? ? 85 C9 74 3E" ) ).add( 2 ).to< size_t >( );
	studioHdr                       = pattern::find( m_client_dll, XOR( "8B 86 ? ? ? ? 89 44 24 10 85 C0" ) ).add( 2 ).to< size_t >( );
    UTIL_TraceLine                  = pattern::find( m_client_dll, XOR( "55 8B EC 83 E4 F0 83 EC 7C 56 52" ) );
    CTraceFilterSimple_vmt          = UTIL_TraceLine.add( 0x3D ).to( );
    CTraceFilterSkipTwoEntities_vmt = pattern::find( m_client_dll, XOR( "E8 ? ? ? ? F3 0F 10 84 24 ? ? ? ? 8D 84 24 ? ? ? ? F3 0F 58 44 24" ) ).rel32( 1 ).add( 0x59 ).to( );
	LastBoneSetupTime               = InvalidateBoneCache.add( 0x11 ).to< size_t >( );
	MostRecentModelBoneCounter      = InvalidateBoneCache.add( 0x1B ).to< size_t >( );
	m_beams = pattern::find( m_client_dll, XOR( "8D 04 24 50 A1 ? ? ? ? B9" ) ).add( 0x5 ).get< IViewRenderBeams* >( );
	m_hud = pattern::find( m_client_dll, XOR( "B9 ? ? ? ? 0F 94 C0 0F B6 C0 50 68" ) ).add( 0x1 ).get( ).as< CHud* >( );

	// exported functions.
	RandomSeed  = PE::GetExport( m_vstdlib_dll, HASH( "RandomSeed" ) ).as< RandomSeed_t >( );
	RandomInt   = PE::GetExport( m_vstdlib_dll, HASH( "RandomInt" ) ).as< RandomInt_t >( );
	RandomFloat = PE::GetExport( m_vstdlib_dll, HASH( "RandomFloat" ) ).as< RandomFloat_t >( );

	g_netvars.init( );
	g_entoffsets.init( );

	// init fam sigs
	init_family_signatures( );

	// init everything else
 	render::init( );
	detours::init( );

	m_done = true;
	return true;
}
