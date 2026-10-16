#include "includes.h"
// CalcAimMatrixWidthRange / ClampFootYawToAimMatrix (source-verified aim matrix math)
#include "animstate_verify.h"

extern bool g_twist;
extern float g_twist_fl_tolerance;
extern int g_lby_side;
extern int g_lby_breaker;
extern int g_lby_angle;
HVH g_anti_aim{ };;

float HVH::DynamicFreestand(Player* target, std::vector<AdaptiveAngle> angles, bool get_worst)
{
    if (!target)
        return angles[0].m_yaw;

    constexpr float STEP = 4.f;

    vec3_t start = target->GetShootPosition();

    for (auto it = angles.begin(); it != angles.end(); ++it)
    {
        vec3_t end{ g_ctx.m_shoot_pos.x + std::cos(math::deg_to_rad(it->m_yaw)) * it->m_range,
            g_ctx.m_shoot_pos.y + std::sin(math::deg_to_rad(it->m_yaw)) * it->m_range,
            g_ctx.m_shoot_pos.z + 16.f };

        vec3_t dir = end - start;
        float len = dir.normalize();
        if (len <= 0.f)
            continue;

        float i = 0.f;
        for (; i < len; i += STEP)
        {
            int contents = g_csgo.m_engine_trace->GetPointContents(start + (dir * i), MASK_SHOT_HULL);
            if (!(contents & MASK_SHOT_HULL))
                continue;

            it->m_dist += STEP;
        }

        vec3_t endpos = start + (dir * i);
        if (endpos.dist_to(start) < endpos.dist_to(end))
        {
            it->m_dist = std::numeric_limits<float>::max();
        }
    }

    std::sort(angles.begin(), angles.end(), [](const AdaptiveAngle& a, const AdaptiveAngle& b)
        {
            return a.m_dist > b.m_dist;
        });

    return get_worst ? angles.back().m_yaw : angles.front().m_yaw;
}

float HVH::normalize_yaw(float yaw)
{
    yaw = std::fmod(yaw, 360.f);
    if (yaw > 180.f) yaw -= 360.f;
    if (yaw < -180.f) yaw += 360.f;
    return yaw;
}

void HVH::ClampMovementSpeed(CUserCmd* cmd, float speed)
{
    if (!cmd)
        return;

    if (speed <= 0.f)
    {
        cmd->m_forward_move = 0.f;
        cmd->m_side_move = 0.f;
        return;
    }

    const float move = std::sqrtf(
        (cmd->m_forward_move * cmd->m_forward_move) +
        (cmd->m_side_move * cmd->m_side_move)
    );

    if (move <= speed || move <= 0.f)
        return;

    const float scale = speed / move;
    cmd->m_forward_move *= scale;
    cmd->m_side_move *= scale;
}

void HVH::ResetSessionState()
{
    m_peek_ticks = 0;
    m_mode = STAND;
    m_lastmove_yaw = 0.f;
    m_lastmove_valid = false;
    in_twist = false;
    force_fl = false;
    m_lby_timing_broken = false;
    m_standing = false;
    m_fakewalking = false;
    m_stand_flick = false;
    m_stand_flick_pending = false;
    m_body_pred = 0.f;
    m_enemy_body_pred = 0.f;
    m_stand_cmd_yaw = 0.f;
    m_sell_start = 0.f;
    m_hide_angle = 0;
    m_override_flag = 0;
    m_break_phase = 0;
    m_peek_result = PEEK_BRO;
    m_best_target = nullptr;
    m_last_layer_speed = 0.f;
    m_was_walk = false;
    m_was_in_air = false;
    m_air_ticks = 0;
    m_air_move_side = 0;
    m_desync_ticks_landed = 0;
    m_desync_jitter_valid = false;
    m_desync_jitter_yaw = 0.f;
    m_landing_block_until = 0.f;
    m_standing_aa_block_until = 0.f;
    m_stand_cached_yaw = 0.f;
    m_stand_cached_body = 0.f;
    m_stand_cached_break = 0;
    m_stand_cached_hide = 0;
    m_stand_cached_phase = 0;
    m_air_yaw_index = 0;
    m_air_yaw_next_switch = 0.f;
}

