#include "includes.h"
#include "helper/helper.h"
#include "agr/agr.h"
#include "extras/extras.h"
#include "weapon_icons.h"
#include "native/nm.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;   // this DLL's base (linker-provided)

namespace {
	// familyhook's menu open: byte 0 of its MainForm, the flag the old fam
	// addon tested (its StartMove detour 1002BDB0: `cmp byte ptr [form],0`)
	// and the one familyhook's own edit think reads. The byte LockCursor
	// compares is not it: reading that one left every click strip dead.
	bool family_menu_open( ) {
		const uint8_t* form = g_csgo.MainForm ? g_csgo.MainForm.as< const uint8_t* >( ) : nullptr;
		return form && *form != 0;
	}

	// Crash log: faults (first chance, so caught ones too) are appended to
	// addon_crash.log next to this DLL as module+offset, so a crash names its
	// spot. Plain WinAPI and stack buffers only: it runs inside a fault.
	char g_crash_path[ MAX_PATH ] = { };
	LONG g_crash_count = 0;
	const void* volatile g_crash_seen[ 64 ] = { };   // one line per fault address

	void module_of( const void* addr, char* name, size_t cap, uintptr_t& base ) {
		HMODULE mod = nullptr;
		name[ 0 ] = 0; base = 0;
		if( !GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, ( LPCSTR )addr, &mod ) || !mod ) {
			lstrcpynA( name, "?", ( int )cap );
			return;
		}
		char path[ MAX_PATH ];
		if( !GetModuleFileNameA( mod, path, MAX_PATH ) ) { lstrcpynA( name, "?", ( int )cap ); return; }
		const char* slash = std::strrchr( path, '\\' );
		lstrcpynA( name, slash ? slash + 1 : path, ( int )cap );
		base = ( uintptr_t )mod;
	}

	LONG CALLBACK crash_handler( EXCEPTION_POINTERS* ep ) {
		const DWORD code = ep->ExceptionRecord->ExceptionCode;
		if( code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO
			&& code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_PRIV_INSTRUCTION )
			return EXCEPTION_CONTINUE_SEARCH;
		const void* at = ep->ExceptionRecord->ExceptionAddress;
		if( !g_crash_path[ 0 ] ) return EXCEPTION_CONTINUE_SEARCH;
		for( const void* seen : g_crash_seen ) if( seen == at ) return EXCEPTION_CONTINUE_SEARCH;
		const LONG slot = InterlockedIncrement( &g_crash_count ) - 1;
		if( slot >= ( LONG )std::size( g_crash_seen ) ) return EXCEPTION_CONTINUE_SEARCH;   // bounded
		g_crash_seen[ slot ] = at;

		char mod[ 64 ]; uintptr_t base = 0;
		module_of( at, mod, sizeof( mod ), base );
		// probes (IsBadReadPtr and co.) fault inside the system dlls on purpose
		if( !lstrcmpiA( mod, "KERNELBASE.dll" ) || !lstrcmpiA( mod, "kernel32.dll" ) || !lstrcmpiA( mod, "ntdll.dll" ) )
			return EXCEPTION_CONTINUE_SEARCH;
		const CONTEXT* c = ep->ContextRecord;
		uintptr_t ret_base = 0; char ret_mod[ 64 ];
		const void* ret = nullptr;
		// the caller, from [esp] (exact for a fault at a call target's first instructions, a hint otherwise)
		if( c->Esp && !IsBadReadPtr( ( void* )c->Esp, 4 ) ) ret = *( void** )c->Esp;
		module_of( ret, ret_mod, sizeof( ret_mod ), ret_base );

		char line[ 512 ];
		SYSTEMTIME t; GetLocalTime( &t );
		const int n = wsprintfA( line,
			"%02d:%02d:%02d code %08X at %s+%X (%08X) %s %08X | eax %08X ebx %08X ecx %08X edx %08X esi %08X edi %08X ebp %08X esp %08X | [esp] %s+%X\r\n",
			t.wHour, t.wMinute, t.wSecond, code, mod, ( unsigned )( ( uintptr_t )at - base ), ( unsigned )( uintptr_t )at,
			code == EXCEPTION_ACCESS_VIOLATION ? ( ep->ExceptionRecord->ExceptionInformation[ 0 ] ? "writing" : "reading" ) : "-",
			code == EXCEPTION_ACCESS_VIOLATION ? ( unsigned )ep->ExceptionRecord->ExceptionInformation[ 1 ] : 0u,
			c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp, ret_mod, ( unsigned )( ( uintptr_t )ret - ret_base ) );
		const HANDLE f = CreateFileA( g_crash_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
		if( f != INVALID_HANDLE_VALUE ) {
			DWORD written = 0;
			WriteFile( f, line, ( DWORD )std::max( n, 0 ), &written, nullptr );
			CloseHandle( f );
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}

	void install_crash_log( ) {
		char path[ MAX_PATH ];
		const DWORD len = GetModuleFileNameA( ( HMODULE )&__ImageBase, path, MAX_PATH );
		if( !len || len >= MAX_PATH ) return;
		char* slash = std::strrchr( path, '\\' );
		if( !slash || ( size_t )( slash - path ) + 20 >= MAX_PATH ) return;
		lstrcpyA( slash + 1, "addon_crash.log" );
		lstrcpynA( g_crash_path, path, MAX_PATH );
		AddVectoredExceptionHandler( 1, &crash_handler );
	}

	void addon_log( const std::string& s );

	// ------------------------------------------------------------- menu clicks never fire
	// Familyhook's ClientMode::CreateMove hook (25: 100AF460, thiscall float,
	// CUserCmd*, ret 8) is the last code before the engine checksums and sends
	// the command, so stripping the attack buttons after it is final whatever
	// familyhook re-adds inside. It finds CL_Move's bSendPacket through its
	// caller's EBP ([ebp] - 0x1C), so the stub must not set up a frame: it
	// calls the original with EBP untouched and strips after the return.
	void* g_cm_original = nullptr;

	void __cdecl strip_menu_attack( CUserCmd* cmd ) {
		if ( cmd && ( helper::menu_open( ) || family_menu_open( ) ) )
			cmd->m_buttons &= ~( IN_ATTACK | IN_ATTACK2 );
		ext::on_create_move_end( );   // the desync walk's send, after familyhook's fakelag
	}

	__declspec( naked ) void hk_family_create_move( ) {
		__asm {
			push dword ptr [ esp + 8 ]    // cmd
			push dword ptr [ esp + 8 ]    // frametime (the slot moved by the push)
			call dword ptr [ g_cm_original ]   // thiscall: ecx kept, callee pops both
			push eax                      // bool result
			push dword ptr [ esp + 0Ch ]  // cmd: [esp] eax, +4 ret, +8 frametime, +0C cmd
			call strip_menu_attack
			add esp, 4
			pop eax
			ret 8
		}
	}

	void hook_family_create_move( ) {
		constexpr const char* k_sig = "55 8B EC C5 FA 10 45 08 8D 55 04 8B";
		if ( g_fam.build != FamilyBuild::fh25 ) return;
		void* target = pattern::find( g_csgo.m_family_dll, k_sig ).as< void* >( );
		// the original pointer is stored before the hook goes live (the stub reads it)
		if ( !target || MH_CreateHook( target, ( void* )&hk_family_create_move, &g_cm_original ) != MH_OK
			|| MH_EnableHook( target ) != MH_OK )
			addon_log( "CreateMove hook failed (menu clicks may still fire)" );
	}

	void addon_log( const std::string& s ) {
		if ( g_csgo.m_cvar ) {
			g_csgo.m_cvar->ConsoleColorPrintf( Color( 112, 128, 144 ), "[addon] " );
			g_csgo.m_cvar->ConsoleColorPrintf( Color( 220, 220, 225 ), "%s\n", s.c_str( ) );
		}
	}
}

void detours::init( ) {
	install_crash_log( );

	if ( MH_Initialize( ) != MH_OK ) {
		addon_log( "MinHook failed to initialise" );
		return;
	}

	// the helper's own hooks (paint, window procedure) and its data
	helper::init( );

	// familyhook hooks, through the detected build's patterns (family.h)
	if ( g_fam.build == FamilyBuild::unknown ) {
		addon_log( "familyhook build unknown: the helper cannot drive (no StartMove / DoMove)" );
		return;
	}
	if ( !originals::FamilyStartMove.Create( pattern::find( g_csgo.m_family_dll, g_fam.sig_start_move ), &FamilyStartMove ) )
		addon_log( "StartMove hook failed" );
	if ( !originals::FamilyDoMove.Create( pattern::find( g_csgo.m_family_dll, g_fam.sig_do_move ), &FamilyDoMove ) )
		addon_log( "DoMove hook failed" );
	if ( !originals::FamilyLockCursor.Create( pattern::find( g_csgo.m_family_dll, g_fam.sig_lock_cursor ), &FamilyLockCursor ) )
		addon_log( "LockCursor hook failed (no cursor in the helper menu)" );
	hook_family_create_move( );

	// weapon icons for familyhook 2024's ESP "icon" mode
	weapon_icons::init( );

	// automatic grenade release (rides the same StartMove / DoMove hooks)
	agr::init( );

	// the familyhook 25 addon's features (extras tab): its own hooks
	ext::init( );
}

void __fastcall detours::FamilyStartMove( void* ecx, char edx, __m128 xmm, CUserCmd* cmd ) {
	static auto original = originals::FamilyStartMove.GetOriginal< void( __thiscall* )( void*, CUserCmd* ) >( );

	// clicks in a menu must not fire the weapon (game input stays on with
	// familyhook's menu up, so the player can still move)
	if ( helper::menu_open( ) || family_menu_open( ) )
		cmd->m_buttons &= ~( IN_ATTACK | IN_ATTACK2 );

	original( ecx, cmd );

	// familyhook rebuilds the buttons from its own copies in places: strip again
	if ( helper::menu_open( ) || family_menu_open( ) )
		cmd->m_buttons &= ~( IN_ATTACK | IN_ATTACK2 );

	g_ctx.m_cmd   = cmd;
	g_ctx.m_local = g_csgo.m_entlist->GetClientEntity< Player* >( g_csgo.m_engine->GetLocalPlayer( ) );

	// buttons + movement for familyhook's strafe angles
	helper::on_start_move( cmd );

	// automatic release: the start_move stage (before engine prediction)
	agr::on_start_move( cmd );

	// extras: adaptive weapon configs
	ext::on_start_move( cmd );
}

void __fastcall detours::FamilyDoMove( void* ecx, void* edx ) {
	static auto original = originals::FamilyDoMove.GetOriginal< decltype( &FamilyDoMove ) >( );
	original( ecx, edx );

	// throw bookkeeping + silent aim, after familyhook's anti-aim
	if ( g_ctx.m_cmd && g_ctx.m_local )
		helper::on_do_move( g_ctx.m_cmd );

	// automatic release: anti_aim / finish_move stages (prediction has run),
	// the send override written last (EndMove leaves it alone)
	if ( g_ctx.m_cmd )
		agr::on_do_move( g_ctx.m_cmd );

	// extras: gk anti-aim send overrides (delay-move flick, twist lag)
	ext::on_do_move( );

	// last word on the buttons: no shots from clicks in a menu
	if ( g_ctx.m_cmd && ( helper::menu_open( ) || family_menu_open( ) ) )
		g_ctx.m_cmd->m_buttons &= ~( IN_ATTACK | IN_ATTACK2 );
}

void __fastcall detours::FamilyLockCursor( void* ecx, void* edx ) {
	static auto original = originals::FamilyLockCursor.GetOriginal< void( __thiscall* )( void* ) >( );

	// the helper's overlay menu needs the cursor free and the game deaf
	if ( helper::menu_open( ) ) {
		g_csgo.m_surface->UnlockCursor( );
		g_csgo.m_input_system->EnableInput( false );
		return;
	}

	// familyhook's menu open (the byte its LockCursor tests): the cursor is
	// free but the game keeps its input, so the player can move with the menu
	// up -- except on the skins tab, out of game and while dead (the old
	// addon's behaviour)
	if ( family_menu_open( ) ) {
		g_csgo.m_surface->UnlockCursor( );
		Player* local = g_csgo.m_engine->IsInGame( ) ? g_csgo.m_entlist->GetClientEntity< Player* >( g_csgo.m_engine->GetLocalPlayer( ) ) : nullptr;
		void* tab = g_csgo.MainForm ? *( void** )( g_csgo.MainForm.as< uintptr_t >( ) + g_fam.form_active_tab ) : nullptr;
		const bool skins = tab && g_menu.title_of( tab ) == "skins";
		// typing into a text box: the keyboard is the box's, not the player's
		const bool typing = nm::available( ) && nm::typing( );
		g_csgo.m_input_system->EnableInput( !( skins || typing || !local || !local->alive( ) ) );

		// movement keys stay live, the mouse buttons do not: a click in the
		// menu also reaches the game's +attack / +attack2 binds, which are
		// released here while a mouse button is held (the weapon's own
		// prediction reads them, not only the command)
		// every frame while held: the bind's key-down can land after a single release
		const bool held = ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) || ( GetAsyncKeyState( VK_RBUTTON ) & 0x8000 );
		if ( held && g_csgo.m_engine )
			g_csgo.m_engine->ExecuteClientCmd( "-attack; -attack2" );
		return;
	}
	original( ecx );
	g_csgo.m_input_system->EnableInput( true );
}
