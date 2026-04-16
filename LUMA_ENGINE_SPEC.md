# LUMA ENGINE — Technical Specification
## Project Starshine · Component 1 of 3
**Version 0.1.0-draft | Based on Petari (SMGCommunity/Petari) decomp of Super Mario Galaxy 1**

---

## 1. OVERVIEW

Luma Engine is the Galaxy-compatible runtime layer of Project Starshine. It is a faithful re-implementation of Super Mario Galaxy 1's engine subsystems, informed directly by the Petari decompilation project and the SMGCommunity's accumulated reverse-engineering work on both Galaxy games.

Its goal is to accept a user-supplied Galaxy 1 or Galaxy 2 ISO, extract assets on first run, and then execute the game's logic using re-implemented native subsystems — compiled to WebAssembly — with the GX render pipeline replaced by a WebGPU/WebGL2 backend.

It is **not** a PC port. It is a clean-room re-implementation of documented behavior, in the same tradition as Ship of Harkinian for Ocarina of Time. No copyrighted code is included.

### What Luma Engine covers
- Super Mario Galaxy 1 (RMGE01 / RMGK01 / RMGP01 — all regions)
- Super Mario Galaxy 2 (RMGE01 / RMGK01 — all regions, via Garigari decomp cross-reference)

Delfino Engine (separate component) covers Super Mario Sunshine.

---

## 2. ARCHITECTURE

