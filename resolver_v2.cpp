// =============================================================================
// resolver_v2.cpp — spec C1–C9 unified resolver
// language: C++17, target: Windows 11 / CSGO 2018, MSVC
// PATCHED per correction spec — six bugs fixed:
//   1. MatchMoveBin: uses GetFirstSequenceAnimTag, not client foot yaw
//   2. DetectMoveStart: origin-delta ServerVelocity, not state->m_vecVelocity
//   3. Priority 3: ApproachAngle(lby, trusted_foot, dt*100), not snap
//   4. Priority 4: ApproachAngle(lby, trusted_foot, dt*(30+20*walkToRun))
//   5. OnShotResult: never flip on layer6 or LBY measurements
//   6. SetupBonesForShot: pose 0..1, record simtime, backup actual abs angles
// =============================================================================
#include "includes.h"
#include "resolver_v2.h"
#include "animstate_verify.h"
#include "animfix_v2.h"

Resolver g_resolver;

// =============================================================================
// helper: get anim tag cycle from a sequence by tag enum
// walks mstudioseqdesc_t::GetAnimTag() — the cheat's existing anim tag data
// =============================================================================
static float GetFirstSequenceAnimTag(Player* player, int sequence, int tag_enum)
{
    if (!player) return 0.f;
    CStudioHdr* hdr = player->GetModelPtr();
    if (!hdr || !hdr->m_pStudioHdr) return 0.f;
    studiohdr_t* shdr = hdr->m_pStudioHdr;
    if (sequence < 0 || sequence >= shdr->m_num_local_seq) return 0.f;
    mstudioseqdesc_t* seq = (mstudioseqdesc_t*)(((byte*)shdr) + shdr->m_local_seq_index) + sequence;
    if (!seq) return 0.f;
    for (int i = 0; i < seq->num_anim_tags; ++i)
    {
        studioanimtag_t* at = seq->GetAnimTag(i);
        if (at && at->tag == tag_enum)
            return at->cycle;
    }
    return -1.f; // sentinel: no tag found — caller must check
}

// =============================================================================
// ResolverState
// =============================================================================
void ResolverState::Reset()
{
    *this = ResolverState{};
}

void ResolverState::RecordShot(float resolved_yaw, float eye_yaw, int side, ResolveMode mode, int tick)
{
    int idx = m_shot_head % MAX_SHOTS;
    m_shots[idx] = { resolved_yaw, eye_yaw, side, mode, tick, false };
    m_shot_head++;
    m_shot_count = std::min(m_shot_count + 1, MAX_SHOTS);
}

// =============================================================================
// Resolver — helpers
// =============================================================================
void Resolver::Reset(int index)
{
    if (index >= 1 && index <= 64)
        m_players[index].Reset();
}

void Resolver::ResetAll()
{
    for (int i = 0; i < 65; ++i)
        m_players[i].Reset();
}

bool Resolver::IsMoving(CCSGOPlayerAnimState* state)
{
    return state && state->m_flVelocityLengthXY > CSGO_ANIM_VELOCITY_MOVING_GATE && state->m_bOnGround;
}

// A1: calc max desync delta from aim matrix width
float Resolver::CalcMaxDelta(CCSGOPlayerAnimState* state)
{
    if (!state) return 58.f;
    float width = CalcAimMatrixWidthRange(state);
    float yaw_max = fabsf(state->m_flAimYawMax * width);
    float yaw_min = fabsf(state->m_flAimYawMin * width);
    return std::max(yaw_max, yaw_min);
}

int Resolver::DetermineSide(float eye_yaw, float foot_yaw)
{
    float delta = AngleDiff(eye_yaw, foot_yaw);
    return (delta >= 0.f) ? 1 : -1;
}

// =============================================================================
// CORRECTED: static velocity helpers — origin delta, not state->m_vecVelocity
// =============================================================================

// same convention as CCSGOPlayerAnimState: atan2(-vy, -vx), degrees, [0, 360)
float Resolver::VelocityYaw(const vec3_t& vel)
{
    float yaw = atan2f(-vel.y, -vel.x) * (180.f / 3.14159265f);
    if (yaw < 0.f)
        yaw += 360.f;
    return yaw;
}

// velocity the server actually integrated. networked m_vecVelocity lies during choke.
vec3_t Resolver::ServerVelocity(Player* player, float simtime, float prev_simtime, const vec3_t& prev_origin)
{
    float dt = simtime - prev_simtime;
    if (dt < g_csgo.m_globals->m_interval * 0.5f)
        dt = g_csgo.m_globals->m_interval;
    vec3_t origin = player->m_vecOrigin();
    return (origin - prev_origin) / dt;
}