void HVH::PreventMoveLayer()
{
    if (!g_ctx.m_weapon_info || !(g_ctx.m_local->m_fFlags() & FL_ONGROUND) || is_shooting_bullet() || !in_twist)
        return;

    const bool is_revolver = (g_ctx.m_weapon_id == REVOLVER);
    const float time = g_csgo.m_globals->m_curtime;
    const float phase = fmodf(time, 0.4f) / 0.2f;
    const float jitter = fabsf(sinf(phase * math::pi));
    const float velocity = g_ctx.m_local->m_vecVelocity().length_2d();
    const float velocity_scale = velocity / 25.f;

    const float base_speed =
        is_revolver
        ? 180.f
        : g_ctx.m_weapon->GetMaxSpeed();

    const float target_speed = std::max(
        base_speed - 0.1f - velocity_scale * jitter,
        0.f
    );

    ClampMovementSpeed(g_ctx.m_cmd, target_speed);
}



//void HVH::SpoofMoveLayer()
//{
//    if ( !g_ctx.m_local || !g_csgo.m_globals )
//        return;
//
//    if ( !( g_ctx.m_local->m_fFlags() & FL_ONGROUND ) )
//    {
//        m_last_layer_speed = g_ctx.m_local->m_vecVelocity().length_2d();
//        return;
//    }
//
//    C_AnimationLayer* layers = g_ctx.m_local->m_AnimOverlay();
//    if ( !layers )
//    {
//        m_last_layer_speed = g_ctx.m_local->m_vecVelocity().length_2d();
//        return;
//    }
//
//    float speed = g_ctx.m_local->m_vecVelocity().length_2d();
//
//    // method 1: weight spoofer
//    // resolver only fires when previous_record->layers[6].weight <= 0.f
//    // keep a micro-value so the condition never evaluates true
//    if ( speed < 0.1f )
//    {
//        if ( layers[ ANIMATION_LAYER_MOVEMENT_MOVE ].m_weight <= 0.f )
//            layers[ ANIMATION_LAYER_MOVEMENT_MOVE ].m_weight = 0.001f;
//    }
//
//    // method 2: cycle scrambling
//    // on stand  move transition the resolver predicts cycle for all 8 directions
//    // and compares int(pred_cycle*1000) == int(cycle*1000)
//    // shifting cycle by 0.003 corrupts the integer cast so no direction matches
//    if ( m_last_layer_speed < 0.1f && speed > 0.1f )
//    {
//        layers[ ANIMATION_LAYER_MOVEMENT_MOVE ].m_cycle =
//            std::fmod( layers[ ANIMATION_LAYER_MOVEMENT_MOVE ].m_cycle + 0.003f, 1.0f );
//    }
//
//    m_last_layer_speed = speed;
//}

Player* HVH::FindBestTarget()
{
    if (!g_ctx.m_local || !g_csgo.m_entlist || !g_csgo.m_globals)
        return nullptr;

    Player* best = nullptr;
    float best_fov = 180.f;

    for (int i = 1; i <= g_csgo.m_globals->m_max_clients; ++i)
    {
        Player* p = g_csgo.m_entlist->GetClientEntity< Player* >(i);
        if (!p || p == g_ctx.m_local)
            continue;
        if (p->dormant())
            continue;
        if (!p->alive())
            continue;
        if (p->m_iTeamNum() == g_ctx.m_local->m_iTeamNum())
            continue;

        float fov = math::GetFOV(g_ctx.m_view_angles, g_ctx.m_shoot_pos, p->WorldSpaceCenter());
        if (fov < best_fov)
        {
            best_fov = fov;
            best = p;
        }
    }

    return best;
}

