#pragma once
// =============================================================================
// animstate_verify.h — verified CCSGOPlayerAnimState layout + helpers
// matches KittenPopo's csgo-2018-source  exactly (x86 offsets)
// =============================================================================

// 2018 source constants from csgo_playeranimstate.cpp
#define CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX       58.0f
#define CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN      -58.0f
#define CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX     90.0f
#define CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN    -90.0f

#define CSGO_ANIM_LOWER_CATCHUP_IDLE             100.0f
#define CSGO_ANIM_LOWER_CATCHUP_WITHIN           3.0f
#define CSGO_ANIM_LOWER_REALIGN_DELAY            1.1f
#define CSGO_ANIM_READJUST_THRESHOLD             120.0f
// csgo_playeranimstate.cpp:2370 - idle LBY realign gate: |AngleDiff(foot, eye)| > 35
#define CSGO_ANIM_LOWER_REALIGN_THRESHOLD          35.0f
// csgo_playeranimstate.cpp:2356 - velocity above which the foot yaw is pulled to the eye
#define CSGO_ANIM_VELOCITY_MOVING_GATE              0.1f
// csgo_playeranimstate.cpp:2358 / 2367 - foot yaw approach rates
#define CSGO_ANIM_FOOT_APPROACH_MOVING_BASE        30.0f
#define CSGO_ANIM_FOOT_APPROACH_MOVING_WALKRUN     20.0f

#define CSGO_ANIM_AIM_NARROW_WALK                0.8f
#define CSGO_ANIM_AIM_NARROW_RUN                 0.5f
#define CSGO_ANIM_AIM_NARROW_CROUCHMOVING        0.5f

#define EIGHT_WAY_WIDTH                          22.5f
// cs_shareddefs.cpp: CS_PLAYER_SPEED_RUN = 260.0f, _STOPPED = 1.0f,
// _WALK_MODIFIER = 0.52f, _DUCK_MODIFIER = 0.34f
#define CS_PLAYER_SPEED_STOPPED                  1.0f   // csgo_playeranimstate.cpp:2380
#define CS_PLAYER_SPEED_RUN                      260.0f
#define CS_PLAYER_SPEED_WALK_MODIFIER            0.52f
#define CS_PLAYER_SPEED_DUCK_MODIFIER            0.34f

// verified offsets within CCSGOPlayerAnimState (x86)
constexpr ptrdiff_t AS_OFFSET_LAST_UPDATE_TIME      = 0x5C;
constexpr ptrdiff_t AS_OFFSET_LAST_UPDATE_FRAME     = 0x60;   // int
constexpr ptrdiff_t AS_OFFSET_LAST_UPDATE_INCREMENT = 0x64;   // float
constexpr ptrdiff_t AS_OFFSET_EYE_YAW               = 0x68;
constexpr ptrdiff_t AS_OFFSET_EYE_PITCH             = 0x6C;
constexpr ptrdiff_t AS_OFFSET_FOOT_YAW              = 0x70;
constexpr ptrdiff_t AS_OFFSET_FOOT_YAW_LAST         = 0x74;
constexpr ptrdiff_t AS_OFFSET_MOVE_YAW              = 0x78;
constexpr ptrdiff_t AS_OFFSET_MOVE_YAW_IDEAL        = 0x7C;
constexpr ptrdiff_t AS_OFFSET_MOVE_YAW_CURRENT_TO_IDEAL = 0x80;   // m_flMoveYawToIdeal in your old naming
constexpr ptrdiff_t AS_OFFSET_TIME_TO_ALIGN_LOWER   = 0x84;
constexpr ptrdiff_t AS_OFFSET_PRIMARY_CYCLE         = 0x88;
constexpr ptrdiff_t AS_OFFSET_MOVE_WEIGHT           = 0x8C;
constexpr ptrdiff_t AS_OFFSET_MOVE_WEIGHT_SMOOTHED  = 0x90;
constexpr ptrdiff_t AS_OFFSET_ANIM_DUCK_AMOUNT      = 0x94;
constexpr ptrdiff_t AS_OFFSET_DUCK_ADDITIONAL       = 0x98;
constexpr ptrdiff_t AS_OFFSET_RE_CROUCH_WEIGHT      = 0x9C;
constexpr ptrdiff_t AS_OFFSET_POS_CURRENT           = 0xA0;   // vec3
constexpr ptrdiff_t AS_OFFSET_POS_LAST              = 0xAC;   // vec3
constexpr ptrdiff_t AS_OFFSET_VELOCITY              = 0xB8;   // vec3
constexpr ptrdiff_t AS_OFFSET_VELOCITY_NORMALIZED   = 0xC4;   // vec3
constexpr ptrdiff_t AS_OFFSET_VELOCITY_LENGTH_XY    = 0xD4;
constexpr ptrdiff_t AS_OFFSET_VELOCITY_LENGTH_Z     = 0xD8;
constexpr ptrdiff_t AS_OFFSET_SPEED_AS_PORTION_RUN  = 0xDC;
constexpr ptrdiff_t AS_OFFSET_SPEED_AS_PORTION_WALK = 0xE0;
constexpr ptrdiff_t AS_OFFSET_SPEED_AS_PORTION_CROUCH = 0xE4;
constexpr ptrdiff_t AS_OFFSET_DURATION_MOVING       = 0xE8;
constexpr ptrdiff_t AS_OFFSET_DURATION_STILL        = 0xEC;
constexpr ptrdiff_t AS_OFFSET_ON_GROUND             = 0xF0;   // bool
constexpr ptrdiff_t AS_OFFSET_LANDING               = 0xF1;   // bool
constexpr ptrdiff_t AS_OFFSET_WALK_TO_RUN_TRANSITION = 0x100;
constexpr ptrdiff_t AS_OFFSET_LANDED_THIS_FRAME     = 0x104;  // bool
constexpr ptrdiff_t AS_OFFSET_LEFT_GROUND_THIS_FRAME = 0x105; // bool
constexpr ptrdiff_t AS_OFFSET_AIM_YAW_MIN           = 0x18C;
constexpr ptrdiff_t AS_OFFSET_AIM_YAW_MAX           = 0x190;
constexpr ptrdiff_t AS_OFFSET_AIM_PITCH_MIN         = 0x194;
constexpr ptrdiff_t AS_OFFSET_AIM_PITCH_MAX         = 0x198;

