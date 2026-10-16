#pragma once
// =============================================================================
// resolver_v2.h — spec-conformant resolver (C1–C9 priority chain)
// PATCHED per correction spec:
//   - MatchMoveBin uses GetFirstSequenceAnimTag, not foot yaw
//   - DetectMoveStart uses origin-delta velocity, not state->m_vecVelocity
//   - P3 standing: ApproachAngle(lby, trusted_foot, dt*100), not snap
//   - P4 moving: ApproachAngle(lby, trusted_foot, dt*(30+20*walkToRun))
//   - OnShotResult: never flip on layer 6 or LBY measurements
//   - SetupBonesForShot: record simtime, backup abs angles, pose 0..1
// =============================================================================

#include "animstate_verify.h"

// ---- C5: 8-way direction bins (moveYawIdeal values, not foot yaw) ----
static const float k_move_bins[8] = {
    180.f, 135.f, 90.f, 45.f, 0.f, -45.f, -90.f, -135.f
};

// ---- resolver modes (spec C9 naming) ----
enum class ResolveMode : int
{
    NONE = 0,
    MOVESTART_LAYER6,   // C5: layer 6 just activated, direction bin match
    LBY_UPDATE,         // C6: layer 3 weight 0→>0 or LBY netvar change > 1°
    STAND_TRUSTED,      // standing, approach LBY from trusted foot
    MOVE_TRUSTED,       // moving, approach LBY from trusted foot at move rate
    BATCH_RECORD,       // C4: chokedticks > 1, hold last measured foot
    BRUTE_SIGN,         // C7: miss → flip sign, magnitude = maxDelta always
};

// ---- per-shot record for miss tracking ----
struct ShotRecord
{
    float m_resolved_yaw  = 0.f;
    float m_eye_yaw       = 0.f;
    int   m_side          = 0;      // +1 or -1
    ResolveMode m_mode    = ResolveMode::NONE;
    int   m_tick          = 0;
    bool  m_hit           = false;
};

// ---- per-player resolver state ----
struct ResolverState
{
    // --- resolved output ---
    ResolveMode m_mode         = ResolveMode::NONE;
    float m_resolved_foot_yaw  = 0.f;
    int   m_resolved_side      = 0;    // +1 = right desync, -1 = left
    float m_max_delta          = 58.f;
    bool  m_valid              = false;

    // --- trusted tracking ---
    int   m_trusted_side       = 0;
    float m_trusted_time       = 0.f;
    float m_trusted_foot       = 0.f;  // last measured/approached foot yaw
    bool  m_has_trusted        = false; // true once m_trusted_foot has a real sample
    float m_move_side_time     = 0.f;  // when move-start last fired

    // --- LBY netvar tracking ---
    float m_last_lby_prev      = 0.f;  // previous tick's LBY for delta detection
    float m_last_lby_time      = 0.f;  // when LBY last updated

    // --- layer 6 (move-start, C5) ---
    float m_prev_layer6_weight = 0.f;
    float m_prev_layer6_cycle  = 0.f;
    int   m_prev_layer6_seq    = 0;

    // --- layer 3 (LBY update, C6) ---
    float m_prev_layer3_weight = 0.f;
    float m_prev_layer3_cycle  = 0.f;
    int   m_prev_layer3_seq    = 0;
    float m_lby_update_time    = 0.f;

    // --- simtime / choke / origin tracking ---
    float  m_prev_simtime      = 0.f;
    vec3_t m_prev_origin       = vec3_t(0, 0, 0);
    int    m_choked_ticks      = 0;

    // --- state tracking ---
    float m_last_eye_yaw       = 0.f;
    float m_last_lby           = 0.f;
    float m_last_foot_yaw      = 0.f;
    float m_last_move_yaw      = 0.f;
    bool  m_was_moving         = false;
    bool  m_on_ground          = false;

    // --- miss tracking (C7) ---
    static constexpr int MAX_SHOTS = 16;
    ShotRecord m_shots[MAX_SHOTS];
    int   m_shot_head          = 0;
    int   m_shot_count         = 0;
    int   m_missed_shots       = 0;
    int   m_brute_side         = 1;

    void Reset();
    void RecordShot(float resolved_yaw, float eye_yaw, int side, ResolveMode mode, int tick);
};

// ---- main resolver class ----
class Resolver
{
public:
    ResolverState m_players[65];

    // --- hooks ---
    void OnPlayerUpdate(Player* player, int index);
    void OnShotFired(int index, float resolved_yaw, float eye_yaw, int side, ResolveMode mode);
    void OnShotResult(int index, bool hit, ResolveMode mode);
    float GetResolvedYaw(Player* player, int index);
    void Reset(int index);
    void ResetAll();

    // --- C8: bone setup for shot ---
    bool SetupBonesForShot(Player* player, int index, float record_simtime, matrix3x4_t* out_matrix);

private:
    // --- detection ---
    bool DetectMoveStart(Player* player, ResolverState& st, C_AnimationLayer* layers, CCSGOPlayerAnimState* state);
    bool DetectLBYUpdate(Player* player, ResolverState& st, C_AnimationLayer* layers);
    bool DetectBatchRecord(Player* player, ResolverState& st);
    bool IsMoving(CCSGOPlayerAnimState* state);

    // --- helpers ---
    float CalcMaxDelta(CCSGOPlayerAnimState* state);
    static int   MatchMoveBin(Player* player, int sequence, float cycle, float playback_rate);
    static float VelocityYaw(const vec3_t& vel);
    static vec3_t ServerVelocity(Player* player, float simtime, float prev_simtime, const vec3_t& prev_origin);
    float FreestandSide(Player* player);
    int   DetermineSide(float eye_yaw, float foot_yaw);
};

extern Resolver g_resolver;
