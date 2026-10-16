// =============================================================================
// animfix_v2.cpp — upgraded animation fix with enemy animstate reconstruction
// language: C++17, target: Windows 11 / CSGO 2018, MSVC
//
// LOCAL PLAYER: unchanged from animfix.cpp — rebuild with fake angles,
//   fix pose params, fix layers, spoof velocity, bone rebuild.
//
// ENEMY PLAYERS: NEW — per-tick animstate reconstruction using A1-A5 math,
//   feeds into resolver_v2 for resolved yaw, builds bones via C8 pipeline.
//
// Integration points:
//   - FRAME_NET_UPDATE_POSTDATAUPDATE_START → OnEnemyAnimUpdate for each enemy
//   - resolver_v2.OnPlayerUpdate runs inside OnEnemyAnimUpdate
//   - aimbot calls SetupEnemyBones for hitbox positioning
//   - lagcomp stores reconstructed state via StoreEnemyRecord
// =============================================================================
#include "includes.h"
#include "animfix_v2.h"
#include "resolver_v2.h"
// lagcomp_fix.h and antiaim_menu.h removed — not present in this tree

// AntiAimMenu removed — not part of this build (no GUI form API in this tree)

AnimFix g_animfix{};

// =============================================================================
// animstate_backup_t — unchanged
// =============================================================================
void animstate_backup_t::Store(CCSGOPlayerAnimState* state)
{
    if (!state) return;
    m_eye_yaw = state->m_flEyeYaw;
    m_eye_pitch = state->m_flEyePitch;
    m_foot_yaw = state->m_flFootYaw;
    m_foot_yaw_last = state->m_flFootYawLast;
    m_move_yaw = state->m_flMoveYaw;
    m_move_yaw_ideal = state->m_flMoveYawIdeal;
    m_move_yaw_current_to_ideal = state->m_flMoveYawCurrentToIdeal;
    m_time_to_align_lower = state->m_flTimeToAlignLowerBody;
    m_primary_cycle = state->m_flPrimaryCycle;
    m_move_weight = state->m_flMoveWeight;
    m_move_weight_smoothed = state->m_flMoveWeightSmoothed;
    m_anim_duck_amount = state->m_flAnimDuckAmount;
    m_duck_additional = state->m_flDuckAdditional;
    m_recrouch_weight = state->m_flRecrouchWeight;
    m_velocity = state->m_vecVelocity;
    m_velocity_normalized = state->m_vecVelocityNormalized;
    m_velocity_length_xy = state->m_flVelocityLengthXY;
    m_velocity_length_z = state->m_flVelocityLengthZ;
    m_speed_as_portion_run = state->m_flSpeedAsPortionOfRunTopSpeed;
    m_speed_as_portion_walk = state->m_flSpeedAsPortionOfWalkTopSpeed;
    m_speed_as_portion_crouch = state->m_flSpeedAsPortionOfCrouchTopSpeed;
    m_duration_moving = state->m_flDurationMoving;
    m_duration_still = state->m_flDurationStill;
    m_on_ground = state->m_bOnGround;
    m_landing = state->m_bLanding;
    m_walk_to_run_transition = state->m_flWalkToRunTransition;
    m_landed_this_frame = state->m_bLandedOnGroundThisFrame;
    m_left_ground_this_frame = state->m_bLeftTheGroundThisFrame;
    m_aim_yaw_min = state->m_flAimYawMin;
    m_aim_yaw_max = state->m_flAimYawMax;
    m_aim_pitch_min = state->m_flAimPitchMin;
    m_aim_pitch_max = state->m_flAimPitchMax;
    m_last_update_time = state->m_flLastUpdateTime;
    m_last_update_frame = state->m_iLastUpdateFrame;
    m_last_update_increment = state->m_flLastUpdateIncrement;
}