// =============================================================================
// helpers that exactly mirror 2018 source
// =============================================================================

__forceinline float ClampCycle(float flCycleIn)
{
    flCycleIn -= int(flCycleIn);
    if (flCycleIn < 0) flCycleIn += 1;
    else if (flCycleIn > 1) flCycleIn -= 1;
    return flCycleIn;
}

__forceinline float AngleDiff(float destAngle, float srcAngle)
{
    float delta = fmodf(destAngle - srcAngle, 360.0f);
    if (destAngle > srcAngle)
    {
        if (delta >= 180)
            delta -= 360;
    }
    else
    {
        if (delta <= -180)
            delta += 360;
    }
    return delta;
}

__forceinline float AngleNormalize(float angle)
{
    angle = fmodf(angle, 360.0f);
    if (angle > 180) angle -= 360;
    if (angle < -180) angle += 360;
    return angle;
}


__forceinline float ApproachAngle(float target, float value, float speed)
{
    float delta = AngleDiff(target, value);
    if (speed < 0) speed = -speed;
    if (delta > speed) value += speed;
    else if (delta < -speed) value -= speed;
    else value = target;
    return AngleNormalize(value);
}

__forceinline float Lerp(float t, float a, float b)
{
    return a + (b - a) * t;
}

// csgo_playeranimstate.cpp:2332-2337 - narrow the aim matrix as speed and duck go up
__forceinline float CalcAimMatrixWidthRange(CCSGOPlayerAnimState* state)
{
    if (!state) return 1.0f;

    float width = Lerp(
        state->m_flSpeedAsPortionOfWalkTopSpeed < 0.f ? 0.f :
        (state->m_flSpeedAsPortionOfWalkTopSpeed > 1.f ? 1.f : state->m_flSpeedAsPortionOfWalkTopSpeed),
        1.0f,
        Lerp(state->m_flWalkToRunTransition, CSGO_ANIM_AIM_NARROW_WALK, CSGO_ANIM_AIM_NARROW_RUN));

    if (state->m_flAnimDuckAmount > 0.f)
    {
        float crouch = state->m_flSpeedAsPortionOfCrouchTopSpeed;
        crouch = crouch < 0.f ? 0.f : (crouch > 1.f ? 1.f : crouch);
        width = Lerp(state->m_flAnimDuckAmount * crouch, width, CSGO_ANIM_AIM_NARROW_CROUCHMOVING);
    }

    return width;
}

// csgo_playeranimstate.cpp:2339-2349 - the foot yaw is pulled back into the aim
// matrix with a bound PER SIDE: eye - abs(m_flAimYawMax * width) on the left and
// eye + abs(m_flAimYawMin * width) on the right. m_flAimYawMin/Max are themselves
// Lerp'd by the duck amount and the aim-blend pose params (2181-2186), so they are
// not simply +/-58 and must not be collapsed into one symmetric value.
__forceinline float ClampFootYawToAimMatrix(float eye_yaw, float foot_yaw,
                                           float yaw_max, float yaw_min, float width)
{
    const float temp_yaw_max = yaw_max * width;
    const float temp_yaw_min = yaw_min * width;
    const float eye_foot_delta = AngleDiff(eye_yaw, foot_yaw);

    if (eye_foot_delta > temp_yaw_max)
        foot_yaw = eye_yaw - fabsf(temp_yaw_max);
    else if (eye_foot_delta < temp_yaw_min)
        foot_yaw = eye_yaw + fabsf(temp_yaw_min);

    return AngleNormalize(foot_yaw);
}


// ---------------------------------------------------------------------------
// Animstate probe clock (proven): csgo_playeranimstate.cpp:248-253.
// Update() early-outs unless curtime or framecount moved, and derives
// m_flLastUpdateIncrement from the curtime delta - so a probe has to move the
// clock and restore it afterwards. Pair with `as->m_nLastUpdateFrame--` to
// satisfy the framecount term (the frame counter itself is not touched).
// ---------------------------------------------------------------------------
struct anim_clock_save_t
{
    float curtime  = 0.f;
    float interval = 0.f;
    bool  saved    = false;
};