bool HVH::is_shooting_bullet()
{
    if (!wants_shoot_bullet() || !g_ctx.m_local || !g_ctx.m_weapon || !g_csgo.m_globals)
        return false;

    if (g_ctx.m_weapon->m_iClip1() <= 0)
        return false;

    const float shoot_time = g_ctx.m_local->m_nTickBase() * g_csgo.m_globals->m_interval;
    if (g_ctx.m_local->m_flNextAttack() > shoot_time)
        return false;

    if (g_ctx.m_weapon->m_flNextPrimaryAttack() > shoot_time)
        return false;

    const bool attack2 = (g_ctx.m_cmd->m_buttons & IN_ATTACK2) != 0;
    if (g_ctx.m_weapon_id == REVOLVER && attack2 && g_ctx.m_weapon->m_flNextSecondaryAttack() > shoot_time)
        return false;

    return true;
}

bool HVH::wants_shoot_bullet()
{
    if (!g_ctx.m_cmd || !g_ctx.m_weapon_info)
        return false;

    const bool attack = (g_ctx.m_cmd->m_buttons & IN_ATTACK) != 0;
    const bool attack2 = (g_ctx.m_cmd->m_buttons & IN_ATTACK2) != 0;
    const bool wants_bullet = attack || (g_ctx.m_weapon_id == REVOLVER && attack2);
    if (!wants_bullet)
        return false;

    const int weapon_type = g_ctx.m_weapon_info->m_weapon_type;
    return weapon_type != WEAPONTYPE_GRENADE
        && weapon_type != WEAPONTYPE_KNIFE
        && weapon_type != WEAPONTYPE_C4
        && weapon_type != WEAPONTYPE_HEALTHSHOT;
}

void HVH::ApplySupression(float& yaw)
{
    const int ticks_after_upd = game::TIME_TO_TICKS(g_csgo.m_globals->m_curtime - (m_body_pred - 1.1f));
    const int ticks_until_upd = game::TIME_TO_TICKS(m_body_pred - g_csgo.m_globals->m_curtime);

    if (ticks_after_upd <= 2)
    {
        yaw = normalize_yaw(m_lastmove_yaw - 90.f);
        m_hide_angle = 1;
        return;
    }

    if (ticks_until_upd <= 2)
    {
        yaw = normalize_yaw(m_lastmove_yaw + 90.f);
        m_hide_angle = 1;
    }
}

float HVH::fit_yaw_to_aim_matrix(float eye_yaw, float yaw)
{
    if (!g_ctx.m_local)
        return yaw;

    CCSGOPlayerAnimState* state = g_ctx.m_local->m_PlayerAnimState();
    if (!state)
        return yaw;

    // csgo_playeranimstate.cpp:2332-2337 (width) and 2339-2349 (per-side clamp)
    const float width = CalcAimMatrixWidthRange(state);

    return ClampFootYawToAimMatrix(eye_yaw, yaw,
        state->m_flAimYawMax, state->m_flAimYawMin, width);
}

// ---------------------------------------------------------------------------
// reference anti-aim, ported onto this code base
// ---------------------------------------------------------------------------
HVH::Band HVH::BuildBand(CCSGOPlayerAnimState* as)
{
    Band band;
    if (!as)
        return band;

    float t = as->m_flSpeedAsPortionOfWalkTopSpeed;
    math::clamp(t, 0.f, 1.f);
    float width = Lerp(t, 1.0f,
        Lerp(as->m_flWalkToRunTransition,
            CSGO_ANIM_AIM_NARROW_WALK,
            CSGO_ANIM_AIM_NARROW_RUN));
    if (as->m_flAnimDuckAmount > 0.f) {
        float ct = as->m_flSpeedAsPortionOfCrouchTopSpeed;
        math::clamp(ct, 0.f, 1.f);
        width = Lerp(as->m_flAnimDuckAmount * ct, width, CSGO_ANIM_AIM_NARROW_CROUCHMOVING);
    }

    band.width = width;
    band.pos   = as->m_flAimYawMax * width;
    band.neg   = as->m_flAimYawMin * width;
    return band;
}