void animstate_backup_t::Restore(CCSGOPlayerAnimState* state)
{
    if (!state) return;
    state->m_flEyeYaw = m_eye_yaw;
    state->m_flEyePitch = m_eye_pitch;
    state->m_flFootYaw = m_foot_yaw;
    state->m_flFootYawLast = m_foot_yaw_last;
    state->m_flMoveYaw = m_move_yaw;
    state->m_flMoveYawIdeal = m_move_yaw_ideal;
    state->m_flMoveYawCurrentToIdeal = m_move_yaw_current_to_ideal;
    state->m_flTimeToAlignLowerBody = m_time_to_align_lower;
    state->m_flPrimaryCycle = m_primary_cycle;
    state->m_flMoveWeight = m_move_weight;
    state->m_flMoveWeightSmoothed = m_move_weight_smoothed;
    state->m_flAnimDuckAmount = m_anim_duck_amount;
    state->m_flDuckAdditional = m_duck_additional;
    state->m_flRecrouchWeight = m_recrouch_weight;
    state->m_vecVelocity = m_velocity;
    state->m_vecVelocityNormalized = m_velocity_normalized;
    state->m_flVelocityLengthXY = m_velocity_length_xy;
    state->m_flVelocityLengthZ = m_velocity_length_z;
    state->m_flSpeedAsPortionOfRunTopSpeed = m_speed_as_portion_run;
    state->m_flSpeedAsPortionOfWalkTopSpeed = m_speed_as_portion_walk;
    state->m_flSpeedAsPortionOfCrouchTopSpeed = m_speed_as_portion_crouch;
    state->m_flDurationMoving = m_duration_moving;
    state->m_flDurationStill = m_duration_still;
    state->m_bOnGround = m_on_ground;
    state->m_bLanding = m_landing;
    state->m_flWalkToRunTransition = m_walk_to_run_transition;
    state->m_bLandedOnGroundThisFrame = m_landed_this_frame;
    state->m_bLeftTheGroundThisFrame = m_left_ground_this_frame;
    state->m_flAimYawMin = m_aim_yaw_min;
    state->m_flAimYawMax = m_aim_yaw_max;
    state->m_flAimPitchMin = m_aim_pitch_min;
    state->m_flAimPitchMax = m_aim_pitch_max;
    state->m_flLastUpdateTime = m_last_update_time;
    state->m_iLastUpdateFrame = m_last_update_frame;
    state->m_flLastUpdateIncrement = m_last_update_increment;
}

// =============================================================================
// layer_backup_t — unchanged
// =============================================================================
void layer_backup_t::Store(Player* player)
{
    if (!player) return;
    C_AnimationLayer* layers = player->m_AnimOverlay();
    if (!layers) return;

    for (int i = 0; i < ANIMATION_LAYER_COUNT; ++i)
        m_layers[i] = layers[i];

    float* pose = player->m_flPoseParameter();
    if (pose)
    {
        for (int i = 0; i < 24; ++i)
            m_pose_param[i] = pose[i];
    }
}

void layer_backup_t::Restore(Player* player)
{
    if (!player) return;
    C_AnimationLayer* layers = player->m_AnimOverlay();
    if (!layers) return;

    for (int i = 0; i < ANIMATION_LAYER_COUNT; ++i)
        layers[i] = m_layers[i];

    float* pose = player->m_flPoseParameter();
    if (pose)
    {
        for (int i = 0; i < 24; ++i)
            pose[i] = m_pose_param[i];
    }
}

// =============================================================================
// Helpers — unchanged
// =============================================================================
float AnimFix::BodyYawToPoseParam(float body_yaw)
{
    return std::clamp(body_yaw / 60.0f, -1.0f, 1.0f);
}

float AnimFix::PoseParamToBodyYaw(float pose)
{
    return pose * 60.0f;
}

float AnimFix::MoveYawToPoseParam(float move_yaw)
{
    return std::clamp(move_yaw / 180.0f, -1.0f, 1.0f);
}