```
┌─────────────────────────────────────────────────────────────┐
│                       LUMA ENGINE                            │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐ │
│  │  ISO / WBFS  │  │  WonderMod   │  │  Constellation    │ │
│  │  Extractor   │  │  (.wondermod)│  │  Editor Bridge    │ │
│  └──────┬───────┘  └──────┬───────┘  └─────────┬─────────┘ │
│         └─────────────────┼──────────────────────┘          │
│                           │                                  │
│              ┌────────────▼────────────┐                    │
│              │     Asset Layer          │                    │
│              │  SZS/Yaz0 decompressor  │                    │
│              │  RARC archive reader    │                    │
│              │  BCSV / JMapInfo parser │                    │
│              │  BTI texture decoder    │                    │
│              │  BMD/BDL model reader   │                    │
│              │  BCK/BCA/BTP anim reader│                    │
│              │  BRSTM audio reader     │                    │
│              └────────────┬────────────┘                    │
│                           │                                  │
│  ┌────────────────────────▼─────────────────────────────┐  │
│  │                   Game Subsystems                      │  │
│  │                                                        │  │
│  │  NameObj     │ LiveActor    │ GravityCreator           │  │
│  │  hierarchy   │ system       │ (sphere/box/disk/cyl)   │  │
│  │              │              │                          │  │
│  │  ScenarioHolder  │  StageDataHolder  │  ZoneHolder    │  │
│  │  (JMapInfo-driven scenario loading)                   │  │
│  │                                                        │  │
│  │  MarioActor  │  CameraController  │  EffectSystem     │  │
│  │  (full move- │  (GalaxyCam exact) │  (JPA particles)  │  │
│  │   set reimpl)│                    │                    │  │
│  │                                                        │  │
│  │  PhysicsHolder  │  CollisionDetector  │  WaterHolder  │  │
│  │                                                        │  │
│  │  AudioSystem (DSP ADPCM → WebAudio)                   │  │
│  └────────────────────────┬─────────────────────────────┘  │
│                           │                                  │
│              ┌────────────▼────────────┐                    │
│              │    Render Backend        │                    │
│              │  GX TEV → WebGPU/WebGL2 │                    │
│              │  BMD → GPU mesh upload  │                    │
│              │  BTI → texture upload   │                    │
│              │  Shadow/bloom/DoF       │                    │
│              └─────────────────────────┘                    │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. SOURCE FIDELITY POLICY

Luma Engine subsystems are implemented in priority order of fidelity:

| Priority | Behavior |
|---|---|
| **1 — Exact match** | Physics integration, gravity sampling, collision response, actor movement speeds, jump arcs. These are bit-for-bit matched against documented Petari values. |
| **2 — Behavioral match** | Camera follow curves, audio playback timing, particle emission. Correct feel, not necessarily identical floating-point math. |
| **3 — Approximated** | Lighting/shading (GX TEV → PBR approximation), texture filtering, post-processing. Visually similar, not identical. |
| **4 — Stubbed** | Features not yet fully reverse-engineered (certain NPC AI, some boss phases). Stubs log warnings. |

---

## 4. FILE FORMAT SUPPORT

All formats are big-endian (PowerPC byte order). The asset extractor converts to little-endian native format on first run and caches to IndexedDB / OPFS.

### 4.1 Archive Formats

| Format | Extension | Description | Status |
|---|---|---|---|
| Yaz0 | `.szs` | LZ-family compression wrapper. Nearly all game files are Yaz0-compressed. | ✅ Implemented |
| RARC | `.arc` inside `.szs` | Nintendo Revolution Archive. Hierarchical file container. | ✅ Implemented |
| U8 | `.arc` | Older GameCube archive format (some Galaxy 1 files). | ✅ Implemented |

### 4.2 Model / Geometry

| Format | Extension | Description | Status |
|---|---|---|---|
| BMD | `.bmd` | J3D Binary Model. Contains geometry, material data (TEV stages), vertex attributes. | ✅ Core implemented |
| BDL | `.bdl` | BMD + precomputed display list data. | ✅ Implemented |
| KCL | `.kcl` | Collision mesh. Triangle-soup with surface type attributes (ice, sand, water, etc.). | ✅ Implemented |
| PA | `.pa` | Path data (splines for cameras and moving objects). | 🔶 Partial |

### 4.3 Animation

| Format | Extension | Description | Status |
|---|---|---|---|
| BCK | `.bck` | J3D Bone (joint) animation. Used for skeletal animation. | ✅ Implemented |
| BCA | `.bca` | J3D Character animation (alternate bone format). | ✅ Implemented |
| BTP | `.btp` | J3D Texture pattern animation (frame-by-frame texture swap). | ✅ Implemented |
| BTK | `.btk` | J3D Texture SRT animation (UV scrolling, scaling). | ✅ Implemented |
| BRK | `.brk` | J3D Color register animation (material color over time). | 🔶 Partial |
| BPK | `.bpk` | J3D Palette animation. | ⬜ Stubbed |
| BVA | `.bva` | J3D Mesh visibility animation. | 🔶 Partial |

### 4.4 Texture

| Format | Extension | Description | Status |
|---|---|---|---|
| BTI | `.bti` | Binary Texture Image. Embedded in BMD or standalone. Supports: CMPR (DXT1 variant), RGB5A3, RGBA8, I4, I8, IA4, IA8, CI4, CI8, RGB565. | ✅ All formats |

### 4.5 Data Tables

| Format | Extension | Description | Status |
|---|---|---|---|
| BCSV | `.bcsv` | Binary CSV. JMap data tables. Fixed-width rows, FNV-1a hashed column names. Used for **everything** — object placement, scenario config, galaxy lists, camera zones, sound cues. | ✅ Implemented |
| MSBT | `.msbt` | Message Studio Binary Text. Game dialogue/UI text. | ✅ Implemented |
| MSBF | `.msbf` | Message Studio Binary Flow. Text branching logic. | 🔶 Partial |

### 4.6 Stage / Scenario

| File | Path | Description |
|---|---|---|
| `*Map.arc` | `StageData/[Galaxy]/` | Main stage geometry, object placement JMap tables |
| `*Scenario.arc` | `StageData/[Galaxy]/` | Per-scenario (star mission) configuration |
| `*Zone.arc` | `StageData/[Galaxy]/` | Sub-zone data (areas within a galaxy) |
| `GalaxyDataTable.bcsv` | `SystemData/` | Master galaxy list with world assignments |
| `ObjInfo.bcsv` | `ObjectData/SystemDataTable.arc` | Actor class → model mapping |
| `PlanetMapDataTable.bcsv` | `ObjectData/` | Planet LOD model assignments |

### 4.7 Audio

| Format | Extension | Description | Status |
|---|---|---|---|
| BRSTM | `.brstm` | Binary Revolution Stream. Looping BGM. Stereo/multi-channel PCM. | ✅ Implemented |
| BRSAR | `.brsar` | Binary Revolution Sound ARchive. SFX container. | 🔶 Partial |
| BRWAV | `.brwav` | Binary Revolution WAV. Individual sound effect. | ✅ Implemented |
| DSP ADPCM | embedded in BRSAR | GC DSP 4-bit ADPCM codec. Decodes to 16-bit PCM. | ✅ Implemented |

---

## 5. CLASS HIERARCHY (from Petari)

These class names are taken directly from Petari's decompilation and the SMG1 Korean symbol map. Luma Engine reimplements each.

```
NameObj                          — Base object. Has name, flags, executor index.
│
├── LiveActor                    — Base for all in-world objects (actors).
│   │   init(), initAfterPlacement()
│   │   movement(), draw(), calcAnim(), calcViewAndEntry()
│   │   mActorFlags, mTranslation, mRotation, mScale
│   │   mActorAnimKeeper, mActorShadow
│   │
│   ├── MapObjActor              — Static world geometry/prop actors.
│   │   ├── MapObjActorInitInfo
│   │   └── [many map objects: PlanetActor, IceStepActor, etc.]
│   │
│   ├── MarioActor               — The player. Full moveset reimplementation.
│   │   │  mVelocity, mGravityVector, mGroundNormal
│   │   │  mAnimName, mHatVisible, mFLUDDEquipped (unused in Galaxy)
│   │   │  updateMove(), tryJump(), trySpinAttack()
│   │   │  adjustGravity()       ← queries GravityCreator every frame
│   │   └── [sub-states: NormalMario, InvincibleMario, etc.]
│   │
│   ├── EnemyActor               — Base enemy class.
│   │   ├── Kuribo (Goomba)
│   │   ├── Koopa
│   │   ├── BeeActor
│   │   └── [~80 enemy types]
│   │
│   ├── CameraActor              — In-world camera triggers.
│   └── AreaObj                  — Invisible area volumes (gravity zones, water, etc.)
│       ├── GravityArea
│       ├── WaterArea
│       ├── SoundArea
│       └── DeathArea
│
├── NameObjHolder<T>             — Typed collection of NameObjs.
│   ├── LiveActorGroup           — Groups of LiveActors (star bit groups, etc.)
│   └── MarioHolder              — Holds the Mario instance.
│
└── [JSystem objects]
    ├── JMapInfo                 — Parsed BCSV table. Provides row/column iteration.
    ├── JMapInfoIter             — Iterator over a JMapInfo table.
    └── JMapLinkArray            — Array of JMap object links.
