#pragma once
// ─────────────────────────────────────────────
//  STARSHINE ENGINE — FLUDD System (Sunshine)
//
//  F.L.U.D.D. (Flash Liquidizer Ultra Dousing Device)
//  is the central mechanic of Super Mario Sunshine.
//  This module handles:
//
//  Nozzles:
//    - Squirt:  standard directional water jet
//    - Hover:   sustained upward thrust, slows fall
//    - Rocket:  brief explosive upward launch
//    - Turbo:   high-speed horizontal propulsion
//
//  Physics effects:
//    - Recoil force on Mario (opposite spray direction)
//    - Water projectiles for goop cleaning
//    - Momentum carried between Hover and air states
//
//  The goop (pollution) system is separate but driven
//  by water contact — cleaned tiles tracked per-stage.
// ─────────────────────────────────────────────

#include "../core_types.h"
#include <vector>

namespace Starshine {
namespace Actor {

// ── Nozzle types ───────────────────────────────
enum class FluddNozzle {
    None,
    Squirt,   // standard — fires a water jet in aim direction
    Hover,    // sustained hover with water thrust
    Rocket,   // stored charge → explosive upward launch
    Turbo,    // forward dash with sustained spray
};

// ── Water projectile ───────────────────────────
struct WaterParticle {
    Vec3  position;
    Vec3  velocity;
    f32   life;        // seconds remaining
    f32   radius;      // collision radius for goop cleaning
    bool  active = true;

    void update(f32 dt) {
        velocity.y -= 15.f * dt;   // gravity on water
        position   += velocity * dt;
        life       -= dt;
        if(life <= 0.f || position.y < -50.f) active = false;
    }
};

// ── FLUDD state ────────────────────────────────
class FLUDD {
public:
    explicit FLUDD(FluddNozzle nozzle = FluddNozzle::Squirt)
        : m_nozzle(nozzle) {}

    FluddNozzle nozzle()     const { return m_nozzle; }
    f32         water()      const { return m_water; }    // [0-1]
    bool        isEmpty()    const { return m_water <= 0.f; }
    bool        isFiring()   const { return m_firing; }
    bool        isCharging() const { return m_charging; }

    // Set nozzle (when player collects a nozzle upgrade)
    void setNozzle(FluddNozzle n) { m_nozzle = n; m_chargeTimer = 0.f; m_charging = false; }

    // Called every frame with input and Mario's current state
    // Returns recoil force to apply to Mario (in world space)
    Vec3 update(bool triggerHeld, bool triggerPressed, bool triggerReleased,
                const Vec3& aimDir, const Vec3& marioUp, bool marioOnGround,
                f32 dt);

    // Refill (at water source, or when near sea/lake)
    void refill(f32 amount = 1.f) { m_water = std::min(1.f, m_water + amount); }

    // Get active water projectiles (for collision + visual)
    const std::vector<WaterParticle>& particles() const { return m_particles; }
    std::vector<WaterParticle>&       particles()       { return m_particles; }

    // Tick all particles
    void updateParticles(f32 dt) {
        for(auto& p : m_particles) p.update(dt);
        m_particles.erase(
            std::remove_if(m_particles.begin(), m_particles.end(),
                [](const WaterParticle& p){ return !p.active; }),
            m_particles.end());
    }

    // Rocket charge [0-1] (only relevant for Rocket nozzle)
    f32 rocketCharge() const { return m_chargeTimer / kRocketChargeTime; }

private:
    FluddNozzle              m_nozzle;
    f32                      m_water        = 1.f;  // tank level [0-1]
    bool                     m_firing       = false;
    bool                     m_charging     = false;
    f32                      m_chargeTimer  = 0.f;
    f32                      m_fireTimer    = 0.f;
    std::vector<WaterParticle> m_particles;

    // Water consumption rates
    static constexpr f32 kSquirtDrain   = 0.08f;  // per second
    static constexpr f32 kHoverDrain    = 0.12f;
    static constexpr f32 kRocketDrain   = 0.35f;  // on launch
    static constexpr f32 kTurboDrain    = 0.10f;
    static constexpr f32 kRocketChargeTime = 1.5f; // seconds to full charge

