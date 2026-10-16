#pragma once

#define MAX_BEAM_ENTS	10
#define NOISE_DIVISIONS	128

class Beam_t
{
public:
	void* unk_1;
	void* unk_2;
	void* unk_3;
	vec3_t	m_Mins;
	vec3_t	m_Maxs;
	void* m_queryHandleHalo;
	float	m_haloProxySize;
	Beam_t* next;
	int		type;
	int		flags;
	int		numAttachments;
	vec3_t	attachment[ MAX_BEAM_ENTS ];
	vec3_t	delta;
	float	t;
	float	freq;
	float	die;
	float	width;
	float	endWidth;
	float	fadeLength;
	float	amplitude;
	float	life;
	float	r, g, b;
	float	brightness;
	float	speed;
	float	frameRate;
	float	frame;
	int		segments;
	EHANDLE	entity[ MAX_BEAM_ENTS ];
	int		attachmentIndex[ MAX_BEAM_ENTS ];
	int		modelIndex;
	int		haloIndex;
	float	haloScale;
	int		frameCount;
	float	rgNoise[ NOISE_DIVISIONS + 1 ];
	void* trail;
	float	start_radius;
	float	end_radius;
	bool	m_bCalculatedNoise;
	float	m_flHDRColorScale;
};

enum
{
	FBEAM_STARTENTITY = 0x00000001,
	FBEAM_ENDENTITY = 0x00000002,
	FBEAM_FADEIN = 0x00000004,
	FBEAM_FADEOUT = 0x00000008,
	FBEAM_SINENOISE = 0x00000010,
	FBEAM_SOLID = 0x00000020,
	FBEAM_SHADEIN = 0x00000040,
	FBEAM_SHADEOUT = 0x00000080,
	FBEAM_ONLYNOISEONCE = 0x00000100,
	FBEAM_NOTILE = 0x00000200,
	FBEAM_USE_HITBOXES = 0x00000400,
	FBEAM_STARTVISIBLE = 0x00000800,
	FBEAM_ENDVISIBLE = 0x00001000,
	FBEAM_ISACTIVE = 0x00002000,
	FBEAM_FOREVER = 0x00004000,
	FBEAM_HALOBEAM = 0x00008000,
	FBEAM_REVERSED = 0x00010000,
	NUM_BEAM_FLAGS = 17
};

struct BeamInfo_t
{
	int		   m_nType;
	Entity* m_pStartEnt;
	int		   m_nStartAttachment;
	Entity* m_pEndEnt;
	int		   m_nEndAttachment;
	vec3_t	   m_vecStart;
	vec3_t	   m_vecEnd;
	int		   m_nModelIndex;
	const char* m_pszModelName;
	int		   m_nHaloIndex;
	const char* m_pszHaloName;
	float	   m_flHaloScale;
	float	   m_flLife;
	float	   m_flWidth;
	float	   m_flEndWidth;
	float	   m_flFadeLength;
	float	   m_flAmplitude;
	float	   m_flBrightness;
	float	   m_flSpeed;
	int		   m_nStartFrame;
	float	   m_flFrameRate;
	float	   m_flRed;
	float	   m_flGreen;
	float	   m_flBlue;
	bool	   m_bRenderable;
	int		   m_nSegments;
	int		   m_nFlags;
	vec3_t	   m_vecCenter;
	float	   m_flStartRadius;
	float	   m_flEndRadius;

	__forceinline BeamInfo_t()
	{
		m_nType = 0;
		m_nSegments = -1;
		m_pszModelName = 0;
		m_pszHaloName = 0;
		m_nModelIndex = -1;
		m_nHaloIndex = -1;
		m_bRenderable = true;
		m_nFlags = 0;
		m_pStartEnt = nullptr;
		m_pEndEnt = nullptr;
		m_nStartAttachment = 0;
		m_nEndAttachment = 0;
		m_flHaloScale = 0.f;
		m_flLife = 0.f;
		m_flWidth = 0.f;
		m_flEndWidth = 0.f;
		m_flFadeLength = 0.f;
		m_flAmplitude = 0.f;
		m_flBrightness = 255.f;
		m_flSpeed = 0.f;
		m_nStartFrame = 0;
		m_flFrameRate = 0.f;
		m_flRed = 0.f;
		m_flGreen = 0.f;
		m_flBlue = 0.f;
		m_flStartRadius = 0.f;
		m_flEndRadius = 0.f;
	}
};

class IViewRenderBeams
{
public:
	enum indices : size_t
	{
		DRAWBEAM = 4,
		CREATEBEAMPOINTS = 12,
	};

	__forceinline void DrawBeam( Beam_t* beam )
	{
		util::get_method< void( __thiscall* )( void*, Beam_t* ) >( this, DRAWBEAM )( this, beam );
	}

	__forceinline Beam_t* CreateBeamPoints( BeamInfo_t& beam_info )
	{
		return util::get_method< Beam_t* ( __thiscall* )( void*, BeamInfo_t& ) >( this, CREATEBEAMPOINTS )( this, beam_info );
	}
};
