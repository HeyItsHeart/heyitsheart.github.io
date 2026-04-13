#pragma once
// ─────────────────────────────────────────────
//  STARSHINE ENGINE — WASM Bridge
//
//  This is the boundary between C++ engine code
//  and the JavaScript/TypeScript browser shell.
//
//  All functions here are exported via Emscripten's
//  EMSCRIPTEN_KEEPALIVE macro, making them callable
//  from JavaScript as:
//    Module._starshine_init(...)
//    Module._starshine_update(dt)
//    etc.
//
//  The JS shell handles:
//    - File loading (fetch → MEMFS)
//    - WebGL canvas setup
//    - Input polling → starshine_input()
//    - requestAnimationFrame loop
// ─────────────────────────────────────────────

#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
  #include <emscripten/html5.h>
  #define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
  #define WASM_EXPORT
#endif

#include "core_types.h"
#include "filesys/arc_parser.h"
#include "filesys/yaz0.h"
#include "filesys/bcsv.h"
#include "physics/gravity.h"
#include "actor/mario_controller.h"
#include "actor/fludd.h"
#include "scene/scene.h"
#include "renderer/renderer.h"

#include <cstdio>
#include <cstring>

namespace Starshine {

// ── Engine state ───────────────────────────────
struct EngineState {
    Game                             game;
    Renderer::Renderer               renderer;
    Physics::GravityManager          gravity;
    std::unique_ptr<Actor::MarioController> mario;
    std::unique_ptr<Actor::FLUDD>           fludd;
    std::unique_ptr<Scene::Scene>           scene;
    bool                             initialized = false;
    bool                             running     = false;
    u32                              frameCount  = 0;
    double                           lastTime    = 0.0;
    // Current input state
    Actor::MarioInput                input;
    Vec2                             prevStick   = {};
};

static EngineState* g_engine = nullptr;

} // namespace Starshine