// =============================================================================
// CORRECTED: MatchMoveBin — uses GetFirstSequenceAnimTag per-bin cycle matching
// returns the bin index, or -1. does NOT look at foot yaw.
// =============================================================================
int Resolver::MatchMoveBin(Player* player, int sequence, float cycle, float playback_rate)
{
    int best = -1;
    int best_err = 2; // milli-cycle. 1 is the real tolerance; 0 is exact.
    for (int i = 0; i < 8; ++i)
    {
        float tag = GetFirstSequenceAnimTag(player, sequence, ANIMTAG_STARTCYCLE_N + i);
        if (tag < 0.f) continue; // no tag for this bin — skip
        float pred = ClampCycle(tag + playback_rate);
        int err = abs((int)(pred * 1000.f) - (int)(cycle * 1000.f));
        if (err <= best_err)
        {
            best_err = err;
            best = i;
        }
    }
    return best;
}

// =============================================================================
// CORRECTED: DetectMoveStart — origin-delta velocity, MatchMoveBin, weapon
// layer suppression, on_ground check
// =============================================================================
bool Resolver::DetectMoveStart(Player* player, ResolverState& st,
                               C_AnimationLayer* layers, CCSGOPlayerAnimState* state)
{
    if (!layers || !state || !player)
        return false;

    const auto& layer6 = layers[ANIMATION_LAYER_MOVEMENT_MOVE];
    bool just_started = st.m_prev_layer6_weight < 0.01f && layer6.m_weight > 0.01f;

    st.m_prev_layer6_weight = layer6.m_weight;
    st.m_prev_layer6_cycle  = layer6.m_cycle;
    st.m_prev_layer6_seq    = layer6.m_sequence;

    if (!just_started)
        return false;
    if (!state->m_bOnGround)
        return false;

    // weapon layer eating the move layer: do not guess
    const auto& wpn = layers[ANIMATION_LAYER_WEAPON_ACTION];
    if (wpn.m_weight > 0.9f)
        return false;

    float simtime = player->m_flSimulationTime();
    vec3_t vel = ServerVelocity(player, simtime, st.m_prev_simtime, st.m_prev_origin);
    float speed = vel.length_2d();
    if (speed < 1.0f)
        return false;

    int bin = MatchMoveBin(player, layer6.m_sequence, layer6.m_cycle, layer6.m_playback_rate);
    if (bin < 0)
        return false;

    // moveYawIdeal = AngleDiff(rawVelocityYaw, footYaw)
    // so footYaw = AngleNormalize(raw - bin)
    float raw = VelocityYaw(vel);
    float foot = AngleNormalize(raw - k_move_bins[bin]);

    st.m_resolved_foot_yaw = foot;
    st.m_trusted_foot      = foot;
    st.m_has_trusted       = true;
    st.m_resolved_side     = DetermineSide(state->m_flEyeYaw, foot);
    st.m_trusted_side      = st.m_resolved_side;
    st.m_trusted_time      = simtime;
    st.m_move_side_time    = simtime;
    st.m_mode              = ResolveMode::MOVESTART_LAYER6;
    st.m_valid             = true;
    return true;
}

// =============================================================================
// C6: LBY update detection via layer 3 (ANIMATION_LAYER_ADJUST)
// weight transitions 0 → >0 means LBY just realigned
// =============================================================================
bool Resolver::DetectLBYUpdate(Player* player, ResolverState& st, C_AnimationLayer* layers)
{
    if (!layers) return false;

    const auto& layer3 = layers[ANIMATION_LAYER_ADJUST];

    bool weight_started = st.m_prev_layer3_weight < 0.01f && layer3.m_weight > 0.01f;
    bool cycle_valid = layer3.m_cycle > st.m_prev_layer3_cycle
        && (layer3.m_cycle - st.m_prev_layer3_cycle) < 0.5f;
    bool seq_valid = layer3.m_sequence > 0;

    st.m_prev_layer3_weight = layer3.m_weight;
    st.m_prev_layer3_cycle  = layer3.m_cycle;
    st.m_prev_layer3_seq    = layer3.m_sequence;

    if (weight_started && cycle_valid && seq_valid)
    {
        st.m_lby_update_time = g_csgo.m_globals->m_curtime;

        float lby = player->m_flLowerBodyYawTarget();
        st.m_resolved_foot_yaw = lby;
        st.m_trusted_foot      = lby;
        st.m_has_trusted       = true;

        CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
        if (state)
        {
            st.m_resolved_side = DetermineSide(state->m_flEyeYaw, lby);
            st.m_trusted_side = st.m_resolved_side;
            st.m_trusted_time = g_csgo.m_globals->m_curtime;
            st.m_last_lby_time = st.m_trusted_time;
        }

        st.m_mode = ResolveMode::LBY_UPDATE;
        st.m_valid = true;
        return true;
    }

    return false;
}

