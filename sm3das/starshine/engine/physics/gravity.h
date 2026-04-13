#pragma once
// ─────────────────────────────────────────────
//  STARSHINE ENGINE — Galaxy Gravity System
//
//  This is the single most defining feature of
//  Galaxy 1 & 2. Every planet has its own gravity
//  field that re-orients Mario and all physics.
//
//  Gravity types (from Galaxy source research):
//    - Spherical: gravity toward planet center
//    - Cylindrical: gravity toward an axis
//    - Disk: gravity perpendicular to a plane
//    - Point: gravity toward a single point
//    - Global: flat downward (like Sunshine/SM64)
//    - Bazooka/Special: for specific set-pieces
//
//  Mario's "up" vector is continuously updated
//  based on the strongest gravity field he's in.
//  This drives rotation, slope detection, and
//  jump direction.
// ─────────────────────────────────────────────

#include "../core_types.h"
#include <vector>
#include <memory>
#include <optional>

namespace Starshine {
namespace Physics {

// ── Gravity types ──────────────────────────────
enum class GravityType {
    Global,       // flat -Y, used for hub worlds and some planets
    Sphere,       // toward sphere center — most common
    Cylinder,     // toward cylinder axis (tube planets)
    Disk,         // perpendicular to disk plane (flat planets)
    Point,        // toward a single world-space point
    InvSphere,    // away from center (inside-out planets)
    Torus,        // toward torus ring center
};

// ── Priority system (matches Galaxy's own system) ──
// Higher priority overrides lower when Mario is in range
enum class GravityPriority : u8 {
    Lowest   = 0,
    Low      = 64,
    Normal   = 128,
    High     = 192,
    Highest  = 255,
};

// ── Base gravity field ─────────────────────────
struct GravityField {
    GravityType      type;
    GravityPriority  priority    = GravityPriority::Normal;
    f32              strength    = kGalaxyGravity;  // units/s²
    f32              range       = -1.f;  // -1 = infinite
    Vec3             position    = {};    // world-space origin
    Vec3             axis        = {0,1,0}; // for cylinder/disk
    bool             active      = true;

    // Returns the gravity acceleration vector for a point in world space.
    // The returned vector is already scaled by strength.
    // Returns nullopt if the point is outside this field's range.
    virtual std::optional<Vec3> calcGravity(const Vec3& point) const = 0;

    // Returns the "up" direction (opposite of gravity direction) at a point
    std::optional<Vec3> calcUp(const Vec3& point) const {
        auto g = calcGravity(point);
        if(!g) return std::nullopt;
        return (-(*g)).normalized();
    }

    // Distance from field origin (for range checks)
    virtual f32 distanceTo(const Vec3& point) const {
        return (point - position).length();
    }

    bool inRange(const Vec3& point) const {
        if(range < 0) return true;
        return distanceTo(point) <= range;
    }

    virtual ~GravityField() = default;
};

// ── Spherical gravity (most common in Galaxy) ──
// Example: Beehive, Honeybee Planet, most round planets
struct SphereGravity : GravityField {
    f32 radius = 100.f;  // planet surface radius (for distance calc)

    SphereGravity() { type = GravityType::Sphere; }

    std::optional<Vec3> calcGravity(const Vec3& point) const override {
        if(!active) return std::nullopt;
        Vec3 toCenter = position - point;
        f32  dist     = toCenter.length();
        if(range > 0 && dist > range) return std::nullopt;
        if(dist < 0.001f) return Vec3{0, strength, 0};
        // Gravity pulls toward center, scaled by strength
        return toCenter.normalized() * fabsf(strength);
    }

    f32 distanceTo(const Vec3& point) const override {
        f32 d = (point - position).length();
        return d - radius; // distance from surface, not center
    }
};

// ── Inverted sphere (inside-out planets) ────────
// Example: the coconut / bubble planets
struct InvSphereGravity : SphereGravity {
    InvSphereGravity() { type = GravityType::InvSphere; }

    std::optional<Vec3> calcGravity(const Vec3& point) const override {
        if(!active) return std::nullopt;
        Vec3 fromCenter = point - position;
        f32  dist       = fromCenter.length();
        if(range > 0 && dist > range) return std::nullopt;
        if(dist < 0.001f) return Vec3{0, -strength, 0};
        return fromCenter.normalized() * fabsf(strength);
    }
};

// ── Cylindrical gravity ────────────────────────
// Example: Tube planets, cylindrical asteroids
struct CylinderGravity : GravityField {
    f32 height = 200.f; // cylinder half-height along axis

    CylinderGravity() { type = GravityType::Cylinder; }

    std::optional<Vec3> calcGravity(const Vec3& point) const override {
        if(!active) return std::nullopt;
        // Project point onto axis
        Vec3 norm = axis.normalized();
        Vec3 d    = point - position;
        f32  t    = d.dot(norm);
        // Check height bounds
        if(fabsf(t) > height) return std::nullopt;
        // Closest point on axis
        Vec3 onAxis  = position + norm * t;
        Vec3 toAxis  = onAxis - point;
        f32  dist    = toAxis.length();
        if(range > 0 && dist > range) return std::nullopt;
        if(dist < 0.001f) return std::nullopt;
        return toAxis.normalized() * fabsf(strength);
    }
};

// ── Disk / plane gravity ───────────────────────
// Example: flat disc planets, some athletic galaxies
struct DiskGravity : GravityField {
    f32 diskRadius = 150.f;

