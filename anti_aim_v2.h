#pragma once
// =============================================================================
// anti_aim_v2.h — REWRITTEN: real desync via send/choke split
//
// Architecture:
//   choked cmd = real yaw   (where you aim, what bones resolve to)
//   sent cmd   = fake yaw   (what the enemy renderer draws)
//   break tick = choked real (micro-move OFF during break window)
//
// 2018 timer from SetUpVelocity:
//   moving (speed > 0.1):  timer = curtime + 1.1 * 0.2,  LBY = eyeYaw
//   standing, timer due:   timer = curtime + 1.1,         LBY = eyeYaw
//
// Build switch (m_micro_build):
//   true  = micro-move build: ApplyMicroMove when standing, BreakDue
//           only refreshes the timer — never fires.
//   false = break build: BreakDue fires as written, ApplyMicroMove never runs.
//
// Shares math with resolver via animstate_verify.h (A1–A5).
// =============================================================================

#include <vector>
#include "animstate_verify.h"

class AntiAimV2
{
public:
    // ---- state ----
    bool  m_active            = false;
    int   m_mode              = 0;    // 1=STAND, 2=WALK, 3=AIR
    int   m_side              = 1;    // +1 right desync, -1 left
    float m_fake_offset       = 180.f; // fake = view + offset (backwards default)
    bool  m_lby_breaking      = false;

    // LBY break state — 2018 timer
    float m_next_break        = 0.f;  // next time LBY realigns

    // batch choke (B5)
    bool  m_force_choke       = false;

    // side stability — hold for 0.3s minimum, no noisy per-tick flipping
    float m_side_set_time     = 0.f;
    float m_side_min_hold     = 0.3f;

    // air
    int   m_air_ticks         = 0;

    // ---- build switch ----
    // true  = micro-move build (ApplyMicroMove runs, BreakDue refreshes only)
    // false = break build (BreakDue fires, ApplyMicroMove never called)
    bool  m_micro_build       = true;

    // ---- interface ----
    void Run(CUserCmd* cmd, bool* send_packet);
    static void CorrectMovement(CUserCmd* cmd, float old_yaw, float new_yaw);
    void Reset();

private:
    // core yaw computations — the whole desync lives here
    float FakeYaw(float view_yaw);
    float RealYaw(float view_yaw);

    // 2018 timer logic
    bool  BreakDue(float curtime, bool on_ground, float speed);

    // B3: micro-move — ONLY on non-break standing ticks
    void  ApplyMicroMove(CUserCmd* cmd);

    // B6: freestand side pick (stable, not per-tick)
    int   PickSide(Player* target, float eye_yaw);

    // aim matrix max delta
    float GetMaxDelta();

    // find nearest enemy
    Player* FindTarget();
};

extern AntiAimV2 g_antiaim_v2;