// =============================================================================
// C4: batch record detection
// simtime jump > tickinterval * 1.5 means chokedticks > 1
// the networked eye yaw is from the LAST choked command, not the first
// server animated with the FIRST command's yaw → eye is fake
// =============================================================================
bool Resolver::DetectBatchRecord(Player* player, ResolverState& st)
{
    float simtime = player->m_flSimulationTime();
    float interval = g_csgo.m_globals->m_interval;

    if (st.m_prev_simtime > 0.f)
    {
        float dt = simtime - st.m_prev_simtime;
        if (dt > 0.f)
        {
            st.m_choked_ticks = (int)(dt / interval + 0.5f);
        }
    }

    // NOTE: m_prev_simtime and m_prev_origin are stored at the bottom of OnPlayerUpdate

    // batch = more than 1 tick choked
    return st.m_choked_ticks > 1;
}

// =============================================================================
// freestand: trace left/right from local to enemy, pick side with more cover
// returns +1 (right desync) or -1 (left desync)
// =============================================================================
float Resolver::FreestandSide(Player* player)
{
    if (!player || !g_ctx.m_local) return 1.f;

    vec3_t local_pos = g_ctx.m_local->GetShootPosition();
    vec3_t enemy_pos = player->GetShootPosition();

    // direction from local to enemy
    vec3_t delta = enemy_pos - local_pos;
    float to_enemy_yaw = atan2f(delta.y, delta.x) * (180.f / 3.14159265f);

    // trace perpendicular left and right from enemy
    float left_yaw  = AngleNormalize(to_enemy_yaw - 90.f);
    float right_yaw = AngleNormalize(to_enemy_yaw + 90.f);

    auto trace_dist = [&](float yaw) -> float {
        vec3_t dir;
        math::AngleVectors(ang_t(0, yaw, 0), &dir);
        constexpr float MAX_DIST = 64.f;
        constexpr float STEP = 4.f;
        float dist = 0.f;
        for (float d = STEP; d <= MAX_DIST; d += STEP)
        {
            vec3_t pos = enemy_pos + dir * d;
            int contents = g_csgo.m_engine_trace->GetPointContents(pos, MASK_SHOT_HULL);
            if (contents & MASK_SHOT_HULL)
                break;
            dist = d;
        }
        return dist;
    };

    float left_dist  = trace_dist(left_yaw);
    float right_dist = trace_dist(right_yaw);

    // enemy hides behind the closer wall → their body is pushed to the open side
    return (left_dist < right_dist) ? -1.f : 1.f;
}

