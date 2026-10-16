#pragma once
// =============================================================================
// animfix_v2.h — upgraded animation fix
//
// LOCAL PLAYER: same as animfix.h (rebuild animstate with fake angles,
//   fix pose params, fix layers, velocity spoof, bone rebuild)
//
// ENEMY PLAYERS: new — reconstructs animstate per-tick using A1-A5 math,
//   feeds resolved yaw into bone matrices via resolver_v2 C8 pipeline.
//
// Shares math with resolver + anti-aim via animstate_verify.h.
// =============================================================================

#include "animstate_verify.h"
#include "anti_aim_v2.h"

// forward
class Resolver;

// =============================================================================
// enemy animstate reconstruction record — one per player
// =============================================================================
struct enemy_anim_record_t
{
    // reconstructed from networked data + A1-A5 math
    float  m_foot_yaw           = 0.f;
    float  m_foot_yaw_last      = 0.f;
    float  m_eye_yaw            = 0.f;
    float  m_eye_pitch          = 0.f;
    float  m_lby                = 0.f;
    float  m_move_yaw           = 0.f;
    float  m_velocity_length_xy = 0.f;
    float  m_duck_amount        = 0.f;
    float  m_walk_to_run        = 0.f;
    float  m_speed_walk         = 0.f;
    float  m_speed_run          = 0.f;
    float  m_speed_crouch       = 0.f;
    float  m_max_delta          = 58.f;
    bool   m_on_ground          = false;
    bool   m_moving             = false;

    // simtime tracking
    float  m_simtime            = 0.f;
    float  m_old_simtime        = 0.f;
    int    m_choked_ticks       = 0;

    // resolved output (filled by resolver)
    float  m_resolved_foot_yaw  = 0.f;
    bool   m_resolved           = false;

    // layer snapshots for detection
    C_AnimationLayer m_layers[ANIMATION_LAYER_COUNT];

    // bone matrix built with resolved yaw
    bool   m_bones_valid        = false;

    void Reset() { *this = enemy_anim_record_t{}; }
};

// =============================================================================
// Animation fix configuration
// =============================================================================
struct animfix_config_t
{
    bool m_enabled = true;
    bool m_rebuild_animstate = true;
    bool m_fix_pose_params = true;
    bool m_fix_body_yaw = true;         // POSE_BODY_YAW
    bool m_fix_move_yaw = true;         // POSE_MOVE_YAW
    bool m_fix_speed = false;           // POSE_SPEED (for fake walk)
    bool m_fix_layers = true;
    bool m_spoof_velocity = true;       // for fake walk
    bool m_fix_jump_fall = true;
    bool m_fix_simtime = true;          // 8-bit simtime handling
    bool m_update_clientside = true;    // UpdateClientSideAnimation
    bool m_interpolate = true;          // fix interpolation
    bool m_local_animation = true;      // rebuild local animdata
    bool m_rebuild_bones = true;        // rebuild bone matrices with fake angles
    bool m_fix_thirdperson = true;      // fix thirdperson camera angle
    bool m_fix_local_interp = true;     // fix local player interpolation

    // ---- new: enemy animfix ----
    bool m_enemy_animfix = true;        // enable enemy animstate reconstruction
    bool m_enemy_resolve_bones = true;  // build enemy bones with resolved yaw

    // ---- layer suppression (moved from old HVH cfg) ----
    bool m_suppress_979 = true;         // suppress seq 979 on adjust + alive loop
    bool m_spoof_move_layer = true;     // spoof move layer weight when zero
    bool m_adaptive_air = false;        // adaptive air layer scaling
    bool m_visualize_lby = false;       // paint LBY debug overlay
};

