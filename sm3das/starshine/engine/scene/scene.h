#pragma once
// ─────────────────────────────────────────────
//  STARSHINE ENGINE — Scene Graph & Actor System
//
//  Reimplements Nintendo's JDrama/JStage scene
//  hierarchy from Sunshine, and the PlacementInfo/
//  ActorInfo system from Galaxy.
//
//  Key concepts:
//    Actor    — any game object (Mario, enemy, item,
//               platform, trigger volume, etc.)
//    ActorId  — unique type identifier per class
//    Scene    — a level/world containing actors
//    Layer    — Galaxy uses layers per scenario
//               (e.g. layer A = star 1, layer B = star 2)
// ─────────────────────────────────────────────

#include "../core_types.h"
#include "../filesys/bcsv.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

namespace Starshine {
namespace Scene {

// ── Transform ──────────────────────────────────
struct Transform {
    Vec3 position = {};
    Vec3 rotation = {};   // Euler angles (degrees), matches BCSV format
    Vec3 scale    = {1,1,1};

    Mat4 toMatrix() const {
        return Mat4::translation(position)
             * Mat4::rotationY(rotation.y * kDegToRad)
             * Mat4::rotationX(rotation.x * kDegToRad)
             * Mat4::rotationZ(rotation.z * kDegToRad)
             * Mat4::scale(scale);
    }
};

// ── Actor base ─────────────────────────────────
class Actor {
public:
    std::string  name;         // e.g. "Goomba", "Coin", "WarpObj"
    u32          id      = 0;  // unique instance id
    Transform    transform;
    bool         active  = true;
    bool         visible = true;

    // BCSV args (Galaxy: Arg0–Arg7 per placement entry)
    s32 args[8] = {};

    // Layer flags — which Galaxy scenarios this actor appears in
    u32 scenarioMask = 0xFFFFFFFF;

    // Called once when actor is spawned into the scene
    virtual void onSpawn() {}

    // Called every frame
    virtual void onUpdate(f32 dt) {}

    // Called when actor is about to be removed
    virtual void onDespawn() {}

    // Does this actor exist in a given Galaxy scenario?
    bool isActiveInScenario(u32 scenario) const {
        return (scenarioMask & (1u << scenario)) != 0;
    }

    virtual ~Actor() = default;
};

// ── Actor registry: factory for actor types ────
using ActorFactory = std::function<std::unique_ptr<Actor>()>;

class ActorRegistry {
public:
    static ActorRegistry& get() {
        static ActorRegistry s;
        return s;
    }

    void registerActor(const std::string& name, ActorFactory factory) {
        m_factories[name] = std::move(factory);
    }

    std::unique_ptr<Actor> create(const std::string& name) const {
        auto it = m_factories.find(name);
        if(it == m_factories.end()) {
            // Unknown actor — create a placeholder
            auto a = std::make_unique<Actor>();
            a->name = name;
            return a;
        }
        return it->second();
    }

    bool knows(const std::string& name) const {
        return m_factories.count(name) > 0;
    }

private:
    std::unordered_map<std::string, ActorFactory> m_factories;
};

// ── Galaxy layer/scenario system ───────────────
// Galaxy levels have multiple "scenarios" (= star objectives).
// Different actors appear depending on which scenario is active.
struct Scenario {
    u32         index    = 0;
    std::string name;      // e.g. "Bees to the Tree"
    std::string powerStar; // which star this scenario targets
};

// ── The scene ──────────────────────────────────
class Scene {
public:
    std::string            name;
    Game                   game = Game::Galaxy1;
    std::vector<Scenario>  scenarios;
    u32                    activeScenario = 0;

    // All actors in the scene
    const std::vector<std::unique_ptr<Actor>>& actors() const { return m_actors; }

    // Spawn an actor
    Actor* spawnActor(std::unique_ptr<Actor> actor) {
        Actor* ptr = actor.get();
        ptr->onSpawn();
        m_actors.push_back(std::move(actor));
        return ptr;
    }

    // Remove actor by id
    void despawnActor(u32 id) {
        for(auto& a : m_actors)
            if(a->id == id) { a->onDespawn(); a->active = false; }
        m_actors.erase(
            std::remove_if(m_actors.begin(), m_actors.end(),
                [](const auto& a){ return !a->active; }),
            m_actors.end());
    }

    // Update all active actors
    void update(f32 dt) {
        for(auto& a : m_actors) {
            if(!a->active) continue;
            if(!a->isActiveInScenario(activeScenario)) continue;
            a->onUpdate(dt);
        }
    }

    // Find actor by name (first match)
    Actor* findByName(const std::string& name) {
        for(auto& a : m_actors)
            if(a->name == name) return a.get();
        return nullptr;
    }

    // Populate scene from BCSV placement tables (Galaxy)
    // objInfoBcsv = ObjInfo.bcsv loaded from stage ARC
    void loadFromBcsv(const FileSys::BcsvTable& objInfo) {
        objInfo.forEach([&](const FileSys::BcsvRow& row, u32 idx) {
            std::string actorName = row.getObjName();
            if(actorName.empty()) return;

            auto actor = ActorRegistry::get().create(actorName);
            actor->name       = actorName;
            actor->id         = idx + 1;
            actor->transform.position = row.getPos();
            actor->transform.rotation = row.getRot();
            actor->args[0]    = row.getInt(FileSys::Hashes::kArg0);
            actor->args[1]    = row.getInt(FileSys::Hashes::kArg1);
            actor->args[2]    = row.getInt(FileSys::Hashes::kArg2);
            actor->args[3]    = row.getInt(FileSys::Hashes::kArg3);
            // Scenario mask from l_id bitmask
            u32 lid = (u32)row.getInt(FileSys::Hashes::kL_id, -1);
            actor->scenarioMask = lid;

            spawnActor(std::move(actor));
        });
    }

    // Debug: print all actors
    void dumpActors() const {
        printf("[Scene: %s] %zu actors, scenario %u\n",
               name.c_str(), m_actors.size(), activeScenario);
        for(const auto& a : m_actors) {
            printf("  [%4u] %-30s pos=(%.1f,%.1f,%.1f)\n",
                   a->id, a->name.c_str(),
                   a->transform.position.x,
                   a->transform.position.y,
                   a->transform.position.z);
        }
    }

private:
    std::vector<std::unique_ptr<Actor>> m_actors;
    u32 m_nextId = 1;
};

} // namespace Scene
} // namespace Starshine
