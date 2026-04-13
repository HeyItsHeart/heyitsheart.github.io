#pragma once
// ─────────────────────────────────────────────
//  STARSHINE ENGINE — Mario Movement Controller
//
//  Implements Mario's core movement state machine,
//  shared between Sunshine and Galaxy with per-game
//  overrides.
//
//  State machine covers:
//    Idle → Walk → Run → Jump → DoubleJump →
//    TripleJump → Backflip → LongJump →
//    WallKick → Crouch → Slide → Swim →
//    ... and Sunshine-specific: FLUDD hover,
//        spin jump, side somersault
//    ... and Galaxy-specific: spin attack,
//        rolling, pull stars, launch stars
//
//  Physics are gravity-relative: all movement
//  happens in the local gravity frame, then
//  transformed to world space.
// ─────────────────────────────────────────────

#include "../core_types.h"
#include "gravity.h"

namespace Starshine {
namespace Actor {

// ── Input snapshot ─────────────────────────────
struct MarioInput {
    Vec2 stick     = {};       // analog stick [-1,1]
    bool btnJump   = false;    // A button
    bool btnAction = false;    // B (Sunshine: spray, Galaxy: spin)
    bool btnCrouch = false;    // Z (crouch/ground pound)
    bool btnCam    = false;    // C / camera
    // Held vs just-pressed this frame
    bool jumpPressed   = false; // true only on frame of press
    bool actionPressed = false;
    bool crouchPressed = false;
};

// ── Mario states ───────────────────────────────
enum class MarioState : u8 {
    // Ground states
    Idle,
    Walk,
    Run,
    Crouch,
    CrouchIdle,
    Slide,
    Skid,
    // Air states
    Jump,
    DoubleJump,
    TripleJump,
    Backflip,
    LongJump,
    SideFlip,
    WallKick,
    Fall,
    // Water states
    Swim,
    SwimIdle,
    // Damage states
    HitAir,
    HitGround,
    // Sunshine extras
    FLUDD_Hover,
    FLUDD_Rocket,
    SpinJump,
    // Galaxy extras
    SpinAttack,
    PullStar,
    LaunchStar,
    RollBall,
    // Death
    DeadFall,
};

// ── Per-game constants ──────────────────────────
struct MarioGameParams {
    f32 walkAccel      = 8.f;
    f32 runSpeed       = 17.f;
    f32 jumpInitialVY  = 28.f;
    f32 doubleJumpVY   = 32.f;
    f32 tripleJumpVY   = 38.f;
    f32 backflipVY     = 40.f;
    f32 longJumpVY     = 20.f;
    f32 longJumpVXZ    = 22.f;
    f32 wallKickVY     = 30.f;
    f32 gravityAir     = -25.f;
    f32 gravitySlide   = -18.f;
    f32 maxFallSpeed   = -75.f;
    f32 groundFriction = 0.85f;
    f32 airControl     = 0.4f;  // horizontal control in air [0-1]
    // Sunshine-specific
    f32 fluddHoverVY   = 2.f;
    f32 fluddRocketVY  = 60.f;
    // Galaxy-specific
    f32 spinLaunchVY   = 15.f;
    f32 spinAttackRange= 80.f;

    static MarioGameParams forSunshine() {
        MarioGameParams p;
        p.runSpeed = 16.5f;
        p.gravityAir = -24.f;
        return p;
    }
    static MarioGameParams forGalaxy() {
        MarioGameParams p;
        p.gravityAir = kGalaxyGravity;
        p.runSpeed = 17.5f;
        return p;
    }
};

// ── Mario physics body ─────────────────────────
struct MarioPhysics {
    Vec3 position   = {};
    Vec3 velocity   = {};          // world-space velocity
    Vec3 up         = {0,1,0};     // current up vector (gravity-relative)
    f32  facingYaw  = 0.f;         // angle in local XZ plane (radians)
    bool onGround   = false;
    bool onWall     = false;
    bool inWater    = false;
    f32  groundY    = 0.f;         // Y position of ground contact
    Vec3 groundNormal = {0,1,0};
    f32  slopeAngle   = 0.f;       // angle of slope in degrees
};

// ── The Mario controller ───────────────────────
class MarioController {
public:
    explicit MarioController(Game game) : m_game(game) {
        m_params = (game == Game::Sunshine) ?
            MarioGameParams::forSunshine() : MarioGameParams::forGalaxy();
    }