```

### GravityCreator Subsystem

This is one of Galaxy's most important and distinctive subsystems. Every frame, GravityCreator samples all registered gravity fields and returns the dominant gravity vector for a given world position. Luma Engine implements this exactly.

```
GravityCreator (singleton, manages all gravity fields)
│
├── PlanetGravityCreator        — Sphere gravity (planet core attraction)
│   └── Params: mGravityPoint, mGravityPower, mRange, mPriority
│
├── ParallelGravityCreator      — Box/slab uniform-direction gravity
│   └── Params: mBaseMatrix (OBB), mGravityPower, mRange
│
├── PointGravityCreator         — Point attractor (black holes, warp stars)
│   └── Params: mGravityPoint, mGravityPower, mRange, mDistanceCalc
│
├── DiskGravityCreator          — Disk-shaped planet surface gravity
│   └── Params: mGravityPoint, mNormal, mRadius, mGravityPower
│
├── CylinderGravityCreator      — Cylindrical world gravity
│   └── Params: mGravityPoint, mAxis, mRadius, mHeight, mGravityPower
│
├── SegmentGravityCreator       — Gravity along a line segment
└── TremorGravityCreator        — Tremor/shockwave gravity effect
```

### ScenarioHolder / Stage Loading

```
ScenarioHolder
│   Reads [Galaxy]Scenario.arc → [Galaxy]Scenario/ScenarioData.bcsv
│   Each row = one star mission (Scenario 1, 2, 3...)
│   Columns: ScenarioName, PowerStarId, AppearPowerStarObj,
│             CometTypeName, LuigiModeTimer, IsHidden
│
├── StageDataHolder
│   Reads [Galaxy]Map.arc
│   Contains: /Stage/jmp/Placement/ → ObjInfo, PlanetObj, StartInfo, etc.
│             /Stage/jmp/MapParts/  → map-specific object tables
│             /Stage/jmp/Camera/    → camera area data
│
└── ZoneHolder (one per loaded zone)
    Reads [Zone]Zone.arc
    Contains per-zone JMap tables (same structure as StageData)