void AnimFix::SetLayerSequenceWeight(C_AnimationLayer& layer, int seq, float weight, float cycle)
{
    layer.m_sequence = seq;
    layer.m_weight = weight;
    layer.m_cycle = cycle;
    layer.m_playback_rate = 0.f;
}

// =============================================================================
// Init
// =============================================================================
void AnimFix::Init()
{
    m_in_update = false;
    m_should_rebuild = false;
    m_last_simtime = 0.f;
    m_last_tick = 0;
    m_interp_curtime = 0.f;

    // reset enemy records
    for (int i = 0; i < 65; ++i)
        m_enemy_records[i].Reset();
}

// =============================================================================
// RebuildAnimState — local player, inject fake eye angles
// =============================================================================
void AnimFix::RebuildAnimState(Player* player, float eye_yaw, float eye_pitch)
{
    if (!m_cfg.m_rebuild_animstate || !player)
        return;

    CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
    if (!state) return;

    state->m_flEyeYaw = AngleNormalize(eye_yaw);
    state->m_flEyePitch = std::clamp(eye_pitch, -90.f, 90.f);
    ClampFootYaw(state, state->m_flEyeYaw);
}

// =============================================================================
// FixPoseParameters — local player, prevent body_yaw from leaking real angle
// =============================================================================
void AnimFix::FixPoseParameters(Player* player, float real_yaw, float fake_yaw)
{
    if (!m_cfg.m_fix_pose_params || !player) return;

    float* pose = player->m_flPoseParameter();
    if (!pose) return;

    CCSGOPlayerAnimState* state = player->m_PlayerAnimState();

    if (m_cfg.m_fix_body_yaw)
    {
        float body_yaw = 0.f;
        if (state)
        {
            body_yaw = math::AngleDiff(fake_yaw, state->m_flFootYaw);
            body_yaw = std::clamp(body_yaw, -58.f, 58.f);
        }
        pose[POSE_BODY_YAW] = BodyYawToPoseParam(body_yaw);
    }

    if (m_cfg.m_fix_move_yaw && state)
    {
        float move_yaw = VelocityToYaw(player->m_vecVelocity());
        if (move_yaw != 0.f)
            pose[POSE_MOVE_YAW] = MoveYawToPoseParam(move_yaw);
    }

    if (m_cfg.m_fix_speed && state)
    {
        float speed_portion = state->m_flSpeedAsPortionOfWalkTopSpeed;
        pose[POSE_SPEED] = std::clamp(speed_portion, 0.f, 1.f);
    }
}

// =============================================================================
// FixAnimationLayers — local player
// =============================================================================
void AnimFix::FixAnimationLayers(Player* player)
{
    if (!m_cfg.m_fix_layers || !player) return;

    C_AnimationLayer* layers = player->m_AnimOverlay();
    if (!layers) return;

    auto& adjust = layers[ANIMATION_LAYER_ADJUST];
    if (adjust.m_weight > 0.f && adjust.m_sequence > 0)
    {
        if (m_cfg.m_suppress_979)
        {
            if (adjust.m_weight > 0.8f)
                adjust.m_weight = 0.8f;
        }
    }

    auto& move = layers[ANIMATION_LAYER_MOVEMENT_MOVE];
    if (move.m_weight <= 0.f && m_cfg.m_spoof_move_layer)
        move.m_weight = 0.001f;

    auto& alive_loop = layers[ANIMATION_LAYER_ALIVELOOP];
    if (alive_loop.m_sequence == 979 && m_cfg.m_suppress_979)
    {
        alive_loop.m_weight = 0.f;
        alive_loop.m_cycle = 0.f;
    }
}

