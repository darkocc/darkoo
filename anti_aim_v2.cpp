// =============================================================================
// anti_aim_v2.cpp — REWRITTEN: real desync via send/choke split
// language: C++17, target: Windows 11 / CSGO 2018, MSVC
//
// Architecture:
//   choked cmd = REAL yaw   (where you aim — server animates this first)
//   sent cmd   = FAKE yaw   (what the enemy renderer sees — networked eye)
//   break tick = choked with real yaw, micro-move OFF
//
// 2018 timer from SetUpVelocity:
//   moving (speed > 0.1):  timer = curtime + 1.1*0.2,  LBY = eyeYaw
//   standing, timer due:   timer = curtime + 1.1,       LBY = eyeYaw
//
// Per-tick order:
//   1. shooting → real viewangles, force send, no AA
//   2. detect mode (stand/walk/air)
//   3. check BreakDue — if yes, force choke, set real yaw, NO micro-move
//   4. else: choked tick → real yaw, sent tick → fake yaw
//   5. correct_movement
//   6. micro-move ONLY if standing AND NOT breaking
// =============================================================================
#include "includes.h"
#include "anti_aim_v2.h"

AntiAimV2 g_antiaim_v2;

void AntiAimV2::Reset()
{
    *this = AntiAimV2{};
}

// =============================================================================
// B7: correct_movement — unchanged, this was fine
// =============================================================================
void AntiAimV2::CorrectMovement(CUserCmd* cmd, float old_yaw, float new_yaw)
{
    if (!cmd) return;

    float delta = math::deg_to_rad(new_yaw - old_yaw);
    float cos_d = cosf(delta);
    float sin_d = sinf(delta);

    float fwd  = cmd->m_forward_move;
    float side = cmd->m_side_move;

    cmd->m_forward_move = cos_d * fwd + sin_d * side;
    cmd->m_side_move    = cos_d * side - sin_d * fwd;
}

// =============================================================================
// B3: micro-move — cmd.sidemove = ±1.1 alternating
// keeps velocity above 0.1 so server stays in moving branch
// NEVER called during a break window
// =============================================================================
void AntiAimV2::ApplyMicroMove(CUserCmd* cmd)
{
    if (!cmd) return;

    float vel = g_ctx.m_local->m_vecVelocity().length_2d();
    if (vel > 5.f) return; // already moving enough

    // spec: command_number & 1 for alternation, not a sign toggle
    cmd->m_side_move = (cmd->m_command_number & 1) ? 1.1f : -1.1f;

    // micro-move keeps server in moving branch → refresh the break timer
    // so the 1.1s realign never fires while micro-move is active
    m_next_break = g_csgo.m_globals->m_curtime + CSGO_ANIM_LOWER_REALIGN_DELAY * 0.2f;
}

// =============================================================================
// aim matrix max delta (A1)
// =============================================================================
float AntiAimV2::GetMaxDelta()
{
    CCSGOPlayerAnimState* state = g_ctx.m_local->m_PlayerAnimState();
    if (!state) return 58.f;

    float width = CalcAimMatrixWidthRange(state);
    float yaw_max = fabsf(state->m_flAimYawMax * width);
    float yaw_min = fabsf(state->m_flAimYawMin * width);
    return std::max(yaw_max, yaw_min);
}

// =============================================================================
// FakeYaw — what the sent (networked) cmd carries
// the enemy renderer draws this. it is NOT where the feet are.
// =============================================================================
float AntiAimV2::FakeYaw(float view_yaw)
{
    // 0 = face the view, 180 = backward fake
    return AngleNormalize(view_yaw + m_fake_offset);
}

// =============================================================================
// RealYaw — what the choked cmd carries
// server animates this first. feet sit at eye - side * maxDelta.
// the gap between FakeYaw (networked eye) and RealYaw (animated first)
// is the desync.
// =============================================================================
float AntiAimV2::RealYaw(float view_yaw)
{
    // feet will sit here. side is stable until a key or the 0.3s timer flips.
    return AngleNormalize(view_yaw + m_fake_offset + m_side * GetMaxDelta());
}

