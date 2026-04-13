# Starshine Engine — Architecture Reference

## What this is

A data-driven reimplementation of the Sunshine and Galaxy runtime,
compiled to WebAssembly. It loads original game assets and replays
the engine logic natively — no emulation, no JIT.

Analogous to: sm64-port, OpenMW, OpenRCT2.

---

## Directory Structure

```
starshine/
  CMakeLists.txt              — Build config (Emscripten → WASM)
  engine/
    core_types.h              — Vec2/Vec3/Vec4/Mat4/Quat, scalar types, constants
    filesys/
      yaz0.h                  — Yaz0 decompressor (wraps all Sunshine/Galaxy ARCs)
      arc_parser.h            — Nintendo U8 archive parser (zero-copy)
      bcsv.h                  — BCSV binary table parser (drives Galaxy actor placement)
    physics/
      gravity.h               — Galaxy multi-planet gravity system
    actor/
      mario_controller.h      — Mario movement state machine (Sunshine + Galaxy)
      fludd.h                 — FLUDD nozzle system + goop tracking (Sunshine)
    scene/
      scene.h                 — Scene graph, actor system, BCSV loader
    renderer/
      renderer.h              — WebGL2 renderer (replaces GX/GX2 pipeline)
    wasm/
      bridge.h                — All C exports callable from JavaScript
      main.cpp                — Entry point (includes bridge, native test main)
  web/
    starshine_shell.ts        — TypeScript browser shell (input, rAF loop, file loading)
  docs/
    ARCHITECTURE.md           — This file
```

---

## Boot Flow

### Sunshine
```
1. JS: engine.init()               — load WASM, init WebGL
2. JS: engine.loadArc(url, path)   — fetch .szs, Yaz0 decomp, mount in MEMFS
3. C++: starshine_load_arc(path)   — parse U8 archive, load stage BCSV
4. C++: scene.loadFromBcsv()       — spawn actors from ObjInfo.bcsv
5. JS: engine.addGravity({type:0}) — flat gravity for Sunshine
6. JS: engine.setMarioPosition()   — place Mario at spawn
7. JS: engine.start()              — begin rAF loop
8. Per frame: starshine_frame(dt)  — update scene, Mario, FLUDD, render
```

### Galaxy
```
1. JS: engine.init()               — load WASM
2. JS: engine.loadArc(url, path)   — load galaxy stage archive
3. C++: parse ScenarioData.bcsv    — load scenario list
4. C++: parse ObjInfo.bcsv         — spawn actors per scenario
5. JS: engine.addGravity(...)      — add sphere/cylinder/disk gravity fields
6. JS: engine.start()
7. Per frame: gravity manager orients Mario + all physics objects
```

---

## Key Subsystems

### Gravity (physics/gravity.h)
The defining feature of Galaxy. Every planet type has a GravityField subclass:
- `SphereGravity`     — toward sphere center (most planets)
- `CylinderGravity`   — toward cylinder axis (tube levels)
- `DiskGravity`       — perpendicular to disc plane (flat planets)
- `GlobalGravity`     — flat downward (Sunshine / hub worlds)
- `InvSphereGravity`  — away from center (inside-out)
- `PointGravity`      — toward single point

`GravityManager` blends multiple fields by priority + inverse distance.
Mario queries this every frame to get his local "up" vector.

### Mario Controller (actor/mario_controller.h)
Shared state machine for both games. States:
- Ground: Idle → Walk → Run → Crouch → Slide
- Air:    Jump → DoubleJump → TripleJump → Backflip → LongJump → Fall
- Special: FLUDD_Hover (Sunshine), SpinAttack (Galaxy)

Per-game params via `MarioGameParams::forSunshine()` / `forGalaxy()`.

### FLUDD (actor/fludd.h)
Sunshine-specific. Four nozzles:
- Squirt: directional jet, spawns water particles, recoil
- Hover:  sustained upward thrust
- Rocket: charge-up explosive launch
- Turbo:  forward dash

Water tank [0-1], drains per nozzle, refills on ground.
`GoopGrid` tracks cleaned pollution tiles per stage.

### File System (filesys/)
- `Yaz0::decompress()` — all stage files are Yaz0-wrapped
- `Archive::parse()`   — zero-copy U8 archive, path-indexed lookup
- `BcsvTable::parse()` — binary CSV, drives all actor/scenario data

### Renderer (renderer/renderer.h)
WebGL2 via Emscripten's GLES3 bindings.
TEV (GameCube's texture combiner) approximated in GLSL:
- Opaque geometry batch
- Alpha-test batch
- Translucent geometry (sorted back-to-front)
Environment maps, vertex colors, directional lighting, fog.

---

## Building

### Prerequisites
- Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html
- CMake 3.16+

### WASM build
```bash
source /path/to/emsdk/emsdk_env.sh
mkdir build && cd build
emcmake cmake ..
make -j8
# Output: starshine.js + starshine.wasm
```

### Native test build (no browser)
```bash
mkdir build-native && cd build-native
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j8
./starshine /path/to/bianco.arc   # test ARC parsing
```

---

## Asset Requirements

From an extracted Sunshine or Galaxy ISO:

### Sunshine
```
/files/data/scene/bianco.arc   → Bianco Hills stage
/files/data/scene/mare.arc     → Ricco Harbor
/files/mario/mario.bmd         → Mario model
/files/data/shinemario.arc     → Shine sprites
```

### Galaxy
```
/StageData/BeeHiveGalaxy/      → stage folder
  BeeHiveGalaxyScenario.arc    → scenario table
  BeeHiveGalaxyMap.arc         → terrain + objects
ObjectData/                    → shared object models
```

All files are Yaz0-compressed. Run through `Yaz0::decompress()` first.

---

## Next Steps

### Immediate (to get Bianco Hills loading)
1. `filesys/bmd_parser.h`    — parse GameCube BMD model format
2. `renderer/mesh_builder.h` — convert BMD geometry to GpuMesh
3. `filesys/collision.h`     — parse KCL collision mesh
4. `physics/collision.h`     — raycast against KCL for ground detection
5. Test: load bianco.arc → parse ObjInfo.bcsv → render terrain

### Medium term
6. `renderer/texture.h`     — BTI/TPL texture format → WebGL upload
7. `actor/camera.h`         — Sunshine follow camera
8. `actor/npc.h`            — basic NPC actors from actor registry
9. Galaxy: render first sphere planet with sphere gravity

### Long term
10. BMD skeletal animation (BCK/BCA)
11. BRSTM audio → Web Audio API
12. Full collision response (slopes, walls, ceilings)
13. Galaxy star pointer / motion control emulation via mouse