// =============================================================================
// SpoofVelocityForAnimState — local player, for fake walk
// =============================================================================
void AnimFix::SpoofVelocityForAnimState(Player* player, const vec3_t& spoofed_vel)
{
    if (!m_cfg.m_spoof_velocity || !player) return;

    CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
    if (!state) return;

    state->m_vecVelocity = spoofed_vel;
    state->m_flVelocityLengthXY = spoofed_vel.length_2d();
    state->m_flVelocityLengthZ = spoofed_vel.z;

    if (state->m_flVelocityLengthXY > 0.001f)
        state->m_vecVelocityNormalized = spoofed_vel / state->m_flVelocityLengthXY;
    else
        state->m_vecVelocityNormalized = vec3_t(0, 0, 0);

    const float walk_speed = 135.f;
    const float run_speed = 260.f;
    const float crouch_speed = 88.f;

    state->m_flSpeedAsPortionOfWalkTopSpeed =
        std::clamp(state->m_flVelocityLengthXY / walk_speed, 0.f, 1.f);
    state->m_flSpeedAsPortionOfRunTopSpeed =
        std::clamp(state->m_flVelocityLengthXY / run_speed, 0.f, 1.f);
    state->m_flSpeedAsPortionOfCrouchTopSpeed =
        std::clamp(state->m_flVelocityLengthXY / crouch_speed, 0.f, 1.f);
}

// =============================================================================
// FixJumpFallLayers — local player
// =============================================================================
void AnimFix::FixJumpFallLayers(Player* player)
{
    if (!m_cfg.m_fix_jump_fall || !player) return;

    C_AnimationLayer* layers = player->m_AnimOverlay();
    if (!layers) return;

    bool on_ground = (player->m_fFlags() & FL_ONGROUND) != 0;
    if (on_ground) return;

    auto& jump_fall = layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
    if (m_cfg.m_adaptive_air && jump_fall.m_weight > 0.5f)
        jump_fall.m_weight = 0.5f;
}

// =============================================================================
// FixSimulationTime — local player
// =============================================================================
void AnimFix::FixSimulationTime(Player* player)
{
    if (!m_cfg.m_fix_simtime || !player) return;

    float simtime = player->m_flSimulationTime();
    if (simtime == m_last_simtime) return;

    m_last_simtime = simtime;
}

// =============================================================================
// UpdateClientSideAnimationFix — hook-safe local rebuild
// =============================================================================
void AnimFix::UpdateClientSideAnimationFix(Player* player)
{
    if (!m_cfg.m_update_clientside || !player || !m_cfg.m_enabled) return;

    CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
    if (!state) return;

    if (g_ctx.m_lag > 0 && player == g_ctx.m_local)
        return;

    // m_cmd is stale between CreateMove calls — use the persistent view angles
    float fake_yaw = g_ctx.m_view_angles.y;
    float fake_pitch = g_ctx.m_view_angles.x;

    m_layer_backup.Store(player);
    RebuildAnimState(player, fake_yaw, fake_pitch);
    FixPoseParameters(player, g_ctx.m_view_angles.y, fake_yaw);
    FixAnimationLayers(player);

    if (player == g_ctx.m_local)
        FixJumpFallLayers(player);
}

// =============================================================================
// DoLocalAnimFix — main entry from CreateMove
// =============================================================================
void AnimFix::DoLocalAnimFix(CUserCmd* cmd, bool* send_packet)
{
    ReadMenuConfig();

    if (!m_cfg.m_enabled || !g_ctx.m_local) return;

    Player* local = g_ctx.m_local;
    CCSGOPlayerAnimState* state = local->m_PlayerAnimState();
    if (!state) return;

    float real_yaw = g_ctx.m_view_angles.y;
    float fake_yaw = cmd->m_view_angles.y;
    float fake_pitch = cmd->m_view_angles.x;

    m_animstate_backup.Store(state);
    m_layer_backup.Store(local);

    vec3_t real_vel = local->m_vecVelocity();
    // NOTE: fakewalk not implemented in anti_aim_v2 — velocity spoof disabled for now
    // if (m_cfg.m_spoof_velocity && fakewalking)
    //     SpoofVelocityForAnimState(local, vec3_t(0, 0, 0));

    if (m_cfg.m_rebuild_animstate)
        RebuildAnimState(local, fake_yaw, fake_pitch);

    if (m_cfg.m_fix_pose_params)
        FixPoseParameters(local, real_yaw, fake_yaw);

    if (m_cfg.m_fix_layers)
    {
        FixAnimationLayers(local);
        FixJumpFallLayers(local);
    }

    FixSimulationTime(local);

    if (send_packet && *send_packet)
        m_should_rebuild = true;
}