// =============================================================================
// BreakDue — 2018 LBY timer
//
// moving (speed > 0.1):
//   server does: timer = curtime + 1.1 * 0.2,  LBY = eyeYaw
//   we mirror that — keep pushing the break forward
//
// standing, timer expired:
//   server does: timer = curtime + 1.1,  LBY = eyeYaw
//   we fire the break
// =============================================================================
bool AntiAimV2::BreakDue(float curtime, bool on_ground, float speed)
{
    // spec: if speed > 0.1 or not on ground → nextBreak = curtime + 1.1 * 0.2
    if (speed > CSGO_ANIM_VELOCITY_MOVING_GATE || !on_ground)
    {
        m_next_break = curtime + CSGO_ANIM_LOWER_REALIGN_DELAY * 0.2f;
        return false;
    }

    // standing on ground: check if timer is due
    if (curtime >= m_next_break)
    {
        // fire the break, schedule next at +1.1s
        m_next_break = curtime + CSGO_ANIM_LOWER_REALIGN_DELAY;
        return true;
    }

    return false;
}

// =============================================================================
// B6: freestand side pick — STABLE
// only re-evaluates after m_side_min_hold (0.3s default)
// no per-tick flipping from noisy point-contents traces
// =============================================================================
int AntiAimV2::PickSide(Player* target, float eye_yaw)
{
    float curtime = g_csgo.m_globals->m_curtime;

    // hold side for at least 0.3s to prevent flickering
    if (curtime - m_side_set_time < m_side_min_hold)
        return m_side;

    if (!target || !g_ctx.m_local)
        return m_side;

    vec3_t local_pos = g_ctx.m_local->GetShootPosition();
    vec3_t enemy_pos = target->GetShootPosition();

    vec3_t delta = local_pos - enemy_pos;
    float to_us_yaw = atan2f(delta.y, delta.x) * (180.f / 3.14159265f);

    float left_yaw  = AngleNormalize(to_us_yaw - 90.f);
    float right_yaw = AngleNormalize(to_us_yaw + 90.f);

    auto trace_open = [&](float yaw) -> float {
        vec3_t dir;
        math::AngleVectors(ang_t(0, yaw, 0), &dir);
        float dist = 0.f;
        for (float d = 4.f; d <= 64.f; d += 4.f)
        {
            vec3_t pos = local_pos + dir * d;
            if (g_csgo.m_engine_trace->GetPointContents(pos, MASK_SHOT_HULL) & MASK_SHOT_HULL)
                break;
            dist = d;
        }
        return dist;
    };

    float left_dist  = trace_open(left_yaw);
    float right_dist = trace_open(right_yaw);

    int new_side = (left_dist < right_dist) ? -1 : 1;

    if (new_side != m_side)
    {
        m_side = new_side;
        m_side_set_time = curtime;
    }

    return m_side;
}

// =============================================================================
// find nearest enemy for freestand
// =============================================================================
Player* AntiAimV2::FindTarget()
{
    Player* target = nullptr;
    float best_fov = 180.f;
    for (int i = 1; i <= g_csgo.m_globals->m_max_clients; ++i)
    {
        Player* p = g_csgo.m_entlist->GetClientEntity<Player*>(i);
        if (!p || p == g_ctx.m_local || p->dormant() || !p->alive())
            continue;
        if (p->m_iTeamNum() == g_ctx.m_local->m_iTeamNum())
            continue;
        float fov = math::GetFOV(g_ctx.m_view_angles, g_ctx.m_shoot_pos, p->WorldSpaceCenter());
        if (fov < best_fov)
        {
            best_fov = fov;
            target = p;
        }
    }
    return target;
}