inline anim_clock_save_t g_anim_clock_save{ };

inline void ApplyClock( float probe_time, float interval )
{
    if( !g_csgo.m_globals )
        return;

    g_anim_clock_save.curtime  = g_csgo.m_globals->m_curtime;
    g_anim_clock_save.interval = g_csgo.m_globals->m_interval;
    g_anim_clock_save.saved    = true;

    g_csgo.m_globals->m_curtime  = probe_time;
    g_csgo.m_globals->m_interval = interval;
}

inline void RestoreClock( )
{
    if( !g_anim_clock_save.saved || !g_csgo.m_globals )
        return;

    g_csgo.m_globals->m_curtime  = g_anim_clock_save.curtime;
    g_csgo.m_globals->m_interval = g_anim_clock_save.interval;
    g_anim_clock_save.saved      = false;
}

// convert horizontal velocity to angular yaw (SetUpVelocity, line ~2394)
__forceinline float VelocityToYaw(const vec3_t& velocity)
{
    if (velocity.length_2d() <= 0.1f)
        return 0.f;
    float yaw = atan2f(-velocity.y, -velocity.x) * (180.f / math::pi);
    if (yaw < 0.f) yaw += 360.f;
    return yaw;
}

// exactly as source does in SetUpVelocity after computing flRawYawIdeal
__forceinline float CalcMoveYawIdeal(float flRawYawIdeal, float flFootYaw)
{
    return AngleNormalize(AngleDiff(flRawYawIdeal, flFootYaw));
}

// delta between current move yaw and ideal (source line ~2448)
__forceinline float CalcMoveYawCurrentToIdeal(float flMoveYawIdeal, float flMoveYaw)
{
    return AngleNormalize(AngleDiff(flMoveYawIdeal, flMoveYaw));
}

// =============================================================================
// FootYaw clamping — verified against source lines 2327-2356
// =============================================================================
__forceinline void ClampFootYaw(CCSGOPlayerAnimState* state, float flEyeYaw)
{
    // line 2327
    state->m_flFootYawLast = state->m_flFootYaw;

    // line 2328
    state->m_flFootYaw = std::clamp(state->m_flFootYaw, -360.f, 360.f);

    // line 2329
    float flEyeFootDelta = AngleDiff(flEyeYaw, state->m_flFootYaw);

    // line 2332 — narrow aim matrix width as speed increases
    float flAimMatrixWidthRange = Lerp(
        std::clamp(state->m_flSpeedAsPortionOfWalkTopSpeed, 0.f, 1.f),
        1.0f,
        Lerp(state->m_flWalkToRunTransition, CSGO_ANIM_AIM_NARROW_WALK, CSGO_ANIM_AIM_NARROW_RUN)
    );

    // line 2334-2337 — crouch modifier
    if (state->m_flAnimDuckAmount > 0.f)
    {
        flAimMatrixWidthRange = Lerp(
            state->m_flAnimDuckAmount * std::clamp(state->m_flSpeedAsPortionOfCrouchTopSpeed, 0.f, 1.f),
            flAimMatrixWidthRange,
            CSGO_ANIM_AIM_NARROW_CROUCHMOVING
        );
    }

    // line 2339-2340
    float flTempYawMax = state->m_flAimYawMax * flAimMatrixWidthRange;
    float flTempYawMin = state->m_flAimYawMin * flAimMatrixWidthRange;

    // line 2342-2347
    if (flEyeFootDelta > flTempYawMax)
        state->m_flFootYaw = flEyeYaw - fabsf(flTempYawMax);
    else if (flEyeFootDelta < flTempYawMin)
        state->m_flFootYaw = flEyeYaw + fabsf(flTempYawMin);

    // line 2348
    state->m_flFootYaw = AngleNormalize(state->m_flFootYaw);
}

// =============================================================================
// Ground foot-yaw approach — verified against source lines 2351-2378
// =============================================================================
__forceinline void UpdateFootYawOnGround(CCSGOPlayerAnimState* state, float flEyeYaw, float flLowerBodyYawTarget, float flCurtime)
{
    if (!state->m_bOnGround)
        return;

    // lines 2354-2361 — moving
    if (state->m_flVelocityLengthXY > 0.1f)
    {
        state->m_flFootYaw = ApproachAngle(
            flEyeYaw,
            state->m_flFootYaw,
            state->m_flLastUpdateIncrement * (30.0f + 20.0f * state->m_flWalkToRunTransition)
        );

        // server-only lines omitted (LBY timer updates)
    }
    // lines 2363-2378 — idle
    else
    {
        state->m_flFootYaw = ApproachAngle(
            flLowerBodyYawTarget,
            state->m_flFootYaw,
            state->m_flLastUpdateIncrement * CSGO_ANIM_LOWER_CATCHUP_IDLE
        );

        // server-only realign timer check omitted
    }
}