// =============================================================================
// OnPlayerUpdate — main resolver tick, runs C9 priority chain
// CORRECTED: P3/P4 use ApproachAngle + trusted_foot tracking
//            LBY netvar change detection (>1° delta)
// =============================================================================
void Resolver::OnPlayerUpdate(Player* player, int index)
{
    if (!player || index < 1 || index > 64)
        return;

    auto& st = m_players[index];

    CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
    C_AnimationLayer* layers = player->m_AnimOverlay();

    if (!state || !layers)
        return;

    // compute current max desync
    st.m_max_delta = CalcMaxDelta(state);

    // save previous state
    st.m_was_moving = IsMoving(state);
    st.m_on_ground = state->m_bOnGround;
    st.m_last_eye_yaw = state->m_flEyeYaw;
    st.m_last_lby = player->m_flLowerBodyYawTarget();

    // detect batch (C4) — always run, used by priority chain
    bool is_batch = DetectBatchRecord(player, st);

    // ---- C9 PRIORITY CHAIN ----

    // P1: layer 6 move-start (C5)
    if (DetectMoveStart(player, st, layers, state))
    {
        st.m_prev_simtime = player->m_flSimulationTime();
        st.m_prev_origin  = player->m_vecOrigin();
        st.m_last_lby_prev = player->m_flLowerBodyYawTarget();
        return;
    }

    // P2: LBY just updated (C6, layer 3)
    if (DetectLBYUpdate(player, st, layers))
    {
        st.m_prev_simtime = player->m_flSimulationTime();
        st.m_prev_origin  = player->m_vecOrigin();
        st.m_last_lby_prev = player->m_flLowerBodyYawTarget();
        return;
    }

    // ---- CORRECTED: merged P3/P4 with trusted foot approach ----
    float eye = state->m_flEyeYaw;
    float lby = player->m_flLowerBodyYawTarget();
    float dt  = g_csgo.m_globals->m_interval * std::max(st.m_choked_ticks, 1);
    bool moving = IsMoving(state);
    float age = g_csgo.m_globals->m_curtime - st.m_trusted_time;

    // LBY netvar moved and layer 3 did not catch it
    if (fabsf(AngleDiff(lby, st.m_last_lby_prev)) > 1.f)
    {
        st.m_resolved_foot_yaw = lby;
        st.m_trusted_foot = lby;
        st.m_has_trusted  = true;
        st.m_resolved_side = DetermineSide(eye, lby);
        st.m_trusted_side = st.m_resolved_side;
        st.m_trusted_time = g_csgo.m_globals->m_curtime;
        st.m_last_lby_time = st.m_trusted_time;
        st.m_mode = ResolveMode::LBY_UPDATE;
        st.m_valid = true;
        st.m_last_lby_prev = lby;
        st.m_prev_simtime = player->m_flSimulationTime();
        st.m_prev_origin  = player->m_vecOrigin();
        return;
    }
    st.m_last_lby_prev = lby;

    // P3: standing with trusted foot — approach, don't snap
    if (!moving && st.m_has_trusted)
    {
        // server: ApproachAngle(LBY, foot, dt * 100). do not snap back to the extreme.
        float foot = ApproachAngle(lby, st.m_trusted_foot, dt * 100.f);
        st.m_trusted_foot = foot;
        st.m_resolved_foot_yaw = foot;
        st.m_resolved_side = st.m_trusted_side;
        st.m_mode = ResolveMode::STAND_TRUSTED;
        st.m_valid = true;
        st.m_prev_simtime = player->m_flSimulationTime();
        st.m_prev_origin  = player->m_vecOrigin();
        return;
    }

    // P4: moving with trusted side — feet approach LBY at move rate
    if (moving && st.m_trusted_side != 0 && age < 0.5f)
    {
        // LBY while moving is the simulated eye (the real), written inside the
        // moving branch. feet have not arrived there yet.
        float rate = dt * (30.f + 20.f * state->m_flWalkToRunTransition);
        float foot = ApproachAngle(lby, st.m_trusted_foot, rate);
        st.m_trusted_foot = foot;
        st.m_resolved_foot_yaw = foot;
        st.m_resolved_side = st.m_trusted_side;
        st.m_mode = ResolveMode::MOVE_TRUSTED;
        st.m_valid = true;
        st.m_prev_simtime = player->m_flSimulationTime();
        st.m_prev_origin  = player->m_vecOrigin();
        return;
    }

    // P5: batch record — hold last measured foot, don't freestand off fake eye
    if (is_batch && st.m_has_trusted)
    {
        // networked eye is the last cmd. server animated the first.
        // holding the last measured foot beats freestanding off the fake eye.
        st.m_resolved_foot_yaw = st.m_trusted_foot;
        st.m_resolved_side = st.m_trusted_side;
        st.m_mode = ResolveMode::BATCH_RECORD;
        st.m_valid = true;
        st.m_prev_simtime = player->m_flSimulationTime();
        st.m_prev_origin  = player->m_vecOrigin();
        return;
    }

    // P6: brute sign flip (C7) — last resort
    // only sign is free, magnitude is always maxDelta
    {
        float side = (st.m_brute_side != 0) ? (float)st.m_brute_side : FreestandSide(player);

        st.m_resolved_side = (int)side;
        st.m_resolved_foot_yaw = AngleNormalize(eye - side * st.m_max_delta);
        st.m_mode = ResolveMode::BRUTE_SIGN;
        st.m_valid = true;
    }

    // ---- store origin for next tick's ServerVelocity ----
    st.m_prev_simtime = player->m_flSimulationTime();
    st.m_prev_origin  = player->m_vecOrigin();
}

// =============================================================================
// GetResolvedYaw — return the resolved foot yaw for aimbot
// =============================================================================
float Resolver::GetResolvedYaw(Player* player, int index)
{
    if (!player || index < 1 || index > 64)
        return player ? player->m_angEyeAngles().y : 0.f;

    auto& st = m_players[index];
    if (st.m_valid)
        return st.m_resolved_foot_yaw;

    // no valid resolution — return eye yaw
    return player->m_angEyeAngles().y;
}