// friction/brake per cs_gamemovement.cpp:1355-1357; the accel/air branch per
// CGameMovement::AirAccelerate (gamemovement.cpp:1960). The stamina term of the
// reference is omitted: this base has no m_flStamina netvar.
void HVH::ClampVelocity(CUserCmd* c, Player* me, float target)
{
    if (!c || !me)
        return;

    const vec3_t vel = me->m_vecVelocity();
    float vx = vel.x, vy = vel.y;

    float budget = CS_PLAYER_SPEED_RUN;
    {
        const float wmax = (g_ctx.m_weapon ? g_ctx.m_weapon->GetMaxSpeed() : 0.f);
        if (wmax > 0.f && wmax < budget)
            budget = wmax;
    }

    const bool on_ground = (me->m_fFlags() & FL_ONGROUND) != 0;
    const float surface  = me->m_surfaceFriction();
    const float dt       = g_csgo.m_globals ? g_csgo.m_globals->m_interval : 0.f;

    c->m_buttons &= ~IN_SPEED;

    if (on_ground)
    {
        const float mag = std::sqrt(vx * vx + vy * vy);
        if (mag >= 0.1f)
        {
            const float control = mag < (g_csgo.sv_stopspeed ? g_csgo.sv_stopspeed->GetFloat() : 1.f) ? (g_csgo.sv_stopspeed ? g_csgo.sv_stopspeed->GetFloat() : 1.f) : mag;
            const float drop    = (g_csgo.sv_friction ? g_csgo.sv_friction->GetFloat() : 1.f) * surface * control * dt;
            float newspeed = mag - drop;
            if (newspeed < 0.f)
                newspeed = 0.f;
            if (newspeed != mag)
            {
                const float ratio = newspeed / mag;
                vx *= ratio;
                vy *= ratio;
            }
        }
    }

    const float mag = std::sqrt(vx * vx + vy * vy);
    if (target >= mag)
    {
        float fwd  = c->m_forward_move;
        float side = c->m_side_move;
        const float cur = std::sqrt(fwd * fwd + side * side);
        if (target <= 0.1f || cur <= 0.1f)
            fwd = side = 0.f;
        else
        {
            fwd  = fwd / cur * target;
            side = side / cur * target;
        }
        c->m_forward_move = fwd;
        c->m_side_move    = side;
        return;
    }

    const float vel_yaw = (vx == 0.f && vy == 0.f)
        ? 90.f
        : math::rad_to_deg(std::atan2(-vy, -vx));
    const float delta = math::deg_to_rad(c->m_view_angles.y - vel_yaw);

    float fwd_dir  = std::cos(delta);
    float side_dir = std::sin(delta);
    {
        const float len = std::sqrt(fwd_dir * fwd_dir + side_dir * side_dir + 1e-10f);
        fwd_dir  /= len;
        side_dir /= len;
    }

    float scale;
    if (on_ground)
        scale = mag - target;
    else
    {
        const float accel = (g_csgo.sv_airaccelerate ? g_csgo.sv_airaccelerate->GetFloat() : 1.f) * dt * surface;
        scale = accel != 0.f ? (mag - target) / accel : 0.f;
        if (scale > budget)
            scale = budget;
    }

    c->m_forward_move = fwd_dir * scale;
    c->m_side_move    = side_dir * scale;
}