    DiskGravity() { type = GravityType::Disk; }

    std::optional<Vec3> calcGravity(const Vec3& point) const override {
        if(!active) return std::nullopt;
        Vec3 norm = axis.normalized();
        // Gravity is perpendicular to the disk plane
        f32  side = (point - position).dot(norm);
        // Only pull toward the "inside" face
        if(range > 0 && fabsf(side) > range) return std::nullopt;
        // Check radial distance
        Vec3 radialVec = (point - position) - norm * side;
        if(diskRadius > 0 && radialVec.length() > diskRadius) return std::nullopt;
        // Pull toward plane
        Vec3 dir = (side > 0) ? -norm : norm;
        return dir * fabsf(strength);
    }
};

// ── Global (flat) gravity ──────────────────────
// Sunshine / hub worlds / some Galaxy areas
struct GlobalGravity : GravityField {
    GlobalGravity() { type = GravityType::Global; }

    std::optional<Vec3> calcGravity(const Vec3& /*point*/) const override {
        if(!active) return std::nullopt;
        return axis.normalized() * (-fabsf(strength));
    }
};

// ── Point gravity (pull toward exact point) ────
struct PointGravity : GravityField {
    PointGravity() { type = GravityType::Point; }

    std::optional<Vec3> calcGravity(const Vec3& point) const override {
        if(!active) return std::nullopt;
        Vec3 d = position - point;
        f32  dist = d.length();
        if(range > 0 && dist > range) return std::nullopt;
        if(dist < 0.001f) return std::nullopt;
        return d.normalized() * fabsf(strength);
    }
};

// ── Gravity Manager ────────────────────────────
// Holds all gravity fields in the current level.
// Mario (and any physics object) queries this every frame.
class GravityManager {
public:
    void addField(std::shared_ptr<GravityField> field) {
        m_fields.push_back(std::move(field));
    }

    void removeField(GravityField* field) {
        m_fields.erase(
            std::remove_if(m_fields.begin(), m_fields.end(),
                [field](const auto& p){ return p.get() == field; }),
            m_fields.end());
    }

    void clear() { m_fields.clear(); }

    // Calculate the dominant gravity vector at a point.
    // Picks the highest-priority field that covers this point.
    // Multiple same-priority fields are blended by inverse distance.
    Vec3 calcGravity(const Vec3& point) const {
        Vec3  bestG   = {0, kGalaxyGravity, 0}; // fallback
        u8    bestPri = 0;
        bool  found   = false;

        // Collect all active fields at this point
        struct Candidate { Vec3 g; u8 pri; f32 dist; };
        std::vector<Candidate> candidates;

        for(const auto& field : m_fields) {
            if(!field->active) continue;
            auto g = field->calcGravity(point);
            if(!g) continue;
            u8  pri  = (u8)field->priority;
            f32 dist = field->distanceTo(point);
            candidates.push_back({*g, pri, dist});
            if(!found || pri > bestPri) { bestPri = pri; found = true; }
        }

        if(candidates.empty()) return bestG;

        // Filter to highest priority candidates
        Vec3  sumG   = {};
        f32   sumW   = 0.f;
        bool  hasTop = false;

        for(const auto& c : candidates) {
            if(c.pri < bestPri) continue;
            // Blend by inverse distance (closer = more influence)
            f32 w = 1.f / (c.dist + 0.1f);
            sumG += c.g * w;
            sumW += w;
            hasTop = true;
        }

        if(!hasTop || sumW < 1e-9f) return bestG;
        return sumG * (1.f / sumW);
    }

    // Get the "up" vector for an entity at a point
    Vec3 calcUp(const Vec3& point) const {
        return (-calcGravity(point)).normalized();
    }

    // Align an orientation to the current gravity field.
    // Returns a rotation matrix where +Y = local up (anti-gravity).
    Mat4 calcOrientationMatrix(const Vec3& point, const Vec3& forwardHint) const {
        Vec3 up = calcUp(point);
        return Mat4::fromUpVector(up, forwardHint);
    }

    size_t fieldCount() const { return m_fields.size(); }

    // Debug: which field types are active
    void debugPrint() const {
        printf("[GravityManager] %zu fields:\n", m_fields.size());
        for(const auto& f : m_fields) {
            const char* typeName = "Unknown";
            switch(f->type) {
                case GravityType::Global:    typeName="Global"; break;
                case GravityType::Sphere:    typeName="Sphere"; break;
                case GravityType::InvSphere: typeName="InvSphere"; break;
                case GravityType::Cylinder:  typeName="Cylinder"; break;
                case GravityType::Disk:      typeName="Disk"; break;
                case GravityType::Point:     typeName="Point"; break;
                case GravityType::Torus:     typeName="Torus"; break;
            }
            printf("  [%s] strength=%.1f range=%.1f active=%d pri=%d\n",
                typeName, f->strength, f->range, (int)f->active, (int)f->priority);
        }
    }

private:
    std::vector<std::shared_ptr<GravityField>> m_fields;
};

} // namespace Physics
} // namespace Starshine
