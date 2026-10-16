#pragma once
#include "sdk.h"
#include "modules.h"
#include "address.h"
// predeclares.
class CCSGOPlayerAnimState;
class C_CSGameRules;
class IGameEventManager2;
class IViewRenderBeams;


class CHudElement
{
public:
	__forceinline const char* GetName()
	{
		return util::get_method< const char* ( __thiscall* )( void* ) >( this, 12 )( this );
	}
};

class CHud
{
public:
	PAD( 0x1C );
	CUtlVector< CHudElement* > list;

public:
	__forceinline CHudElement* FindElement( hash32_t hash )
	{
		size_t size = list.Count();
		for ( size_t i{}; i < size; ++i )
		{
			if ( FNV1a::get( list[ i ]->GetName() ) == hash )
				return list[ i ];
		}

		return nullptr;
	}
};

class CSGO {
public:
	using MD5_PseudoRandom_t = uint32_t(__thiscall *)(uint32_t);
	using RandomSeed_t = void(__cdecl *)(int);
	using RandomInt_t = int(__cdecl *)(int, int);
	using RandomFloat_t = float(__cdecl *)(float, float);
	using AngleMatrix_t = void(__fastcall *)(const ang_t &, matrix3x4_t &);
	using ServerRankRevealAll_t = bool(__cdecl *)(CCSUsrMsg_ServerRankRevealAll *);
	using IsReady_t = void(__cdecl *)();
	using GetEconItemView_t = C_EconItemView * (__thiscall *)(Weapon *);
	using GetStaticData_t = CEconItemDefinition * (__thiscall *)(C_EconItemView *);
	using ConcatTransforms_t = void(__fastcall *)();
	using GetShotgunSpread_t = void(__stdcall *)(int, int, int, float *, float *);
public:
	bool m_done;

	// module objects.
	PE::Module m_kernel32_dll;
	PE::Module m_user32_dll;
	PE::Module m_shell32_dll;
	PE::Module m_shlwapi_dll;
	PE::Module m_client_dll;
	PE::Module m_engine_dll;
	PE::Module m_vstdlib_dll;
	PE::Module m_tier0_dll;
	PE::Module m_serverbrowser_dll;
	PE::Module m_family_dll;

public:
	CHLClient* m_client;
	ICvar *m_cvar;
	IVEngineClient *m_engine;
	IClientEntityList* m_entlist;
	IEngineTrace* m_engine_trace;
	IInputSystem *m_input_system;
	ISurface *m_surface;
	IPanel *m_panel;
	IEngineVGui *m_engine_vgui;
	IVRenderView *m_render_view;
	CViewRender *m_view_render;
	IVModelRender *m_model_render;
	IMaterialSystem *m_material_system;
	CStudioRenderContext *m_studio_render;
	IVModelInfo *m_model_info;
	IVDebugOverlay *m_debug_overlay;
	ILocalize *m_localize;
	INetworkStringTableContainer *m_networkstringtable;
	IPhysicsSurfaceProps *m_phys_props;
	IGameEventManager2 *m_game_events;
	IViewRenderBeams *m_beams;
	CGlobalVarsBase *m_globals;
	CInput *m_input;
	INetChannel *m_net;
	CHud *m_hud;

	CClientState *m_cl;
	CGame *m_game;
	CRender *m_render;
	IMemAlloc *m_mem_alloc;
	IClientShadowMgr *m_shadow_mgr;
	C_CSGameRules *m_gamerules;
	void *m_radar;
	void *m_hookable_cl;

public:
	// convars.
	ConVar *clear;
	ConVar *toggleconsole;
	ConVar *name;
	ConVar *sv_maxunlag;
	ConVar* cl_csm_shadows;
	ConVar* mat_disable_fancy_blending;
	ConVar* mat_detail_tex;
	ConVar *sv_gravity;
	ConVar *sv_jump_impulse;
	ConVar *sv_enablebunnyhopping;
	ConVar *sv_airaccelerate;
	ConVar *sv_friction;
	ConVar *sv_stopspeed;
	ConVar *cl_interp;
	ConVar *cl_interp_ratio;
	ConVar *cl_updaterate;
	ConVar *cl_cmdrate;
	ConVar *cl_lagcompensation;
	ConVar *mp_teammates_are_enemies;
	ConVar *weapon_debug_spread_show;
	ConVar *net_showfragments;
	ConVar *molotov_throw_detonate_time;
	ConVar *weapon_molotov_maxdetonateslope;
	ConVar *weapon_recoil_scale;
	ConVar *view_recoil_tracking;
	ConVar *cl_fullupdate;
	ConVar *r_DrawSpecificStaticProp;
	ConVar *cl_crosshair_sniper_width;
	ConVar *hud_scaling;
	ConVar *sv_clip_penetration_traces_to_players;
	ConVar *weapon_accuracy_shotgun_spread_patterns;
	ConVar *sv_penetration_type;
	ConVar *ff_damage_reduction_bullets;
	ConVar *ff_damage_bullet_penetration;

public:
	// functions.
	MD5_PseudoRandom_t	     MD5_PseudoRandom;
	Address                  SetAbsAngles;
	Address				     SetAbsOrigin;
	Address                  InvalidateBoneCache;
	Address				     LockStudioHdr;
	RandomSeed_t             RandomSeed;
	RandomInt_t              RandomInt;
	RandomFloat_t            RandomFloat;
	Address	                 SetAbsVelocity;
	AngleMatrix_t            AngleMatrix;
	Address					 DoProceduralFootPlant;
	Address					 ComputeHitboxSurroundingBox;
	Address					 GetSequenceActivity;
	ServerRankRevealAll_t    ServerRankRevealAll;
	Address					 HasC4;
	Address					 InvalidatePhysicsRecursive;
	IsReady_t				 IsReady;
	GetEconItemView_t        GetEconItemView;
	GetStaticData_t          GetStaticData;
	Address					 TEFireBullets;
	GetShotgunSpread_t       GetShotgunSpread;

	size_t BoneAccessor;
	size_t AnimOverlay;
	size_t SpawnTime;
	size_t MostRecentModelBoneCounter;
	size_t LastBoneSetupTime;
	size_t IsLocalPlayer;
	size_t PlayerAnimState;
	size_t studioHdr;

	Address UTIL_TraceLine;
	Address CTraceFilterSimple_vmt;
	Address CTraceFilterSkipTwoEntities_vmt;

	int *m_nPredictionRandomSeed;
	Player *m_pPredictionPlayer;

	Address MainForm;
	Address Config;
	Address Tabs;

public:
	// initialize class.
	void init_family_signatures( );
	bool init( );
};

extern CSGO g_csgo;

namespace game {
	__forceinline float GetClientInterpAmount() {
		return std::max(g_csgo.cl_interp->GetFloat(), g_csgo.cl_interp_ratio->GetFloat() / g_csgo.cl_updaterate->GetFloat());
	}

	__forceinline int TIME_TO_TICKS(float time) {
		return (int)(0.5f + time / g_csgo.m_globals->m_interval);
	}

	__forceinline float TICKS_TO_TIME(int ticks) {
		return g_csgo.m_globals->m_interval * (float)(ticks);
	}



}