```

---

## 6. JMAP / BCSV DATA TABLE SYSTEM

BCSV (Binary CSV) is Galaxy's primary data format. Almost all game configuration lives in BCSV files. Understanding it is essential.

### 6.1 Binary Layout

```
BCSV Header (big-endian):
  Offset 0x00  u32   Entry count
  Offset 0x04  u32   Field count
  Offset 0x08  u32   Entry size (bytes per row, 4-byte aligned)
  Offset 0x0C  u32   Data offset (where row data starts)

Field Descriptor (12 bytes each, immediately after header):
  Offset 0x00  u32   Field name hash (FNV-1a of ASCII field name)
  Offset 0x04  u32   Bitmask (for packed integer fields)
  Offset 0x06  u16   Offset within each row (in bytes)
  Offset 0x08  u8    Shift amount (for bitmask extraction)
  Offset 0x09  u8    Data type:
                       0x00 = s32 (long)
                       0x02 = string (offset into string table)
                       0x04 = f32 (float)
                       0x05 = s32 (long, alternate)
                       0x06 = s16 (short)

Row data: Entry count × Entry size bytes
String table: null-terminated strings referenced by offset
```

### 6.2 FNV-1a Hash (field name lookup)

```python
# Python reference implementation
FNV_PRIME  = 0x01000193
FNV_OFFSET = 0x811C9DC5

def bcsv_hash(name: str) -> int:
    h = FNV_OFFSET
    for c in name:
        h ^= ord(c)
        h = (h * FNV_PRIME) & 0xFFFFFFFF
    return h
```

### 6.3 Key BCSV Tables (Placement)

Located in `[Galaxy]Map.arc/Stage/jmp/Placement/`:

| File | Key columns | Purpose |
|---|---|---|
| `ObjInfo` | `name`, `objdbname`, `pos_x/y/z`, `dir_x/y/z`, `scale_x/y/z`, `l_id`, `Obj_arg0..7` | Main object spawns |
| `PlanetObj` | `name`, `pos_x/y/z`, `dir_x/y/z`, `scale_x/y/z`, `LowFlag`, `MiddleFlag` | Planet model actors |
| `StartInfo` | `MarioNo`, `pos_x/y/z`, `dir_x/y/z`, `CameraId` | Mario spawn points |
| `DemoObjInfo` | `name`, `DemoName`, `DemoSheetName` | Cutscene actors |
| `AreaObjInfo` | `name`, `pos_x/y/z`, `scale_x/y/z`, `Priority`, `SW_APPEAR` | Area volume actors |
| `CameraInfo` | `id`, `pos_x/y/z`, `look_x/y/z`, `fovy`, `CamType` | Camera zone configs |

Located in `[Galaxy]Map.arc/Stage/jmp/Camera/`:

| File | Purpose |
|---|---|
| `CameraArea` | Camera trigger zones (AABB + sphere volumes) |
| `GroupCameraInfo` | Groups of camera parameters |

---

## 7. GRAVITY SYSTEM — EXACT SPECIFICATION

This is implemented to match Petari's documented `GravityCreator::calcGravity()` behavior.

### 7.1 Sampling Algorithm

```
Each frame, for each object that uses gravity (Mario, enemies, etc.):