// =============================================================================
// InterpolatePlayer — fix interpolation gaps
// =============================================================================
void AnimFix::InterpolatePlayer(Player* player)
{
    if (!m_cfg.m_interpolate || !player) return;
    // placeholder — exact netvar offset needed for real implementation
}

// =============================================================================
// RebuildBoneMatrices — local player, third-person bones
// =============================================================================
void AnimFix::RebuildBoneMatrices(Player* player, float fake_yaw, float fake_pitch)
{
    if (!m_cfg.m_rebuild_bones || !player) return;

    CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
    if (!state) return;

    state->m_flEyeYaw = AngleNormalize(fake_yaw);
    state->m_flEyePitch = std::clamp(fake_pitch, -90.f, 90.f);
    ClampFootYaw(state, state->m_flEyeYaw);

    // CCSGOPlayerAnimState is a flat struct in this base (no vtable — entity.h).
    // A vtable-slot [7] call reads an int field as a function pointer = crash.
    // Clamping foot yaw into the aim matrix is the whole third-person fixup
    // we need here; the engine's own Update runs on its normal call path.
}

// =============================================================================
// FixThirdPersonCamera — local player
// =============================================================================
void AnimFix::FixThirdPersonCamera(Player* player)
{
    if (!m_cfg.m_fix_thirdperson || !player || player != g_ctx.m_local) return;

    ang_t& eye_angles = player->m_angEyeAngles();
    // m_cmd is stale outside CreateMove — use the persistent view angles
    eye_angles.y = g_ctx.m_view_angles.y;
    eye_angles.x = g_ctx.m_view_angles.x;
}

// =============================================================================
// FixLocalInterpolation
// =============================================================================
void AnimFix::FixLocalInterpolation(Player* player)
{
    if (!m_cfg.m_fix_local_interp || !player || player != g_ctx.m_local) return;

    float* interp_amount = reinterpret_cast<float*>(
        reinterpret_cast<uintptr_t>(player) + 0x64);

    if (*interp_amount > g_csgo.m_globals->m_interval * 2.f)
        *interp_amount = g_csgo.m_globals->m_interval;
}

// =============================================================================
// SetupBonesFix — local player
// =============================================================================
void AnimFix::SetupBonesFix(Player* player, matrix3x4_t* bone_to_world)
{
    if (!m_cfg.m_rebuild_bones || !player || !bone_to_world) return;

    if (player == g_ctx.m_local)
        RebuildBoneMatrices(player, g_ctx.m_view_angles.y, g_ctx.m_view_angles.x);
}

// =============================================================================
// =============================================================================
//
//                     ENEMY ANIMSTATE RECONSTRUCTION (NEW)
//
// =============================================================================
// =============================================================================