    void setGravityManager(Physics::GravityManager* gm) { m_gravity = gm; }

    // Called every frame with delta time in seconds
    void update(const MarioInput& input, f32 dt);

    // Access current state
    MarioState         state()    const { return m_state; }
    const MarioPhysics& physics() const { return m_phys; }
    MarioPhysics&      physics()        { return m_phys; }

    // How many jumps have been chained this sequence
    int jumpCount() const { return m_jumpCount; }

    // FLUDD water amount [0-1] (Sunshine only)
    f32 fluddWater() const { return m_fluddWater; }

    // Galaxy spin cooldown timer
    f32 spinCooldown() const { return m_spinCooldown; }

    bool isOnGround() const { return m_phys.onGround; }
    bool isInAir()    const { return !m_phys.onGround && !m_phys.inWater; }

    // Teleport to position (used by level loading)
    void setPosition(const Vec3& p) { m_phys.position = p; }
    void setFacing(f32 yaw)         { m_phys.facingYaw = yaw; }

    // Force-set gravity up vector (used when entering new gravity zone)
    void setUp(const Vec3& up) { m_phys.up = up.normalized(); }

private:
    Game               m_game;
    MarioGameParams    m_params;
    Physics::GravityManager* m_gravity = nullptr;

    MarioPhysics       m_phys;
    MarioState         m_state       = MarioState::Idle;
    MarioState         m_prevState   = MarioState::Idle;
    f32                m_stateTimer  = 0.f;   // time in current state

    int                m_jumpCount   = 0;     // 0,1,2 for triple jump
    f32                m_jumpWindow  = 0.f;   // time window to chain jumps
    f32                m_fluddWater  = 1.f;   // [0-1]
    f32                m_spinCooldown= 0.f;   // seconds until next spin
    bool               m_spinUsed    = false; // used spin in this air phase
    Vec3               m_lastWallNormal = {};

    void setState(MarioState s) {
        m_prevState  = m_state;
        m_state      = s;
        m_stateTimer = 0.f;
    }

    // ── State handlers ──────────────────────────
    void updateGravity(f32 dt);
    void updateGround(const MarioInput& in, f32 dt);
    void updateAir(const MarioInput& in, f32 dt);
    void updateSlide(const MarioInput& in, f32 dt);
    void updateSwim(const MarioInput& in, f32 dt);
    void updateFLUDD(const MarioInput& in, f32 dt);
    void updateGalaxySpin(const MarioInput& in, f32 dt);

    // ── Helpers ────────────────────────────────
    // Convert stick input to world-space horizontal direction
    Vec3 stickToWorld(const Vec2& stick, const Vec3& camForward) const;

    // Apply slope physics
    void applySlope(f32 dt);

    // Check jump input and transition state
    bool tryJump(const MarioInput& in);

    // Detect wall collision normal (simplified — real engine uses collision mesh)
    // Returns true if we hit a wall this frame
    bool detectWallCollision(Vec3& outNormal);

    // Ground snap: push Mario to floor if slightly above it
    void snapToGround();

