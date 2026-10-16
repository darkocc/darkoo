#pragma once

#include <vector>
#include <limits>

class AdaptiveAngle
{
public:
    float m_yaw;
    float m_dist;
    float m_range;

    __forceinline AdaptiveAngle( float yaw, float range = 32.f )
    {
        m_yaw = math::NormalizedAngle( yaw );
        m_dist = 0.f;
        m_range = range;
    }
};

enum AntiAimMode : size_t
{
    STAND = 1,
    WALK,
    AIR,
};

enum PeekResult_t : char
{
    PEEK_BRO = -1,
    PEEK_UNCHOKE = 0,
    PEEK_FORCE = 1,
};

struct anti_aim_ctx
{
    int      m_anti_aim_mode;            // +0x00
    void*    m_config_ptr;         // +0x04
    void*    m_tick_record_ptr;         // +0x08
    float    m_lby_timer;          // +0x0C  floor(curtime/interval)*interval ?PRECOMPUTED by game
    float    m_real_viewangles;           // +0x10  cmd->viewangle_yaw
    float    m_max_desync;         // +0x18
    int      m_break_state;        // +0x20  0=idle, 1=breaking
    float    m_server_lby_timer;   // +0x24  ctx->server_lby_timer (LBY reference from prev tick record)
    int      m_override_flag;      // +0x30  bypass straight to FinalizeLBY
    float    m_stored_break;       // +0x3C
    float    m_real_body_yaw;      // +0x58
    float    m_abs_yaw;            // +0x5C  current foot yaw (abs yaw)
    float    m_final_yaw;          // +0x74
    int      m_break_phase;        // +0x7C  0=backwards 1=sell 2=?0
    float    m_target_body;       // +0x28  computed body yaw destination

    static anti_aim_ctx get_variables( uintptr_t inst )
    {
        anti_aim_ctx v{ };
        if ( !inst )
            return v;

        v.m_anti_aim_mode     = *reinterpret_cast<int*>    ( inst + 0x00 );
        v.m_config_ptr        = *reinterpret_cast<void**>  ( inst + 0x04 );
        v.m_tick_record_ptr   = *reinterpret_cast<void**>  ( inst + 0x08 );
        v.m_lby_timer         = *reinterpret_cast<float*>  ( inst + 0x0C ); // might be curtime
        v.m_real_viewangles   = *reinterpret_cast<float*>  ( inst + 0x10 );
        v.m_max_desync        = *reinterpret_cast<float*>  ( inst + 0x18 );
        v.m_break_state       = *reinterpret_cast<int*>    ( inst + 0x20 );
        v.m_server_lby_timer  = *reinterpret_cast<float*>  ( inst + 0x24 );
        v.m_override_flag     = *reinterpret_cast<int*>    ( inst + 0x30 );
        v.m_stored_break      = *reinterpret_cast<float*>  ( inst + 0x3C );
        v.m_real_body_yaw     = *reinterpret_cast<float*>  ( inst + 0x58 );
        v.m_abs_yaw           = *reinterpret_cast<float*>  ( inst + 0x5C );
        v.m_final_yaw         = *reinterpret_cast<float*>  ( inst + 0x74 );
        v.m_target_body       = *reinterpret_cast<float*>  ( inst + 0x28 );
        v.m_break_phase       = *reinterpret_cast<int*>    ( inst + 0x7C );
        return v;
    }
};

struct tick_record_ctx
{
    float    m_abs_yaw;         // +0x620
    float    m_server_lby;      // +0x6B4 
    float    m_curtime;         // +0x6B8
    float    stored_lby_yaw;    // +0x778 
    int      m_lby_counter;     // +0x788
    uint8_t  m_lby_dirty;       // +0x791
    int      m_anti_aim_mode;   // +0x794