// =============================================================================
// ReconstructEnemyAnimState — uses A1-A5 shared math to reconstruct
// what the server's animstate looks like for this enemy
//
// reads: networked eye angles, velocity (from origin delta), layers, LBY
// writes: enemy_anim_record_t with reconstructed foot yaw, max delta, etc.
// =============================================================================
void AnimFix::ReconstructEnemyAnimState(Player* player, int index)
{
    if (!player || index < 1 || index > 64) return;

    auto& rec = m_enemy_records[index];

    CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
    C_AnimationLayer* layers = player->m_AnimOverlay();
    if (!state || !layers) return;

    // ---- snapshot networked data ----
    rec.m_eye_yaw   = player->m_angEyeAngles().y;
    rec.m_eye_pitch = player->m_angEyeAngles().x;
    rec.m_lby       = player->m_flLowerBodyYawTarget();
    rec.m_on_ground = (player->m_fFlags() & FL_ONGROUND) != 0;
    rec.m_duck_amount = state->m_flAnimDuckAmount;

    // ---- simtime / choke detection ----
    float simtime = player->m_flSimulationTime();
    float interval = g_csgo.m_globals->m_interval;

    rec.m_old_simtime = rec.m_simtime;
    rec.m_simtime = simtime;

    if (rec.m_old_simtime > 0.f)
    {
        float dt = simtime - rec.m_old_simtime;
        rec.m_choked_ticks = (dt > 0.f) ? (int)(dt / interval + 0.5f) : 0;
    }

    // ---- velocity reconstruction from origin delta ----
    // use lagcomp's CalcVelocity if we have records, otherwise read from animstate
    vec3_t velocity = state->m_vecVelocity;

    // NOTE: lagcomp_fix.h not present in this tree — using animstate velocity as-is.
    // When lagcomp is integrated, prefer origin-delta velocity from lagcomp records here.

    rec.m_velocity_length_xy = velocity.length_2d();
    rec.m_moving = rec.m_velocity_length_xy > CSGO_ANIM_VELOCITY_MOVING_GATE;

    // ---- speed portions for aim matrix width (A1) ----
    // weapon-specific max speeds: approximate from animstate
    rec.m_speed_walk   = state->m_flSpeedAsPortionOfWalkTopSpeed;
    rec.m_speed_run    = state->m_flSpeedAsPortionOfRunTopSpeed;
    rec.m_speed_crouch = state->m_flSpeedAsPortionOfCrouchTopSpeed;
    rec.m_walk_to_run  = state->m_flWalkToRunTransition;

    // ---- A1: max desync delta from aim matrix width ----
    float width = CalcAimMatrixWidthRange(state);
    float yaw_max = fabsf(state->m_flAimYawMax * width);
    float yaw_min = fabsf(state->m_flAimYawMin * width);
    rec.m_max_delta = std::max(yaw_max, yaw_min);

    // ---- A2: reconstruct foot yaw ----
    // on the client we simulate what the server does:
    //   moving: foot yaw approaches eye yaw (ApproachAngle)
    //   standing: foot yaw approaches LBY (ApproachAngle at 100°/s)
    //   always: clamped to aim matrix bounds

    // use the current server animstate foot yaw as base
    rec.m_foot_yaw_last = rec.m_foot_yaw;
    rec.m_foot_yaw = state->m_flFootYaw;

    // simulate server foot yaw update using A3 (UpdateFootYawOnGround)
    if (rec.m_on_ground)
    {
        float update_increment = (rec.m_choked_ticks > 0)
            ? (rec.m_choked_ticks * interval) : interval;

        if (rec.m_moving)
        {
            // moving: foot approaches eye
            rec.m_foot_yaw = ApproachAngle(
                rec.m_eye_yaw, rec.m_foot_yaw,
                update_increment * (CSGO_ANIM_FOOT_APPROACH_MOVING_BASE
                    + CSGO_ANIM_FOOT_APPROACH_MOVING_WALKRUN * rec.m_walk_to_run));
        }
        else
        {
            // standing: foot approaches LBY at idle catchup rate
            rec.m_foot_yaw = ApproachAngle(
                rec.m_lby, rec.m_foot_yaw,
                update_increment * CSGO_ANIM_LOWER_CATCHUP_IDLE);
        }
    }

    // A2: clamp foot yaw to aim matrix bounds
    rec.m_foot_yaw = ClampFootYawToAimMatrix(
        rec.m_eye_yaw, rec.m_foot_yaw,
        state->m_flAimYawMax, state->m_flAimYawMin, width);

    // ---- move yaw from velocity direction ----
    if (rec.m_velocity_length_xy > 0.5f)
        rec.m_move_yaw = VelocityToYaw(velocity);

    // ---- snapshot layers for resolver detection ----
    for (int i = 0; i < ANIMATION_LAYER_COUNT; ++i)
        rec.m_layers[i] = layers[i];
}