// =============================================================================
// Saved animstate for rebuild
// =============================================================================
struct animstate_backup_t
{
    float m_eye_yaw;
    float m_eye_pitch;
    float m_foot_yaw;
    float m_foot_yaw_last;
    float m_move_yaw;
    float m_move_yaw_ideal;
    float m_move_yaw_current_to_ideal;
    float m_time_to_align_lower;
    float m_primary_cycle;
    float m_move_weight;
    float m_move_weight_smoothed;
    float m_anim_duck_amount;
    float m_duck_additional;
    float m_recrouch_weight;
    vec3_t m_velocity;
    vec3_t m_velocity_normalized;
    float m_velocity_length_xy;
    float m_velocity_length_z;
    float m_speed_as_portion_run;
    float m_speed_as_portion_walk;
    float m_speed_as_portion_crouch;
    float m_duration_moving;
    float m_duration_still;
    bool  m_on_ground;
    bool  m_landing;
    float m_walk_to_run_transition;
    bool  m_landed_this_frame;
    bool  m_left_ground_this_frame;
    float m_aim_yaw_min;
    float m_aim_yaw_max;
    float m_aim_pitch_min;
    float m_aim_pitch_max;
    float m_last_update_time;
    int   m_last_update_frame;
    float m_last_update_increment;

    void Store(CCSGOPlayerAnimState* state);
    void Restore(CCSGOPlayerAnimState* state);
};

// =============================================================================
// Animation layer backup
// =============================================================================
struct layer_backup_t
{
    C_AnimationLayer m_layers[ANIMATION_LAYER_COUNT];
    float            m_pose_param[24];

    void Store(Player* player);
    void Restore(Player* player);
};

// =============================================================================
// Main AnimFix class — upgraded
// =============================================================================
class AnimFix
{
public:
    animfix_config_t m_cfg;

    // state
    bool  m_in_update = false;
    bool  m_should_rebuild = false;
    float m_last_simtime = 0.f;
    int   m_last_tick = 0;
    float m_interp_curtime = 0.f;

    // backups (local player)
    animstate_backup_t m_animstate_backup{};
    layer_backup_t     m_layer_backup{};

    // ---- new: per-enemy reconstruction ----
    enemy_anim_record_t m_enemy_records[65];

    // ---- core ----
    void Init();
    void OnCreateMove(CUserCmd* cmd, bool* send_packet);
    void OnFrameStageNotify(Stage_t stage);
    void OnPaintTraverse();

    // ---- local player animstate rebuild ----
    void RebuildAnimState(Player* player, float eye_yaw, float eye_pitch);
    void FixPoseParameters(Player* player, float real_yaw, float fake_yaw);
    void FixAnimationLayers(Player* player);
    void SpoofVelocityForAnimState(Player* player, const vec3_t& spoofed_vel);
    void FixJumpFallLayers(Player* player);
    void FixSimulationTime(Player* player);
    void UpdateClientSideAnimationFix(Player* player);

    // local player specific
    void DoLocalAnimFix(CUserCmd* cmd, bool* send_packet);
    void InterpolatePlayer(Player* player);

    // sophisticated features (local)
    void RebuildBoneMatrices(Player* player, float fake_yaw, float fake_pitch);
    void FixThirdPersonCamera(Player* player);
    void FixLocalInterpolation(Player* player);
    void SetupBonesFix(Player* player, matrix3x4_t* bone_to_world);

    // ---- NEW: enemy player animstate reconstruction ----
    // called from FRAME_NET_UPDATE_POSTDATAUPDATE_START for each enemy
    void OnEnemyAnimUpdate(Player* player, int index);

    // reconstruct enemy foot yaw from networked animstate + A1-A5 math
    void ReconstructEnemyAnimState(Player* player, int index);

    // build enemy bones with resolved yaw — calls g_resolver.SetupBonesForShot
    // if resolver has a valid read, otherwise falls back to reconstructed foot yaw
    bool SetupEnemyBones(Player* player, int index, matrix3x4_t* out_matrix);

    // feed reconstructed animstate into lag record
    void StoreEnemyRecord(Player* player, int index);

    // ---- helpers ----
    static float BodyYawToPoseParam(float body_yaw);   // convert [-58..58] to [-1..1]
    static float PoseParamToBodyYaw(float pose);       // convert [-1..1] to [-58..58]
    static float MoveYawToPoseParam(float move_yaw);   // convert [-180..180] to [-1..1]
    static void  SetLayerSequenceWeight(C_AnimationLayer& layer, int seq, float weight, float cycle);

    // menu integration
    void ReadMenuConfig();
};

extern AnimFix g_animfix;
