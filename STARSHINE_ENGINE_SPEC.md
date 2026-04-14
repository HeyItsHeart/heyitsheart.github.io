# StarShine Engine — Complete Architecture & Setup Guide
**Version 0.1.0-alpha | The Engine of Wonder**

---

## TABLE OF CONTENTS

1. [Architecture Overview](#architecture-overview)
2. [Folder Structure](#folder-structure)
3. [Core Engine Bootstrap](#core-engine-bootstrap)
4. [Renderer: WebGPU + WebGL Fallback](#renderer)
5. [GX → WebGPU Translation](#gx-translation)
6. [Actor System (Both Modes)](#actor-system)
7. [Physics & Animation Systems](#physics-animation)
8. [Asset Extraction Pipeline](#asset-pipeline)
9. [Audio System (DSP → WebAudio)](#audio-system)
10. [WASM Build Instructions](#wasm-build)
11. [Constellation Editor](#constellation-editor)
12. [ConstellationScript Language Spec](#constellationscript)
13. [WonderMod Framework](#wondermod)
14. [Optimization Strategy (Low-End Hardware)](#optimization)
15. [Multi-Mode Runtime Linking](#runtime-linking)
16. [Example Test Level (Both Modes)](#example-level)
17. [Tech Demo Setup](#tech-demo)
18. [Roadmap & Extension Points](#roadmap)

---

## A. ARCHITECTURE OVERVIEW <a name="architecture-overview"></a>

```
┌─────────────────────────────────────────────────────────────────┐
│                    STARSHINE ENGINE v0.1                         │
│                                                                  │
│  ┌──────────────────────┐  ┌──────────────────────────────────┐ │
│  │   MODE A             │  │   MODE B                         │ │
│  │   Compatibility      │  │   Creator Mode                   │ │
│  │   (ROM Runtime)      │  │   (Original Games)               │ │
│  │                      │  │                                  │ │
│  │  • Sunshine/Galaxy   │  │  • Full ECS                      │ │
│  │  • GX → WebGPU       │  │  • Scene Editor                  │ │
│  │  • JMap/BCSV         │  │  • Custom Materials              │ │
│  │  • Exact Physics     │  │  • ConstellationScript           │ │
│  └──────────┬───────────┘  └──────────────┬───────────────────┘ │
│             │                             │                      │
│             └──────────────┬──────────────┘                     │
│                            │                                     │
│                  ┌─────────▼──────────┐                         │
│                  │  STARSHINE RUNTIME  │                         │
│                  │  (C++ → WASM)       │                         │
│                  ├────────────────────┤                         │
│                  │  Scene Graph / ECS  │                         │
│                  │  Actor System       │                         │
│                  │  Physics Engine     │                         │
│                  │  Animation System   │                         │
│                  │  Asset Manager      │                         │
│                  │  Memory Pools       │                         │
│                  │  WonderMod Loader   │                         │
│                  └─────────┬──────────┘                         │
│                            │                                     │
│          ┌─────────────────┼──────────────────┐                 │
│          │                 │                  │                  │
│  ┌───────▼──────┐  ┌───────▼──────┐  ┌───────▼──────┐         │
│  │  WebGPU      │  │  WebAudio    │  │  Input       │         │
│  │  Renderer    │  │  System      │  │  Manager     │         │
│  │  (+ WebGL    │  │  (DSP layer) │  │  (All        │         │
│  │   fallback)  │  │              │  │   platforms) │         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │            CONSTELLATION EDITOR (HTML/JS/WASM)            │  │
│  │  Scene Editor | Inspector | Asset Browser | Shader Graph   │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    WONDERMOD SYSTEM                        │  │
│  │  .wondermod loader | Hooks | Sandboxed Script Execution    │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Core Design Principles

- **WASM-native**: The engine core is C++17 compiled via Emscripten to WebAssembly. This gives near-native performance in browser.
- **WebGPU-first, WebGL-fallback**: Uses the modern WebGPU API where available (Chrome 113+, Firefox 120+ with flag, Edge 113+). Falls back to WebGL 2.0 transparently on older/low-end devices.
- **Two runtime modes share one codebase**: A compile-time flag `STARSHINE_MODE` and a runtime registry separate compatibility-mode subsystems from creator-mode subsystems. Both share the renderer, ECS, physics, and audio layers.
- **Low-end first**: Every subsystem is designed around a budget of 2GB RAM, integrated graphics (Intel UHD 620 class), and a dual-core CPU. Features gracefully degrade.
- **WonderMod is a first-class citizen**: Not bolted on. The mod loader, hook system, and sandboxed scripting are baked into the engine core.

---

## B. FOLDER STRUCTURE <a name="folder-structure"></a>

```
starshine/
├── CMakeLists.txt                  # Root build
├── em_config.json                  # Emscripten config
├── package.json                    # JS tooling (Vite, etc.)
│
├── engine/                         # Core C++ engine (→ WASM)
│   ├── core/
│   │   ├── Engine.h/.cpp           # Main engine class, tick loop
│   │   ├── RuntimeMode.h           # MODE_A / MODE_B enum + registry
│   │   ├── MemoryPool.h/.cpp       # Pool allocators (slab/arena/ring)
│   │   ├── EventBus.h/.cpp         # Typed event dispatch
│   │   └── Log.h/.cpp              # Structured logging → JS console
│   │
│   ├── ecs/
│   │   ├── World.h/.cpp            # ECS world (entity-component)
│   │   ├── Entity.h                # Entity ID + generation
│   │   ├── ComponentStorage.h      # Archetype-based dense storage
│   │   ├── System.h                # System base class
│   │   └── Query.h                 # Component query builder
│   │
│   ├── renderer/
│   │   ├── RenderDevice.h          # Abstract render device interface
│   │   ├── WebGPUDevice.h/.cpp     # WebGPU backend
│   │   ├── WebGLDevice.h/.cpp      # WebGL 2.0 fallback backend
│   │   ├── ShaderCompiler.h/.cpp   # WGSL + GLSL shader management
│   │   ├── MaterialSystem.h/.cpp   # Material/TEV pipeline
│   │   ├── MeshBuffer.h/.cpp       # Vertex/index buffer management
│   │   ├── RenderGraph.h/.cpp      # Render pass scheduling
│   │   ├── ShadowMap.h/.cpp        # PCF shadow mapping
│   │   ├── BloomPass.h/.cpp        # Post-process bloom
│   │   └── DebugRenderer.h/.cpp    # Debug lines/shapes
│   │
│   ├── gx/                         # GX → WebGPU translation (Mode A)
│   │   ├── GXTypes.h               # GX enums/structs (from GC SDK)
│   │   ├── GXToWGPU.h/.cpp         # TEV stage translator
│   │   ├── GXVertexDecoder.h/.cpp  # GC vertex format → WebGPU vertex layout
│   │   ├── GXDisplayList.h/.cpp    # Display list executor
│   │   └── GXTextureFormat.h/.cpp  # CMPR/RGB5A3/IA8 → RGBA8 decompressor
│   │
│   ├── actor/
│   │   ├── ActorBase.h/.cpp        # Base actor class
│   │   ├── ActorRegistry.h/.cpp    # Factory + lookup
│   │   ├── ActorManager.h/.cpp     # Lifecycle: spawn/tick/destroy
│   │   ├── compat/                 # Mode A actors
│   │   │   ├── MarioActor.h/.cpp
│   │   │   ├── EnemyBase.h/.cpp
│   │   │   ├── GravityField.h/.cpp
│   │   │   └── [...]
│   │   └── creator/                # Mode B actor templates
│   │       ├── PlatformerController.h/.cpp
│   │       ├── RigidBodyActor.h/.cpp
│   │       └── [...]
│   │
│   ├── physics/
│   │   ├── PhysicsWorld.h/.cpp     # Physics simulation coordinator
│   │   ├── CollisionDetection.h    # BVH + SAT/GJK
│   │   ├── GravitySystem.h/.cpp    # Galaxy-style arbitrary gravity
│   │   ├── WaterPhysics.h/.cpp     # Sunshine FLUDD/water
│   │   └── RigidBody.h/.cpp        # Rigidbody simulation
│   │
│   ├── animation/
│   │   ├── AnimationPlayer.h/.cpp  # Skeleton/BCK/BCA player
│   │   ├── AnimationBlender.h      # Additive/blend tree
│   │   ├── CutscenePlayer.h/.cpp   # BCF/demo file playback
│   │   └── IKSolver.h/.cpp         # Analytical 2-bone IK
│   │
│   ├── audio/
│   │   ├── AudioManager.h/.cpp     # Main audio coordinator
│   │   ├── DSPDecoder.h/.cpp       # GC DSP ADPCM → PCM
│   │   ├── WebAudioBridge.h/.cpp   # C++ → JS WebAudio bridge
│   │   ├── SoundEffect.h/.cpp      # One-shot SFX
│   │   └── MusicStream.h/.cpp      # Streaming BGM (BRSTM/BCSTM)
│   │
│   ├── camera/
│   │   ├── CameraSystem.h/.cpp     # Camera base + manager
│   │   ├── CompatCamera.h/.cpp     # Sunshine/Galaxy camera behavior
│   │   └── FreeCam.h/.cpp          # Debug/editor camera
│   │
│   ├── io/
│   │   ├── FileSystem.h/.cpp       # VFS over OPFS/IndexedDB
│   │   ├── ARC.h/.cpp              # Nintendo ARC archive
│   │   ├── RARC.h/.cpp             # RARC archive (Sunshine)
│   │   ├── SZS.h/.cpp              # Yaz0 decompressor
│   │   ├── BCSV.h/.cpp             # Binary CSV (JMap data tables)
│   │   └── AssetPack.h/.cpp        # StarShine .ssp asset pack
│   │
│   ├── scenario/
│   │   ├── ScenarioLoader.h/.cpp   # JMap scenario loading
│   │   ├── GalaxyWorld.h/.cpp      # Galaxy world system
│   │   ├── SunshineLevel.h/.cpp    # Sunshine level loading
│   │   └── StageInfo.h             # Stage metadata
│   │
│   ├── scripting/
│   │   ├── ScriptEngine.h/.cpp     # ConstellationScript VM host
│   │   ├── LuaBridge.h/.cpp        # Optional Lua 5.4 integration
│   │   ├── ModScriptSandbox.h      # Sandboxed mod script context
│   │   └── ScriptAPI.h             # Exposed engine API bindings
│   │
│   └── mod/
│       ├── WonderModLoader.h/.cpp  # .wondermod bundle parser
│       ├── ModManifest.h           # manifest.json schema
│       ├── HookSystem.h/.cpp       # File/actor/shader hook registry
│       ├── PatchApplicator.h/.cpp  # BCSV/JMap data patching
│       └── ModSandbox.h/.cpp       # Security/validation layer
│
├── constellation/                  # Editor UI (HTML/JS/WASM)
│   ├── index.html                  # Editor shell
│   ├── src/
│   │   ├── main.ts                 # Editor entry point
│   │   ├── editor/
│   │   │   ├── EditorApp.ts        # Main editor class
│   │   │   ├── SceneEditor.ts      # 3D viewport
│   │   │   ├── Hierarchy.ts        # Entity tree panel
│   │   │   ├── Inspector.ts        # Component property editor
│   │   │   ├── AssetBrowser.ts     # Asset library panel
│   │   │   ├── MaterialEditor.ts   # Material node graph
│   │   │   ├── ShaderGraph.ts      # WGSL visual shader editor
│   │   │   ├── ModDebugger.ts      # WonderMod debugging tools
│   │   │   └── ProjectManager.ts   # Project open/save/export
│   │   ├── bridge/
│   │   │   ├── EngineBridge.ts     # JS ↔ WASM messaging layer
│   │   │   └── WASMLoader.ts       # Loads + initializes WASM module
│   │   ├── ui/
│   │   │   ├── Panel.ts            # Dockable panel base
│   │   │   ├── MenuBar.ts
│   │   │   ├── Toolbar.ts
│   │   │   ├── TreeView.ts
│   │   │   ├── NumberField.ts
│   │   │   └── ColorPicker.ts
│   │   └── styles/
│   │       └── constellation.css   # Editor dark theme
│   └── dist/                       # Built editor output
│
├── toolchain/                      # Companion WASM tools
│   ├── extractor/                  # ISO/ROM extractor
│   │   └── ISOExtractor.cpp
│   ├── symbolmap/                  # Symbol mapper
│   │   └── SymbolMapper.cpp
│   ├── gxdump/                     # GX shader extractor
│   │   └── GXDumper.cpp
│   ├── packbuilder/                # .ssp asset pack builder
│   │   └── PackBuilder.cpp
│   ├── modbuilder/                 # .wondermod bundler
│   │   └── ModBuilder.cpp
│   └── exportpipeline/             # Game export to HTML/WASM
│       └── ExportPipeline.cpp
│
├── cscript/                        # ConstellationScript compiler/VM
│   ├── lexer/
│   ├── parser/
│   ├── compiler/                   # → WASM bytecode
│   ├── vm/                         # WASM-friendly stack VM
│   └── stdlib/                     # Standard library
│
├── examples/
│   ├── test_level_compat/          # Mode A test (Galaxy-style)
│   ├── test_level_creator/         # Mode B test (original game)
│   └── tech_demo/                  # School-laptop tech demo
│       ├── index.html
│       ├── demo.js
│       └── README_IMPORT.md        # How to import into Constellation
│
└── docs/
    ├── MOD_API.md                  # Stable Mod API reference
    ├── CSCRIPT_SPEC.md             # ConstellationScript full spec
    ├── GX_TRANSLATION.md           # GX → WebGPU technical reference
    └── LOW_END_GUIDE.md            # Low-end optimization cookbook
```

---

## C. CORE ENGINE BOOTSTRAP <a name="core-engine-bootstrap"></a>

### Engine.h

```cpp
#pragma once
#include <cstdint>
#include <memory>
#include "RuntimeMode.h"
#include "../ecs/World.h"
#include "../renderer/RenderDevice.h"
#include "../audio/AudioManager.h"
#include "../actor/ActorManager.h"
#include "../physics/PhysicsWorld.h"
#include "../mod/WonderModLoader.h"
#include "../io/FileSystem.h"

namespace StarShine {

struct EngineConfig {
    RuntimeMode mode         = RuntimeMode::Creator;
    uint32_t    targetFPS    = 60;
    bool        vsync        = true;
    bool        lowEndMode   = false;   // auto-detected at startup
    bool        enableMods   = true;
    const char* assetPackPath = nullptr;
};

class Engine {
public:
    static Engine& get();

    bool init(const EngineConfig& cfg);
    void tick();        // called from JS requestAnimationFrame
    void shutdown();

    ECS::World&       world()   { return *m_world; }
    RenderDevice&     render()  { return *m_render; }
    AudioManager&     audio()   { return *m_audio; }
    ActorManager&     actors()  { return *m_actors; }
    PhysicsWorld&     physics() { return *m_physics; }
    WonderModLoader&  mods()    { return *m_mods; }
    FileSystem&       fs()      { return *m_fs; }
    RuntimeMode       mode()  const { return m_config.mode; }

private:
    Engine() = default;
    EngineConfig m_config;

    std::unique_ptr<ECS::World>       m_world;
    std::unique_ptr<RenderDevice>     m_render;
    std::unique_ptr<AudioManager>     m_audio;
    std::unique_ptr<ActorManager>     m_actors;
    std::unique_ptr<PhysicsWorld>     m_physics;
    std::unique_ptr<WonderModLoader>  m_mods;
    std::unique_ptr<FileSystem>       m_fs;

    double m_lastTime = 0.0;
    double m_accumulator = 0.0;
    static constexpr double FIXED_STEP = 1.0 / 60.0;
};

} // namespace StarShine
```

### Engine.cpp — Tick Loop

```cpp
#include "Engine.h"
#include "../renderer/WebGPUDevice.h"
#include "../renderer/WebGLDevice.h"
#include <emscripten.h>
#include <emscripten/html5.h>

namespace StarShine {

Engine& Engine::get() {
    static Engine instance;
    return instance;
}

bool Engine::init(const EngineConfig& cfg) {
    m_config = cfg;

    // Auto-detect low-end hardware via navigator.deviceMemory + renderer string
    // Falls back from WebGPU → WebGL2 automatically
    bool webgpuAvailable = /* JS interop check */ false;
    EM_ASM({
        Module._webgpuAvail = (typeof navigator.gpu !== 'undefined');
    });
    webgpuAvailable = (bool)EM_ASM_INT({ return Module._webgpuAvail ? 1 : 0; });

    m_fs      = std::make_unique<FileSystem>();
    m_world   = std::make_unique<ECS::World>();
    m_audio   = std::make_unique<AudioManager>();
    m_actors  = std::make_unique<ActorManager>(*m_world);
    m_physics = std::make_unique<PhysicsWorld>();
    m_mods    = std::make_unique<WonderModLoader>();

    if (webgpuAvailable && !cfg.lowEndMode) {
        m_render = std::make_unique<WebGPUDevice>();
    } else {
        m_render = std::make_unique<WebGLDevice>();
    }

    if (!m_render->init()) return false;
    if (!m_audio->init())  return false;

    if (cfg.enableMods) {
        m_mods->scanAndLoad("mods/");
    }

    return true;
}

void Engine::tick() {
    double now = emscripten_get_now() / 1000.0;
    double dt  = now - m_lastTime;
    m_lastTime = now;
    if (dt > 0.1) dt = 0.1; // clamp spiral of death

    m_accumulator += dt;
    while (m_accumulator >= FIXED_STEP) {
        m_physics->step(FIXED_STEP);
        m_actors->fixedUpdate(FIXED_STEP);
        m_accumulator -= FIXED_STEP;
    }

    float alpha = (float)(m_accumulator / FIXED_STEP);
    m_actors->update(dt, alpha);
    m_render->beginFrame();
    m_render->renderScene(*m_world, alpha);
    m_render->endFrame();
    m_audio->update(dt);
}

void Engine::shutdown() {
    m_mods->unloadAll();
    m_render->shutdown();
    m_audio->shutdown();
}

} // namespace StarShine

// ---- WASM Exports called from JavaScript ----
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    int starshine_init(int mode, int lowEnd) {
        StarShine::EngineConfig cfg;
        cfg.mode       = (StarShine::RuntimeMode)mode;
        cfg.lowEndMode = (bool)lowEnd;
        return StarShine::Engine::get().init(cfg) ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE
    void starshine_tick() {
        StarShine::Engine::get().tick();
    }

    EMSCRIPTEN_KEEPALIVE
    void starshine_shutdown() {
        StarShine::Engine::get().shutdown();
    }
}
```

### JavaScript Bootstrap (engine.js)

```javascript
// engine.js — entry point loaded in index.html
import StarShineWASM from './starshine.js'; // emscripten-generated

export const StarShine = {
    _module: null,
    canvas: null,

    async init(canvas, options = {}) {
        this.canvas = canvas;
        this._module = await StarShineWASM({
            canvas,
            // Forward OPFS to C++ FileSystem
            locateFile: (f) => `/wasm/${f}`,
        });

        // Detect low-end: <4GB RAM, battery-class GPU
        const lowEnd = (navigator.deviceMemory ?? 4) < 4 ||
                       /intel.*UHD|mali|adreno 5/i.test(
                           (document.createElement('canvas')
                            .getContext('webgl')
                            ?.getExtension('WEBGL_debug_renderer_info')
                            ?.getParameter(37446)) ?? ''
                       );

        const mode = options.mode ?? 0; // 0=Creator, 1=Compat
        const ok = this._module._starshine_init(mode, lowEnd ? 1 : 0);
        if (!ok) throw new Error('StarShine failed to initialize');

        this._loop();
        return this;
    },

    _loop() {
        const tick = () => {
            this._module._starshine_tick();
            requestAnimationFrame(tick);
        };
        requestAnimationFrame(tick);
    },

    shutdown() {
        this._module._starshine_shutdown();
    }
};
```

---

## D. RENDERER: WebGPU + WebGL FALLBACK <a name="renderer"></a>

### RenderDevice.h — Abstract Interface

```cpp
#pragma once
#include "../ecs/World.h"

namespace StarShine {

class RenderDevice {
public:
    virtual ~RenderDevice() = default;
    virtual bool init()       = 0;
    virtual void shutdown()   = 0;
    virtual void beginFrame() = 0;
    virtual void renderScene(ECS::World& world, float alpha) = 0;
    virtual void endFrame()   = 0;

    // Shader/material creation
    virtual uint32_t createShader(const char* wgslOrGlsl,
                                  bool isVertex) = 0;
    virtual uint32_t createMaterial(uint32_t vs, uint32_t fs) = 0;
    virtual uint32_t uploadTexture(const uint8_t* rgba, int w, int h) = 0;
    virtual uint32_t uploadMesh(const void* verts, size_t vsize,
                                const void* idx,   size_t isize) = 0;

    bool isWebGPU() const { return m_isWebGPU; }
protected:
    bool m_isWebGPU = false;
};

} // namespace StarShine
```

### WebGPU Device (key implementation snippets)

```cpp
// WebGPUDevice.cpp — key sections

bool WebGPUDevice::init() {
    m_isWebGPU = true;
    // All WebGPU calls go through JS interop using Emscripten's
    // webgpu.h bindings (or raw EM_ASM / em_webgpu_init)
    int ok = EM_ASM_INT({
        const adapter = await navigator.gpu.requestAdapter({
            powerPreference: 'low-power'  // favor low-end!
        });
        if (!adapter) return 0;
        const device = await adapter.requestDevice({
            requiredLimits: {
                maxTextureDimension2D: 4096,  // conservative for low-end
                maxBindGroups: 4,
            }
        });
        Module._gpuDevice = device;
        Module._gpuQueue  = device.queue;
        return 1;
    });
    return ok == 1;
}

// Render pass setup — single render pass, forward rendering
void WebGPUDevice::beginFrame() {
    EM_ASM({
        Module._commandEncoder = Module._gpuDevice.createCommandEncoder();
        const texture = Module._gpuContext.getCurrentTexture();
        Module._renderPass = Module._commandEncoder.beginRenderPass({
            colorAttachments: [{
                view: texture.createView(),
                clearValue: { r: 0.1, g: 0.1, b: 0.15, a: 1.0 },
                loadOp:  'clear',
                storeOp: 'store',
            }],
            depthStencilAttachment: {
                view: Module._depthView,
                depthClearValue: 1.0,
                depthLoadOp:  'clear',
                depthStoreOp: 'store',
            }
        });
    });
}
```

### WebGL Fallback Device

The WebGL device mirrors the same interface but uses WebGL 2.0 calls. Key mappings:

| WebGPU concept      | WebGL 2.0 equivalent        |
|---------------------|-----------------------------|
| Render pass         | Framebuffer + drawArrays    |
| Bind group          | Uniform buffer objects      |
| Pipeline            | Program + VAO               |
| Storage buffer      | SSBO (WebGL2 ext)           |
| Compute shader      | Transform feedback fallback |
| WGSL shader         | GLSL 3.00 ES translation    |

A thin WGSL→GLSL transpiler (`ShaderTranspiler.cpp`) handles common patterns at load time.

---

## E. GX → WebGPU TRANSLATION <a name="gx-translation"></a>

The GC's GX subsystem is a fixed-function pipeline with TEV (Texture Environment) stages. We translate each TEV stage to WGSL/GLSL fragment shader code at load time.

### TEV Stage → WGSL Fragment

```cpp
// GXToWGPU.cpp

struct TEVStage {
    GXTevColorArg colorA, colorB, colorC, colorD;
    GXTevAlphaArg alphaA, alphaB, alphaC, alphaD;
    GXTevOp       colorOp, alphaOp;
    GXTevBias     bias;
    GXTevScale    scale;
    bool          clamp;
    GXTevRegID    outReg;
};

// Generate WGSL for a single TEV stage
std::string GXToWGPU::tevStageToWGSL(const TEVStage& s, int stageIdx) {
    auto colorArg = [&](GXTevColorArg arg) -> std::string {
        switch (arg) {
            case GX_CC_CPREV: return "tev_color_prev";
            case GX_CC_APREV: return "vec4f(tev_alpha_prev)";
            case GX_CC_C0:    return "tev_reg0";
            case GX_CC_TEXC:  return "tex_color";
            case GX_CC_RASC:  return "rast_color";
            case GX_CC_ONE:   return "vec4f(1.0)";
            case GX_CC_HALF:  return "vec4f(0.5)";
            case GX_CC_ZERO:  return "vec4f(0.0)";
            default:          return "vec4f(0.0)";
        }
    };

    std::string op;
    switch (s.colorOp) {
        case GX_TEV_ADD:  op = "+"; break;
        case GX_TEV_SUB:  op = "-"; break;
        default:          op = "+"; break;
    }

    float scaleVal = 1.0f;
    switch (s.scale) {
        case GX_CS_SCALE_2: scaleVal = 2.0f; break;
        case GX_CS_SCALE_4: scaleVal = 4.0f; break;
        case GX_CS_DIVIDE_2: scaleVal = 0.5f; break;
    }

    std::string clampStr = s.clamp ? "clamp(result, vec4f(0.0), vec4f(1.0))" : "result";

    char buf[512];
    snprintf(buf, sizeof(buf),
        "// TEV Stage %d\n"
        "{\n"
        "  let a%d = %s;\n"
        "  let b%d = %s;\n"
        "  let c%d = %s;\n"
        "  let d%d = %s;\n"
        "  var result = (d%d %s (a%d * (1.0 - c%d) + b%d * c%d)) * %ff;\n"
        "  tev_color_prev = %s;\n"
        "}\n",
        stageIdx,
        stageIdx, colorArg(s.colorA).c_str(),
        stageIdx, colorArg(s.colorB).c_str(),
        stageIdx, colorArg(s.colorC).c_str(),
        stageIdx, colorArg(s.colorD).c_str(),
        stageIdx, op.c_str(),
        stageIdx, stageIdx, stageIdx, stageIdx,
        scaleVal,
        clampStr.c_str()
    );
    return std::string(buf);
}

// Assemble full WGSL fragment shader from TEV stages
std::string GXToWGPU::buildFragmentShader(
    const std::vector<TEVStage>& stages,
    int numTextures)
{
    std::string src = R"(
@group(0) @binding(0) var<uniform> material: MaterialUniforms;
@group(1) @binding(0) var tex0: texture_2d<f32>;
@group(1) @binding(1) var tex_sampler: sampler;

struct FragIn {
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
    @location(2) normal: vec3f,
}

@fragment
fn fs_main(in: FragIn) -> @location(0) vec4f {
    var tev_color_prev = vec4f(0.0);
    var tev_alpha_prev = 0.0;
    var tev_reg0 = material.color0;
    var rast_color = in.color;
    var tex_color = textureSample(tex0, tex_sampler, in.uv);
)";

    for (int i = 0; i < (int)stages.size(); i++) {
        src += tevStageToWGSL(stages[i], i);
    }

    src += R"(
    return tev_color_prev;
}
)";
    return src;
}
```

### Texture Format Decompression (CMPR → RGBA8)

```cpp
// GXTextureFormat.cpp — CMPR (S3TC DXT1 variant) decompressor

void decompressCMPR(
    const uint8_t* src, int width, int height,
    uint8_t* dst /* width*height*4 RGBA output */)
{
    // CMPR tiles are 8x8, each containing four 4x4 DXT1 sub-blocks
    // arranged in a 2x2 grid within the tile
    for (int ty = 0; ty < height; ty += 8) {
        for (int tx = 0; tx < width; tx += 8) {
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    decompressDXT1Block(src,
                        dst, width,
                        tx + sx*4, ty + sy*4);
                    src += 8;
                }
            }
        }
    }
}
```

---

## F. ACTOR SYSTEM <a name="actor-system"></a>

### ActorBase.h

```cpp
#pragma once
#include <cstdint>
#include "../ecs/World.h"

namespace StarShine {

enum class ActorMode { Compat, Creator, Both };

class ActorBase {
public:
    virtual ~ActorBase() = default;

    // Lifecycle
    virtual void onCreate(ECS::World& world) {}
    virtual void onDestroy(ECS::World& world) {}
    virtual void fixedUpdate(float dt) {}
    virtual void update(float dt, float alpha) {}

    // Mod hooks — called before/after each lifecycle event
    virtual void onModPreCreate()  {}
    virtual void onModPostUpdate() {}

    uint32_t    id   = 0;
    ActorMode   mode = ActorMode::Both;
    bool        active = true;
    const char* name = "Actor";
};

// ---- MODE A: Compatibility Actors ----
// These reimplement the exact behavior of original game actors.
// They read from BCSV/JMap data tables, not from ECS components,
// to preserve bit-identical behavior with the original games.

class SunshineActor : public ActorBase {
protected:
    struct Transform {
        float x, y, z;
        float rx, ry, rz;
        float sx = 1, sy = 1, sz = 1;
    } transform;

    virtual void readBCSVParams(const BCSVEntry& entry) {}
};

// ---- MODE B: Creator Actors ----
// These are Unity-style components attached to ECS entities.
// Scripts written in ConstellationScript or Lua attach to these.

class CreatorActor : public ActorBase {
public:
    ECS::EntityID entity = ECS::INVALID_ENTITY;

    template<typename T>
    T* getComponent() {
        return ECS::World::current().get<T>(entity);
    }

    template<typename T>
    T& addComponent(T&& comp) {
        return ECS::World::current().emplace<T>(entity, std::move(comp));
    }
};

} // namespace StarShine
```

### GravityField Actor (Mode A — Galaxy-style)

```cpp
// GravityField.cpp — reimplements SMG gravity zones exactly

class GravityField : public SunshineActor {
public:
    enum class Type { Sphere, Box, Cylinder, Disk, Point, Global };

    Type  type      = Type::Sphere;
    float strength  = 1.0f;
    float range     = 500.0f;
    int   priority  = 0;

    // Returns gravity direction at world position pos
    glm::vec3 getGravityAt(glm::vec3 pos, glm::vec3 objPos) {
        switch (type) {
            case Type::Sphere:
                return glm::normalize(glm::vec3(transform.x,
                                                transform.y,
                                                transform.z) - objPos)
                       * -strength;
            case Type::Global:
                return glm::vec3(0, -1, 0) * strength;
            case Type::Disk: {
                // Project to disk plane, attract inward
                glm::vec3 center(transform.x, transform.y, transform.z);
                glm::vec3 up = localUp();
                glm::vec3 diff = objPos - center;
                glm::vec3 lateral = diff - glm::dot(diff, up) * up;
                if (glm::length(lateral) < 0.001f) return -up * strength;
                return glm::normalize(lateral) * -strength;
            }
            default:
                return glm::vec3(0, -1, 0) * strength;
        }
    }
private:
    glm::vec3 localUp() { /* from transform rotation */ return {0,1,0}; }
};
```

---

## G. PHYSICS & ANIMATION <a name="physics-animation"></a>

### PhysicsWorld.h — Key Interface

```cpp
class PhysicsWorld {
public:
    // Galaxy-style: each object can have a different gravity source
    void registerGravityField(GravityField* field);

    // Returns the dominant gravity vector for a point in space
    glm::vec3 sampleGravity(glm::vec3 worldPos, int layer);

    void addRigidBody(RigidBody* body);
    void step(float dt);

    // Sunshine water volumes (FLUDD interaction)
    bool isInWater(glm::vec3 pos) const;
    float waterDepth(glm::vec3 pos) const;

private:
    std::vector<GravityField*>  m_gravFields;
    std::vector<RigidBody*>     m_bodies;
    BVH                         m_staticBVH;  // static geometry
    // Fixed-step integration at 60Hz to match original games exactly
    void integrate(RigidBody* b, float dt);
    void resolveCollisions();
};
```

### AnimationPlayer — BCK/BCA Format

```cpp
// AnimationPlayer.cpp
// BCK = bone animation (J3D), BCA = character animation (Galaxy)

struct BoneTrack {
    std::vector<float> times;
    std::vector<glm::vec3> positions;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> scales;
};

class AnimationPlayer {
    std::vector<BoneTrack> m_tracks;
    float m_time = 0.0f;
    float m_duration = 0.0f;
    bool  m_loop = true;

public:
    void loadBCK(const uint8_t* data, size_t size);

    void update(float dt) {
        m_time += dt;
        if (m_loop) m_time = fmod(m_time, m_duration);
    }

    // Returns interpolated pose for a bone index
    glm::mat4 getBoneMatrix(int boneIdx) const {
        const auto& track = m_tracks[boneIdx];
        // Hermite spline interpolation (matching original HSD_Animate)
        float t = m_time;
        int frame = findFrame(track.times, t);
        float blend = (t - track.times[frame]) /
                      (track.times[frame+1] - track.times[frame]);
        glm::vec3 pos = glm::mix(track.positions[frame],
                                 track.positions[frame+1], blend);
        glm::quat rot = glm::slerp(track.rotations[frame],
                                   track.rotations[frame+1], blend);
        glm::vec3 scl = glm::mix(track.scales[frame],
                                 track.scales[frame+1], blend);

        return glm::translate(glm::mat4(1), pos)
             * glm::mat4_cast(rot)
             * glm::scale(glm::mat4(1), scl);
    }
};
```

---

## H. ASSET EXTRACTION PIPELINE <a name="asset-pipeline"></a>

### ISO Extractor (toolchain/extractor/ISOExtractor.cpp)

```cpp
// Reads a GameCube/Wii ISO and extracts the filesystem.
// Input: ArrayBuffer from File API in browser
// Output: Files written to OPFS (Origin Private File System)

class ISOExtractor {
public:
    bool extract(const uint8_t* isoData, size_t isoSize,
                 const char* outputDir)
    {
        // GC ISO layout:
        // 0x0000: Disc header (0x440 bytes)
        // 0x0440: Apploader
        // 0x2440: Main DOL
        // Filesystem begins at offset stored in header
        uint32_t fstOffset = read32BE(isoData + 0x0424);
        uint32_t fstSize   = read32BE(isoData + 0x0428);
        uint32_t dataOffset = read32BE(isoData + 0x042C);

        const uint8_t* fst = isoData + fstOffset;
        return extractDirectory(isoData, fst, fstSize, outputDir, 0);
    }

private:
    bool extractDirectory(const uint8_t* iso, const uint8_t* fst,
                          uint32_t fstSize, const char* outDir,
                          uint32_t dirIdx)
    {
        // ... standard GC FST traversal ...
        return true;
    }
    uint32_t read32BE(const uint8_t* p) {
        return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
    }
};
```

### .ssp Asset Pack Format

```
StarShine Pack (.ssp) format:
─────────────────────────────
Offset  Size  Description
0x00    4     Magic: "SSPE"
0x04    4     Version: 0x00010000
0x08    4     Entry count
0x0C    4     String table offset
0x10    N*20  Entry table:
              [0]  4  Name hash (FNV-1a)
              [4]  4  Name string offset
              [8]  4  Data offset
              [12] 4  Compressed size
              [16] 4  Original size
              ---- data follows (Zstd compressed) ----
```

---

## I. WASM BUILD INSTRUCTIONS <a name="wasm-build"></a>

### Prerequisites

```bash
# 1. Install Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 3.1.50
./emsdk activate 3.1.50
source ./emsdk_env.sh   # Add to shell profile

# 2. Install dependencies
sudo apt install cmake ninja-build python3 nodejs npm

# 3. Verify
emcc --version   # should print 3.1.50
```

### CMakeLists.txt (root)

```cmake
cmake_minimum_required(VERSION 3.20)
project(StarShine CXX)
set(CMAKE_CXX_STANDARD 17)

# Emscripten flags
if (EMSCRIPTEN)
  set(EM_FLAGS
    -O2
    -sUSE_WEBGPU=1
    -sUSE_GLFW=3
    -sUSE_SDL=0
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=67108864      # 64MB initial
    -sMAXIMUM_MEMORY=1073741824    # 1GB max
    -sEXPORTED_FUNCTIONS=_starshine_init,_starshine_tick,_starshine_shutdown,_malloc,_free
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8,HEAPU32
    -sENVIRONMENT=web
    -sASSERTIONS=0
    --no-entry
  )
  # Low-end build removes WebGPU, uses WebGL2 only
  if (STARSHINE_LOW_END)
    list(FILTER EM_FLAGS EXCLUDE REGEX "USE_WEBGPU")
    list(APPEND EM_FLAGS -DSTARSHINE_FORCE_WEBGL=1)
  endif()

  add_link_options(${EM_FLAGS})
endif()

# Source files
file(GLOB_RECURSE ENGINE_SOURCES engine/*.cpp)
add_executable(starshine ${ENGINE_SOURCES})
target_include_directories(starshine PRIVATE engine/ third_party/)
target_compile_definitions(starshine PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
    STARSHINE_VERSION="0.1.0"
)
```

### Build Commands

```bash
# Create build directory
mkdir build && cd build

# Configure for WASM
emcmake cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTARSHINE_MODE=CREATOR   # or COMPAT

# Build
ninja -j$(nproc)

# Output: build/starshine.js + build/starshine.wasm
# Copy to web server's /wasm/ directory

# Low-end build (no WebGPU, smaller binary)
emcmake cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTARSHINE_LOW_END=ON
ninja -j$(nproc)
```

### Serving Locally

```bash
# WASM requires certain headers. Use this dev server:
npm install -g serve

# Or use the included server (sets COOP/COEP headers required for SharedArrayBuffer):
node tools/dev-server.js --port 8080

# Then open: http://localhost:8080/constellation/
```

Required HTTP headers for WASM threads:
```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

---

## J. CONSTELLATION EDITOR <a name="constellation-editor"></a>

### Architecture

```
Constellation is a single-page web application that communicates
with the StarShine WASM core via a typed message bridge.

Layout: Three-panel dark UI (inspired by Godot 4 + Unity Editor)
┌──────────────────────────────────────────────────────────────┐
│  Menu Bar  (File | Edit | Asset | Build | Mods | Help)       │
├────────────┬─────────────────────────────────┬───────────────┤
│ Hierarchy  │                                 │  Inspector    │
│ (tree of   │     3D Viewport (WebGPU)        │  (selected    │
│  entities) │                                 │  entity       │
│            │    [Play ▶] [Pause ⏸] [Stop ■] │  components)  │
│            │                                 │               │
├────────────┴─────────────────────────────────┴───────────────┤
│  Asset Browser  │  Console  │  Mod Debugger  │  Shader Graph │
└──────────────────────────────────────────────────────────────┘
```

### EngineBridge.ts — JS ↔ WASM Communication

```typescript
// EngineBridge.ts
export class EngineBridge {
    private module: EmscriptenModule;
    private listeners = new Map<string, ((data: unknown) => void)[]>();

    async load(wasmPath: string): Promise<void> {
        this.module = await StarShineWASM({ locateFile: () => wasmPath });
    }

    // Strongly-typed WASM calls
    init(mode: 'creator' | 'compat', lowEnd = false): boolean {
        return this.module._starshine_init(
            mode === 'creator' ? 0 : 1,
            lowEnd ? 1 : 0
        ) === 1;
    }

    // ECS operations
    createEntity(): number {
        return this.module._ecs_create_entity();
    }

    addComponent(entity: number, componentType: string, data: object): void {
        const json = JSON.stringify(data);
        const ptr  = this.module.allocateUTF8(json);
        this.module._ecs_add_component(entity,
            this.module.allocateUTF8(componentType), ptr);
        this.module._free(ptr);
    }

    // Scene serialization
    saveScene(): string {
        const ptr = this.module._scene_serialize();
        return this.module.UTF8ToString(ptr);
    }

    loadScene(json: string): void {
        const ptr = this.module.allocateUTF8(json);
        this.module._scene_deserialize(ptr);
        this.module._free(ptr);
    }

    // Mod operations
    loadMod(wondermodBuffer: ArrayBuffer): boolean {
        const ptr = this.module._malloc(wondermodBuffer.byteLength);
        this.module.HEAPU8.set(new Uint8Array(wondermodBuffer), ptr);
        const result = this.module._mod_load_buffer(ptr,
                            wondermodBuffer.byteLength);
        this.module._free(ptr);
        return result === 1;
    }
}
```

### Inspector Component (React-style pseudo-code)

```typescript
// Inspector.ts — renders component properties for selected entity
class Inspector {
    render(entity: EntityID): HTMLElement {
        const panel = this.createPanel('Inspector');

        const components = bridge.getComponents(entity);
        for (const comp of components) {
            const section = this.createSection(comp.type);

            for (const [key, value] of Object.entries(comp.props)) {
                const field = this.createField(key, value, (newVal) => {
                    bridge.setComponentProp(entity, comp.type, key, newVal);
                });
                section.append(field);
            }

            panel.append(section);
        }

        // "Add Component" button
        const addBtn = this.createButton('Add Component', () => {
            this.showComponentPicker(entity);
        });
        panel.append(addBtn);

        return panel;
    }

    createField(name: string, value: unknown, onChange: Function): HTMLElement {
        if (typeof value === 'number') return new NumberField(name, value, onChange);
        if (typeof value === 'boolean') return new CheckboxField(name, value, onChange);
        if (typeof value === 'string') return new TextField(name, value, onChange);
        if (isVec3(value)) return new Vec3Field(name, value, onChange);
        if (isColor(value)) return new ColorPicker(name, value, onChange);
        return new JsonField(name, value, onChange);
    }
}
```

---

## K. CONSTELLATIONSCRIPT LANGUAGE SPEC <a name="constellationscript"></a>

### Recommendation & Rationale

After evaluation, **we implement both**:
- **ConstellationScript** for mod scripts and game logic (lightweight, sandboxed, browser-safe)
- **Lua 5.4** as the power-user fallback (more ecosystem, proven in modding)

ConstellationScript fills the gap where Lua is too heavy for hot-path actor logic and where raw C++ is too locked-down for mod authors.

### ConstellationScript Syntax

```cscript
// ConstellationScript v0.1 — example actor script

import { Actor, Vec3, Input, Audio } from "starshine:engine"
import { on, emit }                  from "starshine:events"

// Class-style actors with ECS bindings
actor PlayerController extends Actor {
    speed: f32 = 400.0
    jumpForce: f32 = 800.0
    grounded: bool = false

    // Lifecycle coroutine — runs once at spawn
    async onCreate() {
        this.rb = this.getComponent(RigidBody)
        this.anim = this.getComponent(AnimationPlayer)
        await Audio.preload("jump.wav")
        emit("player:spawned", { pos: this.position })
    }

    // Fixed update — runs at 60Hz
    onFixedUpdate(dt: f32) {
        let move = Vec3(
            Input.axis("horizontal"),
            0.0,
            Input.axis("vertical")
        )

        if move.length() > 0.1 {
            this.rb.velocity.xz = move.normalized() * this.speed
        }

        if Input.pressed("jump") && this.grounded {
            this.rb.velocity.y = this.jumpForce
            Audio.play("jump.wav")
            this.anim.play("jump", once: true)
        }
    }

    // Collision callback
    onCollision(other: Actor, normal: Vec3) {
        if normal.y > 0.7 {
            this.grounded = true
        }
    }
}

// Mod patch example — override an existing actor's behavior
// (used in .wondermod scripts)
patch actor "MarioActor" {
    after onFixedUpdate(dt: f32) {
        // Add custom behavior after original runs
        if Input.pressed("custom_action") {
            emit("mod:custom_action")
        }
    }
}
```

### VM Architecture

- **Compile target**: Custom bytecode → WASM linear memory (AOT)
- **Sandbox**: Each script runs in isolated linear memory segment, no raw pointer access, no FFI except through declared `import` statements
- **Coroutines**: Stackful coroutines via WASM `asyncify` or manual stack-switching
- **JIT**: Tier-1 interpreter for cold code, tier-2 LLVM-based JIT for hot loops (falls back to interpreter on low-end)
- **ECS bindings**: Zero-copy: script gets a typed handle to component data; no serialization overhead

---

## L. WONDERMOD FRAMEWORK <a name="wondermod"></a>

### .wondermod Bundle Format

A `.wondermod` file is a ZIP archive with this structure:

```
my_mod.wondermod (ZIP)
├── manifest.json              ← Required: mod metadata
├── scripts/
│   ├── main.cscript           ← Entry point (ConstellationScript)
│   └── helpers.lua            ← Optional Lua helpers
├── assets/
│   ├── textures/
│   │   └── mario_alt.png      ← Texture overrides
│   ├── models/
│   │   └── custom_enemy.glb   ← Model overrides
│   └── sounds/
│       └── custom_jump.wav
├── data/
│   └── ObjInfo.bcsv.patch     ← BCSV data table patches (JSON diff)
├── actors/
│   └── CustomBoss.cscript     ← New actor definitions
└── worlds/
    └── CustomGalaxy/          ← New level/world data
        ├── world.json
        └── geometry.glb
```

### manifest.json Schema

```json
{
    "id":          "com.example.my_mod",
    "name":        "My Awesome Mod",
    "version":     "1.2.0",
    "api_version": "0.1.0",
    "target":      "compat",       // "compat" | "creator" | "both"
    "game":        "galaxy",       // "sunshine" | "galaxy" | "galaxy2" | null
    "author":      "Your Name",
    "description": "Does cool things",
    "dependencies": [
        { "id": "com.example.dep_mod", "version": ">=1.0.0" }
    ],
    "hooks": {
        "file_replace": [
            { "original": "StageData/HeavensDoorGalaxy/StageMap.arc",
              "replacement": "assets/StageMap_custom.arc" }
        ],
        "actor_inject": [
            { "class": "CustomBoss", "script": "actors/CustomBoss.cscript" }
        ],
        "shader_override": [
            { "material_id": "mario_body", "shader": "scripts/mario_shader.wgsl" }
        ],
        "bcsv_patch": [
            { "file": "ObjInfo.bcsv", "patch": "data/ObjInfo.bcsv.patch" }
        ]
    },
    "sandbox": {
        "allow_network": false,
        "allow_file_write": false,
        "max_memory_mb": 32,
        "script_timeout_ms": 100
    }
}
```

### WonderModLoader.cpp — Core Implementation

```cpp
// WonderModLoader.cpp

struct WonderMod {
    std::string id;
    std::string name;
    std::string version;
    ModManifest manifest;
    // File replacement map: original path → mod-internal path
    std::unordered_map<std::string, std::string> fileHooks;
    // Active scripts
    std::vector<std::unique_ptr<SandboxedScript>> scripts;
    bool loaded = false;
};

bool WonderModLoader::loadBundle(const uint8_t* zipData, size_t size) {
    // 1. Parse ZIP
    auto zip = ZipReader(zipData, size);
    if (!zip.valid()) {
        LOG_ERROR("wondermod: invalid ZIP");
        return false;
    }

    // 2. Read manifest
    auto manifestJson = zip.read("manifest.json");
    WonderMod mod;
    if (!parseManifest(manifestJson, mod.manifest)) return false;
    mod.id = mod.manifest.id;
    mod.version = mod.manifest.version;

    // 3. Check API version compatibility
    if (!isApiCompatible(mod.manifest.api_version)) {
        LOG_ERROR("wondermod: incompatible API version %s",
                  mod.manifest.api_version.c_str());
        return false;
    }

    // 4. Dependency resolution
    for (auto& dep : mod.manifest.dependencies) {
        if (!isModLoaded(dep.id, dep.versionReq)) {
            LOG_ERROR("wondermod: missing dependency %s", dep.id.c_str());
            return false;
        }
    }

    // 5. Register file hooks
    for (auto& hook : mod.manifest.hooks.fileReplace) {
        auto fileData = zip.read(hook.replacement);
        m_fileOverrides[hook.original] = std::move(fileData);
        LOG("wondermod: file hook %s", hook.original.c_str());
    }

    // 6. Register actor injections
    for (auto& inject : mod.manifest.hooks.actorInject) {
        auto scriptSrc = zip.read(inject.script);
        auto sandbox = std::make_unique<ModScriptSandbox>(
            mod.manifest.sandbox);
        sandbox->loadScript(scriptSrc.data(), scriptSrc.size());
        m_actorScripts[inject.className] = std::move(sandbox);
    }

    // 7. Apply BCSV patches
    for (auto& patch : mod.manifest.hooks.bcsvPatch) {
        auto patchData = zip.read(patch.patchFile);
        applyBCSVPatch(patch.targetFile, patchData);
    }

    // 8. Load scripts
    for (auto& entry : zip.entries()) {
        if (entry.ends_with(".cscript")) {
            auto src = zip.read(entry);
            auto sandbox = std::make_unique<ModScriptSandbox>(
                mod.manifest.sandbox);
            sandbox->loadScript(src.data(), src.size());
            mod.scripts.push_back(std::move(sandbox));
        }
    }

    mod.loaded = true;
    m_mods[mod.id] = std::move(mod);
    LOG("wondermod: loaded %s v%s", mod.id.c_str(), mod.version.c_str());
    return true;
}

// Called by FileSystem when opening a game file —
// intercepts and returns mod-overridden data if present
std::optional<std::vector<uint8_t>>
WonderModLoader::interceptFile(const std::string& path) {
    auto it = m_fileOverrides.find(path);
    if (it != m_fileOverrides.end()) {
        LOG("wondermod: intercepted %s", path.c_str());
        return it->second;
    }
    return std::nullopt;
}
```

### Stable Mod API (docs/MOD_API.md)

The Mod API is versioned separately from the engine. Mods target `api_version: "0.1.0"` and the engine guarantees backward compatibility within a major version.

**Available in ConstellationScript mods:**

```typescript
// starshine:engine module
Actor, Vec3, Vec2, Quat, Mat4, Color
Input, Audio, Physics, Scene, Camera

// starshine:hooks module
on(event: string, handler: Function): unsubscribe
emit(event: string, data: any): void
registerFileHook(path: string, provider: () => ArrayBuffer): void
registerActorFactory(name: string, factory: () => Actor): void
registerShaderOverride(materialId: string, wgslCode: string): void

// starshine:data module (Mode A only)
readBCSV(file: string): BCSVTable
patchBCSV(file: string, patch: BCSVPatch): void
readJMap(file: string): JMapInfo
patchJMap(file: string, patch: JMapPatch): void

// starshine:ui module (Constellation editor integration)
registerDebugPanel(name: string, render: () => HTMLElement): void
registerInspectorField(componentType: string, field: InspectorField): void
```

---

## M. OPTIMIZATION STRATEGY <a name="optimization"></a>

### Tier Detection at Runtime

```javascript
// Runs at startup, selects quality preset
function detectHardwareTier() {
    const gl = document.createElement('canvas').getContext('webgl2');
    const renderer = gl?.getExtension('WEBGL_debug_renderer_info')
                    ?.getParameter(37446) ?? '';

    const ram = navigator.deviceMemory ?? 4;
    const cores = navigator.hardwareConcurrency ?? 2;

    // Tier 0: Old laptop / school Chromebook (Intel UHD 620, 4GB RAM)
    if (ram <= 4 || /intel.*UHD 6[0-2]/i.test(renderer) ||
        /mali-[gt][0-9]/i.test(renderer)) {
        return 'low';
    }
    // Tier 1: Mid-range (GTX 1050/RX 570 class, 8GB)
    if (ram <= 8 || cores <= 4) return 'medium';
    // Tier 2: Capable gaming machine
    return 'high';
}
```

### Quality Preset Settings

| Setting              | Low (school laptop) | Medium        | High           |
|----------------------|---------------------|---------------|----------------|
| Render resolution    | 640×360 (upscaled)  | 1280×720      | Native         |
| Shadow map           | Off                 | 512×512 PCF   | 2048×2048 PCF  |
| Texture size cap     | 512×512             | 1024×1024     | 4096×4096      |
| Anti-aliasing        | None                | FXAA          | TAA            |
| Draw distance        | 150 units           | 400 units     | 1000 units     |
| Particle count       | 32                  | 256           | 2048           |
| Post-processing      | None                | Bloom (cheap) | Full           |
| Physics substeps     | 1                   | 2             | 2              |
| Audio voices         | 8                   | 24            | 64             |
| Texture compression  | ETC2 (WASM decode)  | ETC2/ASTC     | ASTC/BC7       |
| WASM threads         | 1 (single)          | 2             | 4              |

### Memory Pool Strategy

```cpp
// MemoryPool.h — three pool types for different lifetimes

class FrameArena {
    // Reset every frame — for per-frame scratch data
    // ~1MB, zero fragmentation, O(1) alloc
    uint8_t buf[1 * 1024 * 1024];
    size_t  cursor = 0;
public:
    void* alloc(size_t n) {
        void* p = buf + cursor;
        cursor = (cursor + n + 7) & ~7; // 8-byte align
        return p;
    }
    void reset() { cursor = 0; }
};

class SlabPool {
    // Fixed-size objects (actors, particles, etc.)
    // Perfect for objects with known size and high churn
};

class LevelArena {
    // Cleared on level load — for all level geometry/asset data
    // ~256MB, grows as needed within WASM heap
};
```

### Rendering Optimizations for Low-End

1. **Occlusion culling**: Simple portal/cell system for enclosed levels
2. **LOD system**: 3 LOD levels per mesh (full, 50%, 25%)
3. **Texture streaming**: Only keep visible textures resident; evict LRU
4. **Batched draw calls**: Merge static geometry into single draw call per material
5. **Half-precision floats**: Use `f16` in WGSL where precision allows (lighting, UVs)
6. **Reduced render scale + bilinear upscale**: 0.5× resolution on Tier 0

---

## N. MULTI-MODE RUNTIME LINKING <a name="runtime-linking"></a>

The engine uses a **Runtime Registry** pattern. Subsystems register themselves with mode tags. The engine loads only the subsystems relevant to the active mode.

```cpp
// RuntimeMode.h
enum class RuntimeMode { Creator = 0, Compat = 1 };

class SubsystemRegistry {
public:
    template<typename T>
    void registerSubsystem(RuntimeMode mode, std::unique_ptr<T> sys) {
        m_systems[mode].push_back(std::move(sys));
    }

    void activateMode(RuntimeMode mode) {
        for (auto& sys : m_systems[mode]) sys->init();
        for (auto& sys : m_systems[RuntimeMode(-1)]) sys->init(); // "Both"
    }
};
```

Mode A (Compat) adds: `GXTranslator`, `JMapLoader`, `BCSVReader`, `CompatCamera`, `DSPDecoder`
Mode B (Creator) adds: `ConstellationScriptVM`, `ShaderGraph`, `EditorBridge`, `ProjectManager`
Both modes share: `ECS`, `PhysicsWorld`, `RenderDevice`, `AudioManager`, `WonderModLoader`

---

## O. EXAMPLE TEST LEVELS <a name="example-level"></a>

### Mode A — Galaxy-style Test Level

```cpp
// examples/test_level_compat/TestLevel.cpp
// Spawns a spherical planet with gravity, a moving platform,
// and a simple coin to collect — exactly as Galaxy would.

void TestLevel::load(Engine& engine) {
    // Spawn planet (sphere gravity field)
    auto planet = engine.actors().spawn<SphereGravityPlanet>();
    planet->radius = 600.0f;
    planet->position = {0, 0, 0};

    // Mario starts 700 units above planet surface
    auto mario = engine.actors().spawn<MarioActor>();
    mario->position = {0, 700, 0};

    // Simple rotating platform
    auto platform = engine.actors().spawn<RotatingPlatform>();
    platform->position = {200, 680, 0};
    platform->rotSpeed = 30.0f; // degrees/sec

    // Coin (collected on contact)
    auto coin = engine.actors().spawn<StarBitActor>();
    coin->position = {100, 710, 0};
}
```

### Mode B — Original Game Test Level

```cscript
// examples/test_level_creator/level.cscript

import { Scene, Vec3 } from "starshine:engine"

// Spawns a floating island platformer scene
on "scene:load" {
    Scene.addMesh("island.glb", at: Vec3(0, 0, 0))
    Scene.addDirectionalLight(
        direction: Vec3(-1, -2, -1).normalized(),
        color: Color.white,
        intensity: 1.2
    )
    Scene.spawnActor("PlatformerController",
        at: Vec3(0, 10, 0))
    Scene.spawnActor("CollectibleCoin", at: Vec3(5, 12, 0))
    Scene.spawnActor("CollectibleCoin", at: Vec3(-5, 15, 3))
}
```

---

## P. TECH DEMO SETUP <a name="tech-demo"></a>

### What the Tech Demo Showcases

The tech demo (`examples/tech_demo/`) runs entirely in-browser and demonstrates:

1. **A floating island** with procedurally animated water
2. **A controllable character** with Galaxy-style sphere gravity
3. **Particle system** (sparkles, water splashes)
4. **Dynamic lighting** (point light orbiting the planet)
5. **WonderMod loading** (a live "color swap" mod you can enable in-demo)
6. **ConstellationScript** (the character controller is written in CScript, visible in source)
7. **Runs at 60fps on an Intel UHD 620** (school laptop quality)

### Importing the Tech Demo into Constellation

1. **Open Constellation** at `http://localhost:8080/constellation/`
2. Click **File → Open Project**
3. Navigate to `examples/tech_demo/` and select `project.ssproject`
4. Constellation loads the scene. You'll see:
   - The island geometry in the 3D viewport
   - "PlayerController" entity in the Hierarchy panel
   - Its ConstellationScript component visible in the Inspector
5. Click **▶ Play** to run the demo in-editor
6. To inspect the mod: click **Mods → Load Mod**, select `examples/tech_demo/demo_colorswap.wondermod`
7. Enable the mod toggle — the island changes color in real-time (this demos the live shader override hook)

### Building the Tech Demo Standalone

```bash
cd build
ninja tech_demo

# Output: build/tech_demo/
#   index.html
#   demo.js
#   starshine.wasm
#   assets/

# Serve it:
npx serve build/tech_demo -p 8080
```

---

## Q. ROADMAP & EXTENSION POINTS <a name="roadmap"></a>

### v0.2 — Wonder Integration
- Wonder's Mod Center API integration (mod upload/download/rating)
- Mod signing & verification (ECDSA)
- Auto-update for mods

### v0.3 — Network Play
- Lockstep multiplayer for Creator Mode games
- Galaxy co-op reimplementation for Compat Mode

### v0.4 — Mobile Targets
- Touch input system
- WebXR/VR support
- PWA packaging for Android/iOS

### v0.5 — Decompilation Assist
- Symbol database for Sunshine/Galaxy/2 (via community decomp projects)
- Automatic function stub generation
- Diff tool for mod patching vs. original binary

### Stable Mod API Promise
The `starshine:engine`, `starshine:hooks`, `starshine:data`, and `starshine:ui` APIs
are **stable from v0.1 onward**. Any breaking change requires a major version bump
and a minimum 6-month deprecation window. Mods targeting `api_version: "0.1.0"` will
continue working through all 0.x releases.

---

*StarShine Engine — Built for wonder.*
*The engine of Constellation, the heart of Wonder.*