// =============================================================================
// OnEnemyAnimUpdate — called per-enemy from FRAME_NET_UPDATE_POSTDATAUPDATE_START
//
// pipeline:
//   1. reconstruct animstate using A1-A5 math
//   2. feed into resolver (C9 priority chain)
//   3. store resolved data into record
//   4. optionally pre-build bones with resolved yaw
// =============================================================================
void AnimFix::OnEnemyAnimUpdate(Player* player, int index)
{
    if (!m_cfg.m_enemy_animfix || !player || index < 1 || index > 64) return;

    // skip local, teammates, dead/dormant
    if (player == g_ctx.m_local) return;
    if (!player->alive() || player->dormant()) return;
    if (g_ctx.m_local && player->m_iTeamNum() == g_ctx.m_local->m_iTeamNum()) return;

    // 1. reconstruct animstate
    ReconstructEnemyAnimState(player, index);

    // 2. run resolver — uses reconstructed layers + animstate
    g_resolver.OnPlayerUpdate(player, index);

    // 3. store resolved foot yaw into our record
    auto& rec = m_enemy_records[index];
    auto& res = g_resolver.m_players[index];
    if (res.m_valid)
    {
        rec.m_resolved_foot_yaw = res.m_resolved_foot_yaw;
        rec.m_resolved = true;
    }
    else
    {
        rec.m_resolved_foot_yaw = rec.m_foot_yaw;
        rec.m_resolved = false;
    }

    // 4. store into lagcomp record
    StoreEnemyRecord(player, index);
}

// =============================================================================
// SetupEnemyBones — build bone matrix with resolved foot yaw
//
// called by aimbot before hitscan. uses resolver C8 pipeline
// (g_resolver.SetupBonesForShot) if resolver has a valid read.
// fallback: use reconstructed foot yaw from our animstate reconstruction.
// =============================================================================
bool AnimFix::SetupEnemyBones(Player* player, int index, matrix3x4_t* out_matrix)
{
    if (!m_cfg.m_enemy_resolve_bones || !player || index < 1 || index > 64 || !out_matrix)
        return false;

    // try resolver C8 pipeline first — this backs up, overrides, builds, restores
    float record_time = m_enemy_records[index].m_simtime;
    if (record_time <= 0.f) record_time = g_csgo.m_globals->m_curtime;
    if (g_resolver.SetupBonesForShot(player, index, record_time, out_matrix))
        return true;

    // fallback: use our reconstructed foot yaw
    auto& rec = m_enemy_records[index];

    CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
    if (!state) return false;

    // backup
    animstate_backup_t as_bk;
    as_bk.Store(state);
    layer_backup_t ly_bk;
    ly_bk.Store(player);

    // CORRECTED: backup actual abs angles before override
    float backed_abs_yaw = player->GetAbsAngles().y;

    // override foot yaw with our reconstruction
    float use_foot_yaw = rec.m_resolved ? rec.m_resolved_foot_yaw : rec.m_foot_yaw;
    state->m_flFootYaw = use_foot_yaw;
    state->m_flFootYawLast = use_foot_yaw;

    // set abs angles to resolved foot yaw
    ang_t resolved_ang(0.f, use_foot_yaw, 0.f);
    using SetAbsAngles_t = void(__thiscall*)(void*, const ang_t&);
    reinterpret_cast<SetAbsAngles_t>((uintptr_t)g_csgo.SetAbsAngles)(player, resolved_ang);

    // CORRECTED: pose param body_yaw is 0..1, 0.5 = zero degrees, range -60..60
    float aim = AngleDiff(state->m_flEyeYaw, use_foot_yaw);
    float* pose_params = player->m_flPoseParameter();
    if (pose_params)
        pose_params[POSE_BODY_YAW] = std::clamp(aim / 120.f + 0.5f, 0.f, 1.f);

    // invalidate + rebuild
    using InvalidateBoneCache_t = void(__thiscall*)(void*);
    reinterpret_cast<InvalidateBoneCache_t>((uintptr_t)g_csgo.InvalidateBoneCache)(player);

    // CORRECTED: use record simtime, not m_curtime
    float bone_time = rec.m_simtime > 0.f ? rec.m_simtime : g_csgo.m_globals->m_curtime;
    bool result = player->SetupBones(out_matrix, 128,
        BONE_USED_BY_HITBOX | BONE_USED_BY_ATTACHMENT,
        bone_time);

    // restore
    as_bk.Restore(state);
    ly_bk.Restore(player);

    // CORRECTED: restore the abs angles you backed up, not the eye yaw
    ang_t orig_ang(0.f, backed_abs_yaw, 0.f);
    reinterpret_cast<SetAbsAngles_t>((uintptr_t)g_csgo.SetAbsAngles)(player, orig_ang);
    reinterpret_cast<InvalidateBoneCache_t>((uintptr_t)g_csgo.InvalidateBoneCache)(player);

    return result;
}