// =============================================================================
// Shot tracking
// =============================================================================
void Resolver::OnShotFired(int index, float resolved_yaw, float eye_yaw, int side, ResolveMode mode)
{
    if (index < 1 || index > 64) return;
    int tick = g_csgo.m_globals ? g_csgo.m_globals->m_tick_count : 0;
    m_players[index].RecordShot(resolved_yaw, eye_yaw, side, mode, tick);
}

// CORRECTED: never flip on layer 6 or LBY measurements — those are ground truth.
// a miss there is spread or the wrong tick, not the wrong side.
void Resolver::OnShotResult(int index, bool hit, ResolveMode mode)
{
    if (index < 1 || index > 64) return;
    auto& st = m_players[index];

    if (hit)
    {
        st.m_missed_shots = 0;
        st.m_trusted_time = g_csgo.m_globals->m_curtime;
        return;
    }

    // layer 6 and LBY are measurements. flipping them on a miss
    // throws away the only correct sample you had.
    if (mode == ResolveMode::MOVESTART_LAYER6 || mode == ResolveMode::LBY_UPDATE)
        return;

    st.m_brute_side = -st.m_brute_side;
    if (st.m_brute_side == 0)
        st.m_brute_side = 1;

    if (mode == ResolveMode::BRUTE_SIGN || mode == ResolveMode::BATCH_RECORD)
        st.m_trusted_side = st.m_brute_side;
}

// =============================================================================
// CORRECTED: C8 bone setup for shot
// - pose param body_yaw: 0..1, not -1..1 (0.5 = zero degrees, range -60..60)
// - use record_simtime, not m_curtime
// - backup and restore actual abs angles via GetAbsAngles().y
// =============================================================================
bool Resolver::SetupBonesForShot(Player* player, int index, float record_simtime, matrix3x4_t* out_matrix)
{
    if (!player || index < 1 || index > 64 || !out_matrix)
        return false;

    auto& st = m_players[index];
    if (!st.m_valid)
        return false;

    CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
    C_AnimationLayer* layers = player->m_AnimOverlay();
    if (!state || !layers)
        return false;

    // ---- backup ----
    animstate_backup_t as_backup;
    as_backup.Store(state);

    layer_backup_t ly_backup;
    ly_backup.Store(player);

    // backup actual abs angles before override
    float backed_abs_yaw = player->GetAbsAngles().y;

    // ---- override foot yaw to resolved value ----
    state->m_flFootYaw = st.m_resolved_foot_yaw;
    state->m_flFootYawLast = st.m_resolved_foot_yaw;

    // ---- SetAbsAngles to resolved foot yaw ----
    ang_t resolved_angles(0.f, st.m_resolved_foot_yaw, 0.f);
    using SetAbsAngles_t = void(__thiscall*)(void*, const ang_t&);
    reinterpret_cast<SetAbsAngles_t>((uintptr_t)g_csgo.SetAbsAngles)(player, resolved_angles);

    // ---- CORRECTED: pose param body_yaw is 0..1, 0.5 = zero degrees, range -60..60 ----
    float aim = AngleDiff(state->m_flEyeYaw, st.m_resolved_foot_yaw);
    float* pose_params = player->m_flPoseParameter();
    if (pose_params)
        pose_params[POSE_BODY_YAW] = std::clamp(aim / 120.f + 0.5f, 0.f, 1.f);

    // ---- invalidate bone cache ----
    using InvalidateBoneCache_t = void(__thiscall*)(void*);
    reinterpret_cast<InvalidateBoneCache_t>((uintptr_t)g_csgo.InvalidateBoneCache)(player);

    // ---- CORRECTED: SetupBones with record simtime, not m_curtime ----
    bool result = player->SetupBones(out_matrix, 128,
        BONE_USED_BY_HITBOX | BONE_USED_BY_ATTACHMENT,
        record_simtime);

    // ---- restore everything ----
    as_backup.Restore(state);
    ly_backup.Restore(player);

    // CORRECTED: restore the angles you backed up, not the eye yaw
    ang_t backed(0.f, backed_abs_yaw, 0.f);
    reinterpret_cast<SetAbsAngles_t>((uintptr_t)g_csgo.SetAbsAngles)(player, backed);

    reinterpret_cast<InvalidateBoneCache_t>((uintptr_t)g_csgo.InvalidateBoneCache)(player);

    return result;
}