// =============================================================================
// Run — main entry point
//
// The ONE RULE: choked cmd = real, sent cmd = fake.
// Break tick = choked real. Micro-move never on during break.
// =============================================================================
void AntiAimV2::Run(CUserCmd* cmd, bool* send_packet)
{
    if (!cmd || !g_ctx.m_local || !g_ctx.m_local->alive() || !g_csgo.m_globals)
    {
        m_active = false;
        return;
    }

    m_force_choke   = false;
    m_lby_breaking  = false;

    float eye_yaw   = cmd->m_view_angles.y;
    float old_yaw   = eye_yaw;
    float curtime   = g_csgo.m_globals->m_curtime;

    bool on_ground  = (g_ctx.m_local->m_fFlags() & FL_ONGROUND) != 0;
    bool on_send    = send_packet && *send_packet;
    float speed     = g_ctx.m_local->m_vecVelocity().length_2d();

    // ---- step 1: shooting → real viewangles, force send, skip AA ----
    {
        bool shooting = false;
        if (g_ctx.m_weapon_info && g_ctx.m_weapon)
        {
            bool attack  = (cmd->m_buttons & IN_ATTACK) != 0;
            bool attack2 = (cmd->m_buttons & IN_ATTACK2) != 0;
            int wtype = g_ctx.m_weapon_info->m_weapon_type;

            if (wtype != WEAPONTYPE_GRENADE && wtype != WEAPONTYPE_KNIFE
                && wtype != WEAPONTYPE_C4 && wtype != WEAPONTYPE_HEALTHSHOT)
            {
                if (attack || (g_ctx.m_weapon_id == REVOLVER && attack2))
                {
                    float shoot_time = g_ctx.m_local->m_nTickBase() * g_csgo.m_globals->m_interval;
                    if (g_ctx.m_local->m_flNextAttack() <= shoot_time
                        && g_ctx.m_weapon->m_flNextPrimaryAttack() <= shoot_time
                        && g_ctx.m_weapon->m_iClip1() > 0)
                    {
                        shooting = true;
                    }
                }
            }
        }

        if (shooting)
        {
            if (send_packet) *send_packet = true;
            m_active = false;
            return;
        }
    }

    m_active = true;

    // ---- step 2: detect mode ----
    if (!on_ground)
        m_mode = 3; // AIR
    else if (speed > CSGO_ANIM_VELOCITY_MOVING_GATE)
        m_mode = 2; // WALK
    else
        m_mode = 1; // STAND

    // ---- pick stable side ----
    Player* target = FindTarget();
    PickSide(target, eye_yaw);

    // ---- step 3: check LBY break ----
    // BreakDue always runs (refreshes timer in both builds).
    // But the FIRE path only executes in break build (!m_micro_build).
    bool breaking = BreakDue(curtime, on_ground, speed) && !m_micro_build;

    float result_yaw;

    if (breaking)
    {
        // =============================
        // LBY BREAK TICK
        // =============================
        // Force choke. The server will animate this command's yaw first,
        // and the LBY snaps to it. We set it to the REAL yaw so the feet
        // land where we want them (at maxDelta from eye).
        //
        // The SENT tick (next tick, not this one) will carry the FAKE yaw,
        // and that becomes the networked eye yaw the enemy sees.
        //
        // Result: feet at RealYaw, networked eye at FakeYaw. Desync exists.
        //
        // Micro-move is OFF this tick — we need the server to be in the
        // standing branch so the LBY realign actually fires.

        m_force_choke  = true;
        m_lby_breaking = true;
        result_yaw     = RealYaw(eye_yaw);
    }
    else
    {
        // =============================
        // NORMAL TICK
        // =============================
        // choked cmd → real yaw (server animates first, not networked)
        // sent cmd   → fake yaw (networked, enemy sees this)
        //
        // The gap between the two is the desync.

        if (on_send)
        {
            // SENT tick: fake yaw. This is what gets networked.
            result_yaw = FakeYaw(eye_yaw);
        }
        else
        {
            // CHOKED tick: real yaw. Server animates this but doesn't network it.
            result_yaw = RealYaw(eye_yaw);
        }
    }

    // ---- apply to cmd ----
    cmd->m_view_angles.y = result_yaw;

    // ---- B5: batch choke enforcement ----
    if (m_force_choke && send_packet)
        *send_packet = false;

    // ---- B7: correct_movement ----
    CorrectMovement(cmd, old_yaw, result_yaw);

    // ---- micro-move: ONLY in micro build, standing, on ground, NOT breaking ----
    // break build: micro-move never runs — BreakDue fires the desync.
    // micro build: micro-move keeps server in moving branch, timer never fires.
    if (m_micro_build && m_mode == 1 && on_ground && !m_lby_breaking)
        ApplyMicroMove(cmd);

    // track air state
    if (!on_ground) m_air_ticks++;
    else m_air_ticks = 0;
}