// =============================================================================
// StoreEnemyRecord — feed reconstructed animstate into lagcomp record
// so backtrack uses resolved hitboxes
// =============================================================================
void AnimFix::StoreEnemyRecord(Player* player, int index)
{
    // NOTE: lagcomp_fix.h not present in this tree — lagcomp record patching
    // disabled. When lagcomp is integrated, patch the front record's animstate
    // snapshot with the resolved foot yaw, move yaw, layers, etc. here.
    (void)player;
    (void)index;
}

// =============================================================================
// OnFrameStageNotify — local + enemy processing
// =============================================================================
void AnimFix::OnFrameStageNotify(Stage_t stage)
{
    if (!m_cfg.m_enabled || !g_ctx.m_local)
        return;

    if (stage == FRAME_RENDER_START)
    {
        if (m_should_rebuild && g_ctx.m_local->m_PlayerAnimState())
        {
            // m_cmd is only valid inside CreateMove (detours.cpp assigns it in
            // FamilyStartMove and never clears it) — between frames it is a
            // stale pointer. m_view_angles persists across the frame, so the
            // rebuild at RENDER_START reads it instead.
            float fake_yaw = g_ctx.m_view_angles.y;
            float fake_pitch = g_ctx.m_view_angles.x;
            RebuildAnimState(g_ctx.m_local, fake_yaw, fake_pitch);
            m_should_rebuild = false;
        }
    }
    else if (stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START)
    {
        // ---- NEW: process all enemy players ----
        if (m_cfg.m_enemy_animfix)
        {
            for (int i = 1; i <= g_csgo.m_globals->m_max_clients; ++i)
            {
                Player* p = g_csgo.m_entlist->GetClientEntity<Player*>(i);
                if (!p || p == g_ctx.m_local) continue;
                if (!p->alive() || p->dormant()) continue;

                // skip teammates
                if (p->m_iTeamNum() == g_ctx.m_local->m_iTeamNum()) continue;

                OnEnemyAnimUpdate(p, i);
            }
        }
    }
}

// =============================================================================
// OnCreateMove — called each tick (local player only)
// =============================================================================
void AnimFix::OnCreateMove(CUserCmd* cmd, bool* send_packet)
{
    DoLocalAnimFix(cmd, send_packet);
}

// =============================================================================
// OnPaintTraverse — visualize animstate info
// =============================================================================
void AnimFix::OnPaintTraverse()
{
    if (!m_cfg.m_visualize_lby || !g_ctx.m_local)
        return;

    CCSGOPlayerAnimState* state = g_ctx.m_local->m_PlayerAnimState();
    if (!state) return;

    int x = 15, y = 400;
    g_csgo.m_surface->DrawSetColor( Color( 255, 255, 255, 255 ) );
}

// =============================================================================
// Menu integration
// =============================================================================
void AnimFix::ReadMenuConfig()
{
    // NOTE: AntiAimMenu not present in this tree — config stays at defaults.
    // Wire up to your menu system here when available.
}