// ─────────────────────────────────────────────
//  C exports (callable from JavaScript)
// ─────────────────────────────────────────────
extern "C" {

// ── Init ───────────────────────────────────────
// Call once after WebGL context is ready.
// game: 0=Sunshine, 1=Galaxy1, 2=Galaxy2
WASM_EXPORT int starshine_init(int game, int viewportW, int viewportH) {
    using namespace Starshine;

    if(g_engine) { delete g_engine; g_engine = nullptr; }
    g_engine = new EngineState();
    g_engine->game = (Game)game;

    // Init renderer
    if(!g_engine->renderer.init((u32)viewportW, (u32)viewportH)) {
        fprintf(stderr, "[Starshine] Renderer init failed\n");
        return 0;
    }

    // Init Mario
    g_engine->mario = std::make_unique<Actor::MarioController>(g_engine->game);
    g_engine->mario->setGravityManager(&g_engine->gravity);

    // Init FLUDD (Sunshine only)
    if(g_engine->game == Game::Sunshine)
        g_engine->fludd = std::make_unique<Actor::FLUDD>(Actor::FluddNozzle::Squirt);

    // Init scene
    g_engine->scene = std::make_unique<Scene::Scene>();
    g_engine->scene->game = g_engine->game;

    // Setup default gravity based on game
    if(g_engine->game == Game::Sunshine) {
        // Sunshine: flat gravity pointing down
        auto globalGrav = std::make_shared<Physics::GlobalGravity>();
        globalGrav->strength = kGravityDefault;
        globalGrav->axis = {0,-1,0};
        g_engine->gravity.addField(globalGrav);
    } else {
        // Galaxy: gravity is added per-level, but start with a global fallback
        auto globalGrav = std::make_shared<Physics::GlobalGravity>();
        globalGrav->strength    = kGalaxyGravity;
        globalGrav->priority    = Physics::GravityPriority::Lowest;
        globalGrav->axis        = {0,-1,0};
        g_engine->gravity.addField(globalGrav);
    }

    g_engine->initialized = true;
    g_engine->running     = true;
    printf("[Starshine] Initialized. Game=%d W=%d H=%d\n", game, viewportW, viewportH);
    return 1;
}

// ── Resize ─────────────────────────────────────
WASM_EXPORT void starshine_resize(int w, int h) {
    if(!Starshine::g_engine) return;
    Starshine::g_engine->renderer.resize((u32)w, (u32)h);
}

// ── Load archive ───────────────────────────────
// Loads a Yaz0-compressed U8 ARC from a path on the
// Emscripten virtual filesystem (MEMFS).
// Returns number of files loaded, or -1 on error.
WASM_EXPORT int starshine_load_arc(const char* path) {
    using namespace Starshine;
    if(!g_engine) return -1;

    // Read file from Emscripten MEMFS
    FILE* f = fopen(path, "rb");
    if(!f) { fprintf(stderr, "[Starshine] Cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<u8> buf((size_t)sz);
    fread(buf.data(), 1, (size_t)sz, f);
    fclose(f);

    // Decompress Yaz0 if needed
    if(FileSys::Yaz0::isYaz0(buf.data(), buf.size())) {
        auto dec = FileSys::Yaz0::decompress(buf);
        if(!dec) { fprintf(stderr, "[Starshine] Yaz0 decomp failed: %s\n", path); return -1; }
        buf = std::move(*dec);
    }

    // Parse ARC
    auto arc = FileSys::Archive::parse(std::move(buf));
    if(!arc) { fprintf(stderr, "[Starshine] ARC parse failed: %s\n", path); return -1; }

    printf("[Starshine] Loaded %s — %zu files\n", path, arc->fileCount());

    // If this is an ObjInfo.bcsv-containing archive, load actors
    if(auto* bcsvFile = arc->findFile("/ObjInfo.bcsv")) {
        auto table = FileSys::BcsvTable::parse(bcsvFile->data, bcsvFile->size);
        if(table) {
            g_engine->scene->loadFromBcsv(*table);
            printf("[Starshine] Loaded %u actors from ObjInfo.bcsv\n", table->rowCount());
        }
    }

    return (int)arc->fileCount();
}

// ── Input ──────────────────────────────────────
// Called from JS requestAnimationFrame loop each frame
WASM_EXPORT void starshine_input(
    float stickX, float stickY,
    int btnJump, int btnAction, int btnCrouch,
    int jumpPressed, int actionPressed, int crouchPressed)
{
    if(!Starshine::g_engine) return;
    auto& inp = Starshine::g_engine->input;
    inp.stick        = {stickX, stickY};
    inp.btnJump      = btnJump != 0;
    inp.btnAction    = btnAction != 0;
    inp.btnCrouch    = btnCrouch != 0;
    inp.jumpPressed  = jumpPressed != 0;
    inp.actionPressed= actionPressed != 0;
    inp.crouchPressed= crouchPressed != 0;
}

// ── Update + render frame ──────────────────────
// Returns 1 on success, 0 if engine not ready
WASM_EXPORT int starshine_frame(double timestamp) {
    using namespace Starshine;
    if(!g_engine || !g_engine->initialized || !g_engine->running) return 0;

    f32 dt = 0.0166f; // target 60fps
    if(g_engine->lastTime > 0.0) {
        dt = (f32)((timestamp - g_engine->lastTime) / 1000.0);
        if(dt > 0.1f) dt = 0.1f; // clamp spike frames
    }
    g_engine->lastTime = timestamp;

    // Update scene actors
    g_engine->scene->update(dt);

    // Update Mario
    g_engine->mario->update(g_engine->input, dt);

    // Update FLUDD (Sunshine)
    if(g_engine->fludd) {
        const auto& mp = g_engine->mario->physics();
        Vec3 aimDir = Vec3{sinf(mp.facingYaw), 0.f, cosf(mp.facingYaw)};
        Vec3 recoil = g_engine->fludd->update(
            g_engine->input.btnAction,
            g_engine->input.actionPressed,
            false, // triggerReleased — needs tracking
            aimDir, mp.up,
            mp.onGround, dt);
        // Apply recoil to Mario velocity
        // g_engine->mario->physics().velocity += recoil * dt;
    }

    // Render
    const auto& mp = g_engine->mario->physics();
    Vec3 eye    = mp.position + mp.up * 180.f +
                  Vec3{-sinf(mp.facingYaw), 0.f, -cosf(mp.facingYaw)} * 300.f;
    Vec3 center = mp.position + mp.up * 80.f;

    g_engine->renderer.beginFrame();
    g_engine->renderer.setCamera(eye, center, mp.up);
    // Scene meshes rendered here once BMD/BRRES parser is integrated
    g_engine->renderer.endFrame();

    g_engine->frameCount++;
    return 1;
}

// ── Spawn gravity field (Galaxy) ───────────────
// type: 0=Global,1=Sphere,2=Cylinder,3=Disk,4=Point,5=InvSphere
WASM_EXPORT void starshine_add_gravity(int type,
    float px, float py, float pz,
    float ax, float ay, float az,
    float strength, float range, int priority)
{
    using namespace Starshine;
    using namespace Physics;
    if(!g_engine) return;

    std::shared_ptr<GravityField> field;
    Vec3 pos = {px,py,pz};
    Vec3 axis= {ax,ay,az};

    switch(type) {
        case 0: { auto f=std::make_shared<GlobalGravity>(); f->axis=axis; field=f; break; }
        case 1: { auto f=std::make_shared<SphereGravity>(); f->position=pos; field=f; break; }
        case 2: { auto f=std::make_shared<CylinderGravity>(); f->position=pos; f->axis=axis; field=f; break; }
        case 3: { auto f=std::make_shared<DiskGravity>(); f->position=pos; f->axis=axis; field=f; break; }
        case 4: { auto f=std::make_shared<PointGravity>(); f->position=pos; field=f; break; }
        case 5: { auto f=std::make_shared<InvSphereGravity>(); f->position=pos; field=f; break; }
        default: return;
    }

    field->strength = strength;
    field->range    = range;
    field->priority = (GravityPriority)(u8)priority;
    g_engine->gravity.addField(field);
}

// ── Clear gravity fields ───────────────────────
WASM_EXPORT void starshine_clear_gravity() {
    if(Starshine::g_engine) Starshine::g_engine->gravity.clear();
}

// ── Set Mario position ─────────────────────────
WASM_EXPORT void starshine_set_mario_pos(float x, float y, float z) {
    if(!Starshine::g_engine || !Starshine::g_engine->mario) return;
    Starshine::g_engine->mario->setPosition({x,y,z});
}

// ── Get Mario position (writes to out[3]) ──────
WASM_EXPORT void starshine_get_mario_pos(float* out) {
    if(!Starshine::g_engine || !Starshine::g_engine->mario || !out) return;
    const auto& p = Starshine::g_engine->mario->physics().position;
    out[0]=p.x; out[1]=p.y; out[2]=p.z;
}

// ── Get Mario state ────────────────────────────
WASM_EXPORT int starshine_get_mario_state() {
    if(!Starshine::g_engine || !Starshine::g_engine->mario) return -1;
    return (int)Starshine::g_engine->mario->state();
}

// ── Get FLUDD water level ──────────────────────
WASM_EXPORT float starshine_get_fludd_water() {
    if(!Starshine::g_engine || !Starshine::g_engine->fludd) return 0.f;
    return Starshine::g_engine->fludd->water();
}

// ── Debug: dump scene ──────────────────────────
WASM_EXPORT void starshine_dump_scene() {
    if(Starshine::g_engine && Starshine::g_engine->scene)
        Starshine::g_engine->scene->dumpActors();
}

// ── Shutdown ────────────────────────────────────
WASM_EXPORT void starshine_shutdown() {
    if(Starshine::g_engine) {
        delete Starshine::g_engine;
        Starshine::g_engine = nullptr;
    }
}

} // extern "C"