    static tick_record_ctx get_variables( uintptr_t rec )
    {
        tick_record_ctx v{ };
        if ( !rec ) 
            return v;

        v.m_abs_yaw         = *reinterpret_cast<float*>   ( rec + 0x620 );
        v.m_server_lby      = *reinterpret_cast<float*>   ( rec + 0x6B4 );
        v.m_curtime         = *reinterpret_cast<float*>   ( rec + 0x6B8 ); // might be the timer instead
        v.stored_lby_yaw    = *reinterpret_cast<float*>   ( rec + 0x778 );
        v.m_lby_counter     = *reinterpret_cast<int*>     ( rec + 0x788 );
        v.m_lby_dirty       = *reinterpret_cast<uint8_t*> ( rec + 0x791 );
        v.m_anti_aim_mode   = *reinterpret_cast<int*>     ( rec + 0x794 );
        return v;
    }
};

class HVH {
public:
    int  m_peek_ticks = 0;
    int  m_mode = STAND;
    float m_lastmove_yaw = 0.f;
    float m_real_yaw = 0.f;
    bool  m_lastmove_valid = false;
    bool m_stand_cache_valid = false;   // STAND AA 캐시 ?�효???�래�?
    bool  in_twist = false;
    bool  force_fl = false;
    bool  m_lby_timing_broken = false;
    bool  m_standing = false;
    bool  m_fakewalking = false;
    bool  m_stand_flick = false;
    bool  m_stand_flick_pending = false;
    float m_body_pred = 0.f;
    float m_enemy_body_pred = 0.f;
    bool  m_fakewalk_lby_stop = false;
    float m_stand_cmd_yaw = 0.f;
    float m_sell_start = 0.f;
    int   m_hide_angle = 0;
    int   m_override_flag = 0;
    int   m_break_phase = 0;
    PeekResult_t m_peek_result = PEEK_BRO;
    Player* m_best_target = nullptr;
    float m_last_layer_speed = 0.f;
    bool  m_was_walk = false;
    bool  m_was_in_air = false;
    int   m_air_ticks = 0;
    int   m_air_move_side = 0;
    int   m_desync_ticks_landed = 0;
    bool  m_desync_jitter_valid = false;
    float m_desync_jitter_yaw = 0.f;
    float m_landing_block_until = 0.f;
    float m_standing_aa_block_until = 0.f;
    float m_stand_cached_yaw = 0.f;
    float m_stand_cached_body = 0.f;
    int   m_stand_cached_break = 0;
    int   m_stand_cached_hide = 0;
    int   m_stand_cached_phase = 0;
    int   m_air_yaw_index = 0;
    float m_air_yaw_next_switch = 0.f;
    bool  m_update_command[150]{};
    float normalize_yaw( float yaw );

    // fits a wanted fake body yaw into the animstate's real aim matrix
    // (source: csgo_playeranimstate.cpp:2332-2349, per-side bounds). Returns the
    // yaw unchanged when it is already inside the range the feet may lead by.
    float fit_yaw_to_aim_matrix( float eye_yaw, float yaw );
    // aim-matrix band (reference anti-aim): width and the per-side bounds
    // the feet may lead the eyes by - csgo_playeranimstate.cpp:2332-2340.
    struct Band { float pos = 0.f, neg = 0.f, width = 1.f; };

    Band BuildBand( CCSGOPlayerAnimState* state );
    void ClampVelocity( CUserCmd* cmd, Player* me, float target );
    void ResetSessionState();
    void ClampMovementSpeed( CUserCmd* cmd, float speed );
    void ApplySupression( float& yaw );
    void PreventMoveLayer();
    void SpoofMoveLayer();
    void PredictBodyPred();

    bool wants_shoot_bullet();
    void ApplyFlick();
    bool is_shooting_bullet();
    Player* FindBestTarget();
    float DynamicFreestand( Player* target, std::vector<AdaptiveAngle> angles, bool get_worst = false );
    float do_anti_aim(float real_yaw, anti_aim_ctx& aa_ctx, tick_record_ctx& record_ctx, bool on_send_tick);
};








extern HVH g_anti_aim;