    // Rotate velocity/facing toward move direction
    void steerToward(const Vec3& dir, f32 turnRate, f32 dt);
};

// ── Implementation ─────────────────────────────

inline void MarioController::update(const MarioInput& input, f32 dt) {
    m_stateTimer += dt;

    // 1. Update gravity direction from gravity manager
    if(m_gravity) {
        Vec3 newUp = m_gravity->calcUp(m_phys.position);
        // Smoothly blend toward new up vector (feels natural)
        m_phys.up = Vec3::lerp(m_phys.up, newUp, 1.f - expf(-10.f * dt)).normalized();
    }

    // 2. Apply gravity to velocity (in local up frame)
    updateGravity(dt);

    // 3. State-specific logic
    switch(m_state) {
        case MarioState::Idle:
        case MarioState::Walk:
        case MarioState::Run:
        case MarioState::Crouch:
        case MarioState::CrouchIdle:
        case MarioState::Skid:
            updateGround(input, dt);
            break;
        case MarioState::Slide:
            updateSlide(input, dt);
            break;
        case MarioState::Jump:
        case MarioState::DoubleJump:
        case MarioState::TripleJump:
        case MarioState::Backflip:
        case MarioState::LongJump:
        case MarioState::SideFlip:
        case MarioState::WallKick:
        case MarioState::Fall:
            updateAir(input, dt);
            break;
        case MarioState::Swim:
        case MarioState::SwimIdle:
            updateSwim(input, dt);
            break;
        default:
            break;
    }

    // 4. Game-specific updates
    if(m_game == Game::Sunshine)
        updateFLUDD(input, dt);
    else
        updateGalaxySpin(input, dt);

    // 5. Integrate position
    m_phys.position += m_phys.velocity * dt;

    // 6. Ground contact check (simplified — real impl uses collision mesh)
    snapToGround();

    // 7. Decay timers
    if(m_jumpWindow > 0.f) m_jumpWindow -= dt;
    if(m_spinCooldown > 0.f) m_spinCooldown -= dt;
}

inline void MarioController::updateGravity(f32 dt) {
    // Gravity acts along the local -up axis
    f32 gravStrength = m_phys.inWater ? m_params.gravityAir * 0.3f : m_params.gravityAir;
    if(m_state == MarioState::FLUDD_Hover) gravStrength *= 0.1f;
    if(m_phys.onGround) return; // no gravity accumulation when grounded
    m_phys.velocity += (-m_phys.up) * fabsf(gravStrength) * dt;
    // Clamp fall speed
    f32 fallComp = m_phys.velocity.dot(-m_phys.up);
    if(fallComp > fabsf(m_params.maxFallSpeed)) {
        m_phys.velocity -= (-m_phys.up) * (fallComp - fabsf(m_params.maxFallSpeed));
    }
}

inline void MarioController::updateGround(const MarioInput& in, f32 dt) {
    // Stick input in world space
    Vec3 camFwd = {sinf(m_phys.facingYaw), 0, cosf(m_phys.facingYaw)};
    Vec3 moveDir = stickToWorld(in.stick, camFwd);

    f32 stickLen = in.stick.length();

    // Jump check
    if(in.jumpPressed) {
        if(in.crouchPressed && !m_phys.onGround) {
            // Ground pound transition (handled elsewhere)
        } else if(stickLen > 0.7f && m_prevState == MarioState::Crouch) {
            // Long jump
            setState(MarioState::LongJump);
            Vec3 fwdWorld = moveDir.normalized();
            m_phys.velocity = fwdWorld * m_params.longJumpVXZ
                            + m_phys.up * m_params.longJumpVY;
            m_jumpCount = 1;
            return;
        } else if(stickLen < 0.1f &&
                  (m_state == MarioState::Idle || m_state == MarioState::CrouchIdle)) {
            // Backflip
            setState(MarioState::Backflip);
            Vec3 backDir = -(Vec3{sinf(m_phys.facingYaw), 0, cosf(m_phys.facingYaw)}
                           .projectOntoPlane(m_phys.up)).normalized();
            m_phys.velocity = backDir * 5.f + m_phys.up * m_params.backflipVY;
            m_jumpCount = 1;
            return;
        } else {
            // Normal jump (single/double/triple chain)
            f32 jumpVY = m_params.jumpInitialVY;
            if(m_jumpWindow > 0.f) {
                if(m_jumpCount == 1) jumpVY = m_params.doubleJumpVY;
                else if(m_jumpCount >= 2) jumpVY = m_params.tripleJumpVY;
            }
            MarioState nextJumpState = (m_jumpCount == 0) ? MarioState::Jump :
                                       (m_jumpCount == 1) ? MarioState::DoubleJump :
                                                            MarioState::TripleJump;
            setState(nextJumpState);
            // Preserve horizontal velocity, replace vertical
            Vec3 horizVel = m_phys.velocity.projectOntoPlane(m_phys.up);
            m_phys.velocity = horizVel + m_phys.up * jumpVY;
            m_jumpCount++;
            m_jumpWindow = 0.5f; // 500ms window for next jump chain
            m_phys.onGround = false;
            return;
        }
    }

    // Crouch transition
    if(in.crouchPressed && stickLen < 0.1f) {
        setState(m_state == MarioState::CrouchIdle ?
                 MarioState::CrouchIdle : MarioState::Crouch);
    }

    // Move
    Vec3 horizVel = m_phys.velocity.projectOntoPlane(m_phys.up);
    f32  speed    = horizVel.length();

    if(stickLen > 0.1f) {
        Vec3 targetDir  = moveDir.normalized();
        f32  targetSpeed = m_params.runSpeed * stickLen;
        // Accelerate
        f32 newSpeed = speed + m_params.walkAccel * dt;
        if(newSpeed > targetSpeed) newSpeed = targetSpeed;
        horizVel = Vec3::lerp(horizVel.normalized(), targetDir, 6.f * dt).normalized()
                   * newSpeed;
        // Update facing
        steerToward(targetDir, 8.f, dt);
        // Transition walk/run states
        if(speed < 6.f) setState(MarioState::Walk);
        else            setState(MarioState::Run);
    } else {
        // Friction deceleration
        horizVel *= powf(m_params.groundFriction, dt * 60.f);
        if(horizVel.length() < 0.5f) {
            horizVel = {};
            setState(MarioState::Idle);
        }
    }

    // Reassemble velocity (preserve up-component for slopes)
    f32 upComp = m_phys.velocity.dot(m_phys.up);
    if(upComp < 0) upComp = 0; // don't pull into ground
    m_phys.velocity = horizVel + m_phys.up * upComp;
}

inline void MarioController::updateAir(const MarioInput& in, f32 dt) {
    // Horizontal air control
    Vec3 camFwd = {sinf(m_phys.facingYaw), 0, cosf(m_phys.facingYaw)};
    Vec3 moveDir = stickToWorld(in.stick, camFwd);
    f32  stickLen = in.stick.length();

    if(stickLen > 0.1f) {
        Vec3 horizVel = m_phys.velocity.projectOntoPlane(m_phys.up);
        Vec3 targetDir = moveDir.normalized();
        Vec3 accel = targetDir * (m_params.walkAccel * m_params.airControl * dt);
        horizVel += accel;
        // Clamp air horizontal speed
        f32 horizSpeed = horizVel.length();
        if(horizSpeed > m_params.runSpeed * 1.1f)
            horizVel = horizVel.normalized() * m_params.runSpeed * 1.1f;
        f32 upComp = m_phys.velocity.dot(m_phys.up);
        m_phys.velocity = horizVel + m_phys.up * upComp;
        steerToward(targetDir, 5.f, dt);
    }

    // Wall kick check
    Vec3 wallNorm;
    if(detectWallCollision(wallNorm) && in.jumpPressed) {
        setState(MarioState::WallKick);
        Vec3 kickDir = (wallNorm + m_phys.up * 0.3f).normalized();
        m_phys.velocity = kickDir * m_params.wallKickVY;
        m_lastWallNormal = wallNorm;
        m_jumpCount = 1;
        m_jumpWindow = 0.3f;
        return;
    }

    // Transition to Fall if going down
    if(m_phys.velocity.dot(m_phys.up) < -2.f &&
       m_state != MarioState::Fall &&
       m_state != MarioState::LongJump)
        setState(MarioState::Fall);

    // Land check handled by snapToGround/collision
}

inline void MarioController::updateSlide(const MarioInput& in, f32 dt) {
    // On steep slopes: Mario slides, limited control
    Vec3 slideDir = (m_phys.groundNormal.cross(m_phys.up)).cross(m_phys.up).normalized();
    m_phys.velocity += slideDir * 15.f * dt;
    f32 speed = m_phys.velocity.length();
    if(speed > 30.f) m_phys.velocity = m_phys.velocity.normalized() * 30.f;
    // Jump out of slide
    if(in.jumpPressed) {
        setState(MarioState::Jump);
        m_phys.velocity += m_phys.up * m_params.jumpInitialVY;
        m_phys.onGround = false;
    }
}

inline void MarioController::updateSwim(const MarioInput& in, f32 dt) {
    // Simplified swim: move in any 3D direction
    Vec3 camFwd = {sinf(m_phys.facingYaw), 0, cosf(m_phys.facingYaw)};
    Vec3 moveDir = stickToWorld(in.stick, camFwd);
    if(in.stick.length() > 0.1f) {
        m_phys.velocity = Vec3::lerp(m_phys.velocity,
                                     moveDir.normalized() * 10.f, 5.f * dt);
    } else {
        m_phys.velocity *= 0.9f;
    }
}

inline void MarioController::updateFLUDD(const MarioInput& in, f32 dt) {
    if(m_game != Game::Sunshine) return;
    // FLUDD hover: hold B in air to slow fall
    if(in.btnAction && isInAir() && m_fluddWater > 0.f) {
        if(m_state != MarioState::FLUDD_Hover)
            setState(MarioState::FLUDD_Hover);
        // Counter gravity partially
        m_phys.velocity += m_phys.up * m_params.fluddHoverVY * dt * 60.f;
        m_fluddWater -= dt * 0.15f;
        if(m_fluddWater < 0) m_fluddWater = 0;
    }
    // Refill when on ground (water source nearby handled by level)
    if(m_phys.onGround) m_fluddWater = std::min(1.f, m_fluddWater + dt * 0.3f);
}

inline void MarioController::updateGalaxySpin(const MarioInput& in, f32 dt) {
    if(m_game == Game::Sunshine) return;
    if(in.actionPressed && m_spinCooldown <= 0.f) {
        setState(MarioState::SpinAttack);
        m_spinCooldown = 0.8f;
        m_spinUsed = true;
        // Small upward boost when spinning in air
        if(isInAir()) {
            f32 upComp = m_phys.velocity.dot(m_phys.up);
            if(upComp < m_params.spinLaunchVY)
                m_phys.velocity += m_phys.up * (m_params.spinLaunchVY - upComp);
        }
    }
    if(m_state == MarioState::SpinAttack && m_stateTimer > 0.4f)
        setState(m_phys.onGround ? MarioState::Idle : MarioState::Fall);
}

inline Vec3 MarioController::stickToWorld(const Vec2& stick, const Vec3& camForward) const {
    if(stick.length() < 0.05f) return {};
    // Build camera-relative basis in the gravity plane
    Vec3 fwd   = camForward.projectOntoPlane(m_phys.up).normalized();
    Vec3 right = fwd.cross(m_phys.up).normalized();
    return (right * stick.x + fwd * (-stick.y));
}

inline void MarioController::steerToward(const Vec3& dir, f32 turnRate, f32 dt) {
    // Project dir onto the gravity plane to get target yaw
    Vec3 flat = dir.projectOntoPlane(m_phys.up);
    if(flat.length() < 0.01f) return;
    flat = flat.normalized();
    f32 targetYaw = atan2f(flat.x, flat.z);
    // Lerp angle
    f32 diff = targetYaw - m_phys.facingYaw;
    // Normalize to [-π, π]
    while(diff > kPI) diff -= k2PI;
    while(diff < -kPI) diff += k2PI;
    m_phys.facingYaw += diff * turnRate * dt;
}

inline bool MarioController::detectWallCollision(Vec3& outNormal) {
    // Stub — real implementation queries collision mesh
    // Returns false until collision is integrated
    return false;
}

inline void MarioController::snapToGround() {
    // Simplified: use a fixed ground plane at Y=0 for now
    // Real implementation will raycast against collision mesh
    f32 groundY = 0.f; // placeholder
    f32 upComp  = m_phys.position.dot(m_phys.up);
    if(upComp <= groundY + 1.f && m_phys.velocity.dot(m_phys.up) <= 0.f) {
        // Project position onto ground
        m_phys.position -= m_phys.up * (upComp - groundY);
        // Zero out downward velocity component
        f32 vc = m_phys.velocity.dot(-m_phys.up);
        if(vc > 0) m_phys.velocity += m_phys.up * vc;
        if(!m_phys.onGround) {
            // Just landed
            m_phys.onGround = true;
            m_jumpCount = 0;
            m_spinUsed  = false;
            if(m_state == MarioState::Fall || m_state == MarioState::Jump ||
               m_state == MarioState::DoubleJump || m_state == MarioState::TripleJump)
                setState(MarioState::Idle);
        }
    } else {
        m_phys.onGround = false;
    }
}

} // namespace Actor
} // namespace Starshine