float HVH::do_anti_aim(float real_yaw,
    anti_aim_ctx& aa_ctx,
    tick_record_ctx& record_ctx,
    bool               on_send_tick)
{

    m_standing = false;
    m_stand_flick = false;
    m_hide_angle = 0;
    m_override_flag = 0;
    m_break_phase = 0;

    const bool on_ground = (g_ctx.m_local->m_fFlags() & FL_ONGROUND) != 0;
    const bool in_air = !on_ground;
    const bool just_landed = m_was_in_air && on_ground;

    if (just_landed)
    {
        m_landing_block_until = g_csgo.m_globals->m_curtime + 0.4f;
        m_standing_aa_block_until = g_csgo.m_globals->m_curtime + 0.7f + 0.7f;
        m_lby_timing_broken = false;
        m_body_pred = g_csgo.m_globals->m_curtime + 0.22f;
    }

    const bool landing_blocked = g_csgo.m_globals->m_curtime < m_landing_block_until;
    const float live_view_yaw = g_ctx.m_cmd ? g_ctx.m_cmd->m_view_angles.y : g_ctx.m_view_before_mod;

    const auto keep_client_yaw_on_crosshair = [&]() {
        real_yaw = aa_ctx.m_final_yaw = live_view_yaw;
        aa_ctx.m_target_body = live_view_yaw;
        aa_ctx.m_break_state = 0;
        m_stand_cmd_yaw = live_view_yaw;
        m_stand_flick_pending = false;
        };

    if (in_air && !m_was_in_air)
    {
        m_air_ticks = 0;
        m_air_move_side = 0;
    }
    if (!in_air)
        m_air_ticks = 0;

    //// grimace line 400-406: skip AA during grenade throw
    //if (g_ctx.m_weapon && g_ctx.m_weapon_info
    //    && g_ctx.m_weapon_info->m_weapon_type == WEAPONTYPE_GRENADE
    //    && !g_ctx.m_weapon->m_bPinPulled())
    //{
    //    keep_client_yaw_on_crosshair();
    //    m_was_in_air = in_air;
    //    return real_yaw;
    //}

    //// grimace line 416-426: skip AA during weapon fire
    //if (g_ctx.m_weapon && g_ctx.m_weapon_info && g_ctx.m_cmd
    //    && g_ctx.m_weapon_info->m_weapon_type != WEAPONTYPE_GRENADE)
    //{
    //    const bool attack2 = (g_ctx.m_cmd->m_buttons & IN_ATTACK2) != 0;
    //    const bool knife = g_ctx.m_weapon_info->m_weapon_type == WEAPONTYPE_KNIFE
    //        && g_ctx.m_weapon_id != ZEUS;

    //    if (wants_shoot_bullet() || (attack2 && knife))
    //    {
    //        keep_client_yaw_on_crosshair();
    //        m_was_in_air = in_air;
    //        return real_yaw;
    //    }
    //}

    // grimace lines 384-392: mode from velocity
    // m_fakewalking prevents WALK mode during DLL fakewalk (vel may fluctuate
    // above 0.1 briefly, but server sees standing)
    {
        const float vel = g_ctx.m_local->m_vecVelocity().length_2d();
        if (!on_ground)
        {
            aa_ctx.m_anti_aim_mode = AIR;
            m_lby_timing_broken = false;
            m_sell_start = 0.f;
        }
        else if (vel > 0.1f && !m_fakewalking)
        {
            aa_ctx.m_anti_aim_mode = WALK;
            m_lby_timing_broken = false;
            m_sell_start = 0.f;
        }
        else
            aa_ctx.m_anti_aim_mode = STAND;

        m_mode = aa_ctx.m_anti_aim_mode;
    }

    const bool stand_aa_active = aa_ctx.m_anti_aim_mode == STAND;

    // STAND AA — recode1 hvh.cpp lines 547-708
    if (stand_aa_active )
    {
        in_twist = false;
        m_standing = true;
        m_override_flag = 1;
        m_fakewalk_lby_stop = true;

        const float abs_yaw = record_ctx.m_abs_yaw;
        const float interval = g_csgo.m_globals->m_interval;
        const float body_pred = m_body_pred;

        // send tick: adaptive LBY fake body (recode1 line 547-578)
        if (on_send_tick)
        {
            if (m_enemy_body_pred > m_body_pred)
            {
                real_yaw = m_lastmove_yaw;
            }
            else
            {
                real_yaw = DynamicFreestand(m_best_target,
                    { live_view_yaw + 180.f, live_view_yaw - 90.f, live_view_yaw + 90.f },
                    g_lby_side == 0);

                if (std::fabsf(math::AngleDiff(real_yaw, m_stand_cached_yaw)) <= 35.f)
                    real_yaw = normalize_yaw(real_yaw + g_csgo.RandomFloat(-90.f, 90.f));
            }
            aa_ctx.m_final_yaw = real_yaw;
            aa_ctx.m_target_body = normalize_yaw(real_yaw + 180.f);
            aa_ctx.m_break_state = m_stand_cached_break;
            m_hide_angle = m_stand_cached_hide;
            m_break_phase = m_stand_cached_phase;
            m_stand_cmd_yaw = real_yaw;
            m_was_in_air = in_air;
            // fit what we apply into the animstate's per-side aim matrix
            real_yaw = fit_yaw_to_aim_matrix(live_view_yaw, real_yaw);
            return real_yaw;
        }

        // choked tick: replay cached (recode1 line 580-582)
        if (g_ctx.m_lag > 0)
        {
            real_yaw = aa_ctx.m_final_yaw = m_stand_cached_yaw;
            aa_ctx.m_target_body = m_stand_cached_body;
            aa_ctx.m_break_state = m_stand_cached_break;
            m_hide_angle = m_stand_cached_hide;
            m_break_phase = m_stand_cached_phase;
            m_stand_cmd_yaw = m_stand_cached_yaw;
            m_was_in_air = in_air;
            // fit what we apply into the animstate's per-side aim matrix
            real_yaw = fit_yaw_to_aim_matrix(live_view_yaw, real_yaw);
            return real_yaw;
        }

        // tick 0: state machine (recode1 line 656-705)
        if (!m_lby_timing_broken)
        {
            if (g_csgo.m_globals->m_curtime > body_pred + interval * 4.f)
            {
                real_yaw = m_lastmove_yaw;
                m_lby_timing_broken = true;
                m_hide_angle = 2;
                m_break_phase = 1;
            }
            else if (g_csgo.m_globals->m_curtime > body_pred)
            {
                real_yaw = abs_yaw;
                m_hide_angle = 1;
                m_break_phase = 0;
            }
            else
            {
                real_yaw = normalize_yaw(m_lastmove_yaw + 180.f);
                m_hide_angle = 1;
                m_break_phase = 0;
            }
            aa_ctx.m_break_state = 1;
        }
        else if (g_csgo.m_globals->m_curtime > body_pred)
        {
            real_yaw = m_lastmove_yaw;
            m_hide_angle = 2;
            m_break_phase = 1;

            if (m_sell_start <= 0.f)
                m_sell_start = g_csgo.m_globals->m_curtime;

            if (g_csgo.m_globals->m_curtime >= m_sell_start + 0.22f)
            {
                m_body_pred = g_csgo.m_globals->m_curtime + 1.1f;
                m_sell_start = 0.f;
            }
            aa_ctx.m_break_state = 1;
        }
        else if (!g_lby_breaker)
        {
            if (g_csgo.m_globals->m_curtime >= body_pred - interval * 2.f)
            {
                real_yaw = normalize_yaw(m_lastmove_yaw + 180.f);
                m_hide_angle = 1;
            }
            else
            {
                float back = live_view_yaw + 180.f;
                float lby_s = normalize_yaw(back - m_lastmove_yaw);
                real_yaw = normalize_yaw(lby_s > 0.f ? back + 45.f : back - 45.f);
            }
            aa_ctx.m_break_state = 1;
            m_break_phase = 0;
        }
        else
        {
            real_yaw = normalize_yaw(m_lastmove_yaw + 180.f);
            ApplySupression(real_yaw);
            aa_ctx.m_break_state = 1;
            m_break_phase = 0;
        }

        aa_ctx.m_final_yaw = real_yaw;

        m_stand_cached_yaw = real_yaw;
        m_stand_cached_body = normalize_yaw(real_yaw + 180.f);
        m_stand_cached_break = aa_ctx.m_break_state;
        m_stand_cached_hide = m_hide_angle;
        m_stand_cached_phase = m_break_phase;

        aa_ctx.m_target_body = normalize_yaw(real_yaw + 180.f);
        m_stand_cmd_yaw = real_yaw;
    }

    if (aa_ctx.m_anti_aim_mode == AIR)
    {
        in_twist = false;

        // ★ (1) 공중에서의 원본 real_yaw를 m_lastmove_yaw에 저장
        //     → 착지 후 STAND AA가 이 값을 회피 대상으로 삼음
        m_lastmove_yaw = real_yaw;

        // ★ (2) 중복된 if(aa_ctx.m_anti_aim_mode == AIR) 제거하고 바로 초기화
        m_desync_ticks_landed = 0;

        if (g_ctx.m_lagcomp)
        {
            real_yaw = aa_ctx.m_final_yaw = normalize_yaw(DynamicFreestand(m_best_target,
                { live_view_yaw + 180.f, live_view_yaw - 90.f, live_view_yaw + 90.f }));
            aa_ctx.m_target_body = real_yaw;
            aa_ctx.m_break_state = 1;
            m_hide_angle = 1;
            m_air_yaw_index = 0;
            m_air_yaw_next_switch = 0.f;
        }
        else
        {
            constexpr float low_velocity_threshold = 20.f;
            constexpr float switch_interval = 1.f;
            constexpr float low_velocity_offsets[] = { 0.f, 60.f, -60.f, 180.f };

            const float velocity = g_ctx.m_local->m_vecVelocity().length_2d();
            // 이제 m_lastmove_yaw는 공중에서의 원본 요이므로, 이를 기준으로 베이스 계산
            const float base_yaw = normalize_yaw(m_lastmove_yaw - 110.f);
            float target_real_yaw = base_yaw;

            if (velocity < low_velocity_threshold)
            {
                if (m_air_yaw_next_switch <= 0.f)
                    m_air_yaw_next_switch = g_csgo.m_globals->m_curtime + switch_interval;
                else if (g_csgo.m_globals->m_curtime >= m_air_yaw_next_switch)
                {
                    m_air_yaw_index = (m_air_yaw_index + 1) %
                        (sizeof(low_velocity_offsets) / sizeof(low_velocity_offsets[0]));
                    m_air_yaw_next_switch = g_csgo.m_globals->m_curtime + switch_interval;
                }

                target_real_yaw = normalize_yaw(base_yaw + low_velocity_offsets[m_air_yaw_index]);
            }
            else
            {
                m_air_yaw_index = 0;
                m_air_yaw_next_switch = g_csgo.m_globals->m_curtime + switch_interval;
            }

            real_yaw = aa_ctx.m_final_yaw = target_real_yaw;
            aa_ctx.m_target_body = target_real_yaw;
            aa_ctx.m_break_state = velocity < low_velocity_threshold ? (m_air_yaw_index & 1) : 1;
        }
    }
    if (aa_ctx.m_anti_aim_mode == WALK && !landing_blocked)
    {

        if (!g_twist)
        {
            in_twist = false;
            force_fl = false;
            m_lastmove_yaw = real_yaw;
            m_was_in_air = in_air;
            // fit what we apply into the animstate's per-side aim matrix
            real_yaw = fit_yaw_to_aim_matrix(live_view_yaw, real_yaw);
            return real_yaw;
        }

        m_lastmove_yaw = real_yaw;
        if (std::fabsf(math::AngleDiff(record_ctx.m_abs_yaw, real_yaw)) <= g_twist_fl_tolerance)
        {
            in_twist = true;
            real_yaw = aa_ctx.m_final_yaw = aa_ctx.m_target_body =
                record_ctx.m_abs_yaw + copysignf(g_twist_fl_tolerance, math::AngleDiff(real_yaw, record_ctx.m_abs_yaw));
        }
    }

    m_was_in_air = in_air;
    // fit what we apply into the animstate's per-side aim matrix
    real_yaw = fit_yaw_to_aim_matrix(live_view_yaw, real_yaw);
    return real_yaw;
};