1. Query GravityCreator with: (worldPosition, layer, gravityFlag)
2. GravityCreator iterates all registered GravityAreaObjs
3. For each: check if worldPosition is within the field's range AABB/sphere
4. Of all fields in range: select the one with highest mPriority
5. If multiple fields share the same priority: additively combine their vectors
6. Normalize the result to unit length, multiply by 1.0 (strength is per-field)
7. Mario's "down" vector = returned gravity direction
8. Mario's transform is realigned to this new "down" each frame
```

### 7.2 Gravity Type Parameters

```
PlanetGravity:
  Range check:  sphere, radius = mRange
  Direction:    normalize(mGravityPoint - objectPosition)
  Power:        mGravityPower (default: 1.0)
  Used by:      Small Round Planet, Medium Round Planet, Large Round Planet

ParallelGravity:
  Range check:  OBB defined by mBaseMatrix and mRange
  Direction:    mGravityDir (constant, not position-dependent)
  Power:        mGravityPower
  Used by:      Flat worlds, space station floors, conveyor areas

DiskGravity:
  Range check:  cylinder: radius = mRadius, height = mRange above/below disk
  Direction:    -mNormal (toward disk surface from above) or +mNormal (from below)
  Power:        mGravityPower, with falloff near edge: power *= (1 - dist/mRadius)
  Used by:      Disc-shaped planets (Gusty Garden's disc sections, etc.)

CylinderGravity:
  Range check:  cylinder volume
  Direction:    normalize(closestPointOnAxis - objectPosition)  [inward radial]
  Power:        mGravityPower
  Used by:      Cylindrical rolling worlds
```

---

## 8. MARIO ACTOR — MOVEMENT CONSTANTS

From Petari and community documentation. These values are stored in `MarioConst.arc`.

| Constant | Value | Notes |
|---|---|---|
| `mRunSpeed` | 22.0 | Max ground run speed (units/frame) |
| `mWalkSpeed` | 8.0 | Walk speed |
| `mJumpPower` | 40.0 | Jump initial Y velocity |
| `mDoubleJumpPower` | 52.0 | Double jump Y velocity |
| `mTripleJumpPower` | 65.0 | Triple jump Y velocity |
| `mLongJumpPower` | 30.0 | Long jump Y velocity |
| `mBackflipPower` | 55.0 | Backflip Y velocity |
| `mGravityPower` | 3.0 | Gravity acceleration (units/frame²) |
| `mGroundFriction` | 0.85 | Ground velocity multiplier per frame |
| `mAirFriction` | 0.97 | Air velocity multiplier per frame |
| `mSpinAttackRadius` | 120.0 | Spin attack hitbox radius |
| `mWaterSinkSpeed` | 5.0 | Speed Mario sinks in water |
| `mWaterBuoyancy` | 1.8 | Upward force in water |

---

## 9. RENDERER SPEC (GX → WebGPU/WebGL2)

### 9.1 GX TEV Pipeline Translation

Galaxy uses Nintendo's GX fixed-function pipeline with up to 8 TEV (Texture Environment Unit) stages per material. Each stage is a configurable combine operation. Luma Engine translates the full TEV graph to a WGSL / GLSL 3.00 ES fragment shader at material load time.

TEV operation (per stage):
```
result.rgb = (d OP ((1-c)*a + c*b)) * scale + bias
result.a   = (d_a OP ((1-c_a)*a_a + c_a*b_a)) * scale_a + bias_a
```

Where a/b/c/d are selectable from: `CPREV, APREV, C0..C2, TEXC, TEXA, RASC, RASA, ONE, HALF, KONST, ZERO`.

This is implemented in `luma-engine/src/renderer/GXMaterialTranslator`.

### 9.2 BTI Texture Format Decoders

All GC texture formats are decoded on the CPU (WASM) at load time, then uploaded as RGBA8 to the GPU.

| GX Format | ID | Description | Implementation |
|---|---|---|---|
| I4 | 0x00 | 4-bit intensity | Trivial |
| I8 | 0x01 | 8-bit intensity | Trivial |
| IA4 | 0x02 | 4-bit intensity + 4-bit alpha | Trivial |
| IA8 | 0x03 | 8-bit intensity + 8-bit alpha | Trivial |
| RGB565 | 0x04 | 16-bit RGB | Unpack 5-6-5 |
| RGB5A3 | 0x05 | 16-bit, either RGB5 or A3RGB4 | MSB-select |
| RGBA8 | 0x06 | 32-bit RGBA, tiled 4×4 | De-tile AR+GB sub-blocks |
| CI4 | 0x08 | 4-bit palette index | Palette lookup |
| CI8 | 0x09 | 8-bit palette index | Palette lookup |
| CMPR | 0x0E | S3TC DXT1 variant, 8×8 super-tiles of 4×4 DXT1 | DXT1 decode |

### 9.3 Lighting Model

Galaxy uses a per-vertex lighting model on GX (GXSetChanCtrl). Luma Engine approximates this as:
- Directional light (sun): matches GX `GX_DIRLGT` behavior
- Point lights: Galaxy uses up to 8 GX hardware lights per pass
- Ambient: constant per-stage, read from material's `AMB0`/`AMB1` registers
- No real-time shadows in Mode A (matches original game)

---

## 10. AUDIO SYSTEM SPEC

### 10.1 DSP ADPCM Decoder

The GC/Wii's DSP uses a proprietary 4-bit ADPCM codec. All game SFX and some music samples use it.

```
DSP ADPCM frame (8 bytes = 14 samples):
  Byte 0:  header (scale [3:0], predictor index [7:4])
  Bytes 1-7: 14 nibbles of 4-bit delta values

Decode formula per sample:
  delta = sign-extend(nibble) << scale
  sample = delta + coef[predictor*2]*hist1 + coef[predictor*2+1]*hist2
  sample = clamp(sample >> 11, -32768, 32767)
  hist2 = hist1; hist1 = sample
```

Luma Engine decodes to 16-bit PCM in WASM, then feeds to WebAudio API via AudioWorklet.

### 10.2 BRSTM Streaming

BGM tracks are streamed from BRSTM files (Binary Revolution Stream):
- Header defines: sample rate, channel count, loop start/end points, ADPCM coefficient tables per channel
- Blocks: fixed-size encoded blocks (default 0x2000 bytes per block per channel)
- Loop: implemented as seamless loop by pre-decoding the loop boundary blocks

---

## 11. ASSET EXTRACTOR SPEC

The asset extractor runs once when the user provides their ISO/WBFS. It:

1. Parses the ISO filesystem table (see §11.1)
2. Decompresses all SZS (Yaz0) files
3. Extracts all RARC archives
4. Decodes all BTI textures to RGBA8 PNG
5. Converts all BMD/BDL models to a Luma Engine internal format (`.lbm`)
6. Converts all BCSV tables to JSON for fast JS-side access
7. Decodes all BRSTM/BRSAR audio to Ogg Vorbis or raw PCM
8. Writes everything to Origin Private File System (OPFS) / IndexedDB
9. Generates a manifest (`luma_manifest.json`) cataloguing all extracted assets

The extractor is a separate WASM module (`luma-extractor.wasm`) that runs in a Web Worker to avoid blocking the main thread.

### 11.1 ISO/WBFS Layout

```
Wii ISO (plain):
  0x000000  WiiDisk header (0x440 bytes)
  0x000440  Partition table info
  0x0400C0  Partition entries (up to 4 partitions)
  ...
  Data partition:
    Partition header (0x1C bytes)
    TMD (Title Metadata)
    Cert chain
    H3 hash table
    Data (encrypted 2MB blocks, Wii discs only)

GameCube ISO (for reference, used in some Wii games as fallback):
  0x000000  Disc header (0x440 bytes)
  0x000440  Apploader (boot code)
  0x002440  DOL (main executable)
  FST offset stored at header+0x0424
  FST:
    Root entry (type=dir, first_child, next, name_off=0)
    File entries: [type(1), name_off(3), data_off(4), size(4)]
    Dir entries:  [type(1), name_off(3), parent(4), next(4)]
    String table: null-terminated names
```

---

## 12. LUMA ENGINE — MODULE LIST

These are the implementation modules, mapped to source files:

| Module | File(s) | Description |
|---|---|---|
| **ISO Extractor** | `tools/asset-extractor/` | Parses Wii ISO, extracts raw filesystem |
| **Yaz0/SZS** | `src/io/Yaz0.h/.cpp` | Decompressor for Nintendo SZS files |
| **RARC** | `src/io/RARC.h/.cpp` | RARC archive reader (Wii variant) |
| **U8** | `src/io/U8.h/.cpp` | U8 archive reader (older format) |
| **BCSV/JMapInfo** | `src/jmap/JMapInfo.h/.cpp` | BCSV parser + JMapInfoIter |
| **BTI** | `src/io/BTI.h/.cpp` | All 10 texture format decoders → RGBA8 |
| **CMPR** | `src/io/CMPR.h/.cpp` | DXT1-variant decoder for CMPR |
| **BMD** | `src/io/BMD.h/.cpp` | J3D model format parser |
| **BCK/BCA** | `src/io/BCK.h/.cpp` | Bone animation parsers |
| **BTP/BTK** | `src/io/BTP.h/.cpp` | Texture animation parsers |
| **BRSTM** | `src/audio/BRSTM.h/.cpp` | BGM stream reader + DSP ADPCM decode |
| **BRSAR** | `src/audio/BRSAR.h/.cpp` | SFX archive reader |
| **NameObj** | `src/actor/NameObj.h/.cpp` | Base object class (from Petari) |
| **LiveActor** | `src/actor/LiveActor.h/.cpp` | Base actor (from Petari) |
| **MarioActor** | `src/actor/MarioActor.h/.cpp` | Player reimplementation |
| **GravityCreator** | `src/gravity/GravityCreator.h/.cpp` | All gravity field types |
| **ScenarioHolder** | `src/scenario/ScenarioHolder.h/.cpp` | Scenario/star loading |
| **StageDataHolder** | `src/scenario/StageDataHolder.h/.cpp` | Stage JMap loading |
| **GXMaterialTranslator** | `src/renderer/GXMaterialTranslator.h/.cpp` | TEV → WGSL/GLSL |
| **BMDRenderer** | `src/renderer/BMDRenderer.h/.cpp` | BMD → WebGPU draw calls |
| **AudioManager** | `src/audio/AudioManager.h/.cpp` | WebAudio bridge + DSP decode |
| **CollisionDetector** | `src/physics/CollisionDetector.h/.cpp` | KCL triangle mesh collision |

---

## 13. RELATIONSHIP TO PETARI

Luma Engine uses Petari as its primary reference document. Specifically:

- **Class names and hierarchies** are taken verbatim from Petari's `include/` headers
- **Member variable offsets** and data types are sourced from Petari's decompiled code
- **MarioConst values** (jump heights, run speeds, etc.) are from Petari's documented constants
- **GravityCreator behavior** is reverse-engineered from Petari's `GravityCreator.cpp` decomp
- **No compiled Petari code is used** — Luma Engine is written independently in C++17/TypeScript

Petari is hosted at: `https://github.com/SMGCommunity/Petari`
Galaxy 2 cross-reference: `https://github.com/SMGCommunity/Garigari`

---

## 14. KNOWN GAPS (v0.1)

These features are planned but not yet implemented:

- Full boss AI (Bowser, Kamella, Bouldergeist, etc.)
- `MBus`/`MSBF` cutscene dialogue flow
- Multi-player (Luigi co-star, pointer controls)
- Comet Observatory hub full logic
- Grand Stars / Power Star unlock sequencing
- Trial galaxies
- All 80+ enemy actor types (10 implemented in v0.1)

---

*Luma Engine is part of Project Starshine.*
*Companion specs: Delfino Engine (SMS) · Constellation Engine (original games)*