    // Spawn a water projectile
    void spawnParticle(const Vec3& origin, const Vec3& vel) {
        WaterParticle p;
        p.position = origin;
        p.velocity = vel;
        p.life     = 2.5f;
        p.radius   = 8.f;
        m_particles.push_back(p);
    }
};

// ── FLUDD::update implementation ───────────────
inline Vec3 FLUDD::update(bool triggerHeld, bool triggerPressed, bool triggerReleased,
                           const Vec3& aimDir, const Vec3& marioUp, bool marioOnGround,
                           f32 dt)
{
    Vec3 recoil = {};
    m_firing    = false;

    // Refill slowly on ground (touching water accelerates this in level logic)
    if(marioOnGround) m_water = std::min(1.f, m_water + 0.005f * dt);

    switch(m_nozzle) {
        // ── Squirt ─────────────────────────────
        case FluddNozzle::Squirt: {
            if(triggerHeld && m_water > 0.f) {
                m_firing = true;
                m_water -= kSquirtDrain * dt;
                m_water  = std::max(0.f, m_water);
                m_fireTimer += dt;
                // Spawn a particle burst every ~0.05s
                if(m_fireTimer >= 0.05f) {
                    m_fireTimer = 0.f;
                    Vec3 spawnPos = aimDir * 40.f; // in front of Mario
                    Vec3 vel = aimDir * 200.f;
                    // Slight spray spread
                    spawnParticle(spawnPos, vel);
                }
                // Recoil pushes Mario opposite to aim direction
                recoil = -aimDir * 4.f;
            } else {
                m_fireTimer = 0.f;
            }
            break;
        }

        // ── Hover ──────────────────────────────
        case FluddNozzle::Hover: {
            if(triggerHeld && !marioOnGround && m_water > 0.f) {
                m_firing = true;
                m_water -= kHoverDrain * dt;
                m_water  = std::max(0.f, m_water);
                // Strong upward thrust
                recoil = marioUp * 22.f;
                // Also fires water downward for visual
                if(dt > 0) {
                    Vec3 downVel = -marioUp * 120.f;
                    spawnParticle(Vec3{}, downVel);
                }
            }
            break;
        }

        // ── Rocket ─────────────────────────────
        case FluddNozzle::Rocket: {
            if(triggerHeld && marioOnGround) {
                m_charging = true;
                m_chargeTimer += dt;
                m_chargeTimer  = std::min(m_chargeTimer, kRocketChargeTime);
            }
            if(triggerReleased && m_charging) {
                m_charging = false;
                if(m_water >= kRocketDrain) {
                    f32  pct    = m_chargeTimer / kRocketChargeTime;
                    f32  power  = 40.f + pct * 60.f;
                    recoil      = marioUp * power;
                    m_water    -= kRocketDrain;
                    m_firing    = true;
                    // Big water burst downward
                    for(int i=0;i<8;i++) {
                        f32 angle = (f32)i / 8.f * k2PI;
                        Vec3 spreadDir = {cosf(angle)*0.4f, -1.f, sinf(angle)*0.4f};
                        spawnParticle(Vec3{}, spreadDir.normalized() * 180.f);
                    }
                }
                m_chargeTimer = 0.f;
            }
            break;
        }

        // ── Turbo ──────────────────────────────
        case FluddNozzle::Turbo: {
            if(triggerHeld && m_water > 0.f) {
                m_firing = true;
                m_water -= kTurboDrain * dt;
                m_water  = std::max(0.f, m_water);
                // Strong forward thrust (aimDir should be Mario's facing)
                recoil = aimDir * 35.f;
            }
            break;
        }

        default: break;
    }

    updateParticles(dt);
    return recoil;
}

// ── Goop tracker ──────────────────────────────
// Tracks which tiles of pollution have been cleaned per-stage.
// In the real game this is a 3D grid overlaid on terrain.
struct GoopGrid {
    std::vector<bool> cells;  // true = clean
    u32 width = 0, height = 0;
    f32 cellSize = 50.f;      // world units per cell
    Vec3 origin  = {};

    void init(u32 w, u32 h, f32 cs, Vec3 orig) {
        width=w; height=h; cellSize=cs; origin=orig;
        cells.assign(w*h, false);
    }

    // Clean all cells within radius r of world-space point p
    void clean(const Vec3& p, f32 r) {
        int cx = (int)((p.x - origin.x) / cellSize);
        int cz = (int)((p.z - origin.z) / cellSize);
        int rCells = (int)(r / cellSize) + 1;
        for(int z=cz-rCells; z<=cz+rCells; z++)
            for(int x=cx-rCells; x<=cx+rCells; x++) {
                if(x<0||z<0||x>=(int)width||z>=(int)height) continue;
                cells[z*width+x] = true;
            }
    }

    f32 percentCleaned() const {
        if(cells.empty()) return 0.f;
        u32 c=0; for(bool b:cells) if(b) c++;
        return (f32)c / (f32)cells.size();
    }
};

} // namespace Actor
} // namespace Starshine
