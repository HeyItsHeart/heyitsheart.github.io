// ─────────────────────────────────────────────
//  STARSHINE ENGINE — Browser Shell (TypeScript)
//
//  This is the JS side of the engine. It:
//    1. Manages the WebGL canvas
//    2. Fetches and mounts game files into MEMFS
//    3. Initializes the WASM module
//    4. Polls input every frame
//    5. Drives the rAF loop → starshine_frame()
//    6. Exposes a clean API for the game UI
// ─────────────────────────────────────────────

// Emscripten module type (simplified)
interface StarshineModule {
  _starshine_init(game: number, w: number, h: number): number;
  _starshine_resize(w: number, h: number): void;
  _starshine_frame(timestamp: number): number;
  _starshine_input(
    stickX: number, stickY: number,
    btnJump: number, btnAction: number, btnCrouch: number,
    jumpPressed: number, actionPressed: number, crouchPressed: number
  ): void;
  _starshine_load_arc(path: number): number;
  _starshine_add_gravity(
    type: number,
    px: number, py: number, pz: number,
    ax: number, ay: number, az: number,
    strength: number, range: number, priority: number
  ): void;
  _starshine_clear_gravity(): void;
  _starshine_set_mario_pos(x: number, y: number, z: number): void;
  _starshine_get_mario_pos(ptr: number): void;
  _starshine_get_mario_state(): number;
  _starshine_get_fludd_water(): number;
  _starshine_dump_scene(): void;
  _starshine_shutdown(): void;

  // Emscripten file system
  FS: {
    mkdir(path: string): void;
    mount(type: unknown, opts: unknown, path: string): void;
    writeFile(path: string, data: Uint8Array): void;
  };
  MEMFS: unknown;

  // Emscripten memory
  HEAPF32: Float32Array;
  _malloc(size: number): number;
  _free(ptr: number): void;
  UTF8ToString(ptr: number): string;
  stringToUTF8(str: string, buf: number, size: number): void;
  lengthBytesUTF8(str: string): number;
}

// ── Game enum matching C++ ─────────────────────
export enum Game { Sunshine = 0, Galaxy1 = 1, Galaxy2 = 2 }

// ── Mario state enum matching C++ ─────────────
export enum MarioState {
  Idle = 0, Walk = 1, Run = 2, Crouch = 3, CrouchIdle = 4,
  Slide = 5, Skid = 6, Jump = 7, DoubleJump = 8, TripleJump = 9,
  Backflip = 10, LongJump = 11, SideFlip = 12, WallKick = 13,
  Fall = 14, Swim = 15, SwimIdle = 16, FLUDD_Hover = 20,
  SpinAttack = 23,
}

// ── Gravity field params ───────────────────────
export interface GravityParams {
  type: 0|1|2|3|4|5;  // Global|Sphere|Cylinder|Disk|Point|InvSphere
  position?: [number,number,number];
  axis?: [number,number,number];
  strength?: number;
  range?: number;       // -1 = infinite
  priority?: number;    // 0–255
}

// ── Engine API ─────────────────────────────────
export class StarshineEngine {
  private mod: StarshineModule | null = null;
  private canvas: HTMLCanvasElement;
  private game: Game;
  private rafId: number = 0;
  private running = false;

  // Input state
  private keys   = new Set<string>();
  private prevKeys = new Set<string>();
  private gamepad: Gamepad | null = null;

  // Callbacks
  public onReady?:  () => void;
  public onError?:  (msg: string) => void;
  public onFrame?:  (state: MarioState, fluddWater: number) => void;

  constructor(canvas: HTMLCanvasElement, game: Game) {
    this.canvas = canvas;
    this.game   = game;
  }

  // ── Initialize ────────────────────────────────
  // Loads the WASM module and calls onReady when done
  async init(wasmPath = 'starshine.js'): Promise<void> {
    return new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = wasmPath;
      script.onload = async () => {
        try {
          // @ts-ignore — Emscripten global
          const mod = await StarshineWasm({ canvas: this.canvas });
          this.mod = mod as StarshineModule;

          const w = this.canvas.clientWidth  || 1280;
          const h = this.canvas.clientHeight || 720;
          const ok = this.mod._starshine_init(this.game, w, h);
          if(!ok) throw new Error('starshine_init returned 0');

          this.setupInputListeners();
          this.setupResizeObserver();
          this.onReady?.();
          resolve();
        } catch(e: unknown) {
          const msg = e instanceof Error ? e.message : String(e);
          this.onError?.(msg);
          reject(e);
        }
      };
      script.onerror = () => {
        const err = `Failed to load ${wasmPath}`;
        this.onError?.(err);
        reject(new Error(err));
      };
      document.head.appendChild(script);
    });
  }

  // ── Load a game file ──────────────────────────
  // Fetches the file and mounts it in Emscripten MEMFS
  async loadFile(url: string, mountPath: string): Promise<void> {
    if(!this.mod) throw new Error('Engine not initialized');

    const resp = await fetch(url);
    if(!resp.ok) throw new Error(`HTTP ${resp.status} loading ${url}`);
    const data = new Uint8Array(await resp.arrayBuffer());

    // Ensure parent directories exist in MEMFS
    const parts = mountPath.split('/').filter(Boolean);
    let cur = '';
    for(let i=0; i<parts.length-1; i++) {
      cur += '/' + parts[i];
      try { this.mod.FS.mkdir(cur); } catch {}
    }

    this.mod.FS.writeFile(mountPath, data);
    console.log(`[Starshine] Mounted ${url} → ${mountPath} (${(data.length/1024).toFixed(1)} KB)`);
  }

  // ── Load and parse an ARC archive ─────────────
  async loadArc(url: string, mountPath: string): Promise<number> {
    if(!this.mod) throw new Error('Engine not initialized');
    await this.loadFile(url, mountPath);
    // Pass the path as a C string
    const pathBytes = this.mod.lengthBytesUTF8(mountPath) + 1;
    const pathPtr   = this.mod._malloc(pathBytes);
    this.mod.stringToUTF8(mountPath, pathPtr, pathBytes);
    const count = this.mod._starshine_load_arc(pathPtr);
    this.mod._free(pathPtr);
    return count;
  }

  // ── Add a gravity field (Galaxy) ──────────────
  addGravity(params: GravityParams): void {
    if(!this.mod) return;
    const p = params.position ?? [0,0,0];
    const a = params.axis     ?? [0,1,0];
    this.mod._starshine_add_gravity(
      params.type,
      p[0],p[1],p[2],
      a[0],a[1],a[2],
      params.strength ?? -20,
      params.range    ?? -1,
      params.priority ?? 128
    );
  }

  clearGravity(): void { this.mod?._starshine_clear_gravity(); }

  // ── Mario helpers ─────────────────────────────
  setMarioPosition(x: number, y: number, z: number): void {
    this.mod?._starshine_set_mario_pos(x,y,z);
  }

  getMarioPosition(): [number,number,number] {
    if(!this.mod) return [0,0,0];
    const ptr = this.mod._malloc(12);
    this.mod._starshine_get_mario_pos(ptr);
    const idx = ptr >> 2;
    const v: [number,number,number] = [
      this.mod.HEAPF32[idx],
      this.mod.HEAPF32[idx+1],
      this.mod.HEAPF32[idx+2],
    ];
    this.mod._free(ptr);
    return v;
  }

  getMarioState(): MarioState {
    return (this.mod?._starshine_get_mario_state() ?? 0) as MarioState;
  }

  getFluddWater(): number {
    return this.mod?._starshine_get_fludd_water() ?? 0;
  }

  dumpScene(): void { this.mod?._starshine_dump_scene(); }

  // ── Run loop ──────────────────────────────────
  start(): void {
    if(this.running) return;
    this.running = true;
    const loop = (ts: number) => {
      if(!this.running) return;
      this.pollInput();
      this.mod?._starshine_frame(ts);
      const state = this.getMarioState();
      const water = this.getFluddWater();
      this.onFrame?.(state, water);
      this.prevKeys = new Set(this.keys);
      this.rafId = requestAnimationFrame(loop);
    };
    this.rafId = requestAnimationFrame(loop);
  }

  stop(): void {
    this.running = false;
    cancelAnimationFrame(this.rafId);
  }

  shutdown(): void {
    this.stop();
    this.mod?._starshine_shutdown();
    this.mod = null;
  }

  // ── Input ─────────────────────────────────────
  private setupInputListeners(): void {
    window.addEventListener('keydown', e => { this.keys.add(e.code); e.preventDefault(); });
    window.addEventListener('keyup',   e => { this.keys.delete(e.code); });
    window.addEventListener('gamepadconnected',    () => this.pollGamepad());
    window.addEventListener('gamepaddisconnected', () => { this.gamepad = null; });
  }

  private pollGamepad(): void {
    const pads = navigator.getGamepads();
    this.gamepad = pads[0] ?? null;
  }

  private pollInput(): void {
    if(!this.mod) return;
    this.pollGamepad();

    // Keyboard: WASD / Arrow keys for stick
    let sx = 0, sy = 0;
    if(this.keys.has('KeyA') || this.keys.has('ArrowLeft'))  sx -= 1;
    if(this.keys.has('KeyD') || this.keys.has('ArrowRight')) sx += 1;
    if(this.keys.has('KeyW') || this.keys.has('ArrowUp'))    sy -= 1;
    if(this.keys.has('KeyS') || this.keys.has('ArrowDown'))  sy += 1;

    // Gamepad overrides keyboard stick
    if(this.gamepad) {
      const axes = this.gamepad.axes;
      if(Math.abs(axes[0]) > 0.1) sx = axes[0];
      if(Math.abs(axes[1]) > 0.1) sy = axes[1];
    }

    // Buttons
    const jumpHeld    = this.keys.has('Space') || this.keys.has('KeyX') ||
                        !!(this.gamepad?.buttons[0].pressed);
    const actionHeld  = this.keys.has('KeyB') || this.keys.has('KeyC') ||
                        !!(this.gamepad?.buttons[2].pressed);
    const crouchHeld  = this.keys.has('KeyZ') || this.keys.has('ShiftLeft') ||
                        !!(this.gamepad?.buttons[5].pressed);

    // Just-pressed: not in prevKeys but in keys now
    const justPressed = (code: string) =>
      this.keys.has(code) && !this.prevKeys.has(code);
    const jumpPressed   = justPressed('Space') || justPressed('KeyX');
    const actionPressed = justPressed('KeyB')  || justPressed('KeyC');
    const crouchPressed = justPressed('KeyZ')  || justPressed('ShiftLeft');

    this.mod._starshine_input(
      sx, sy,
      jumpHeld   ? 1 : 0,
      actionHeld ? 1 : 0,
      crouchHeld ? 1 : 0,
      jumpPressed   ? 1 : 0,
      actionPressed ? 1 : 0,
      crouchPressed ? 1 : 0,
    );
  }

  // ── Resize ────────────────────────────────────
  private setupResizeObserver(): void {
    const ro = new ResizeObserver(() => {
      const w = this.canvas.clientWidth;
      const h = this.canvas.clientHeight;
      this.canvas.width  = w;
      this.canvas.height = h;
      this.mod?._starshine_resize(w, h);
    });
    ro.observe(this.canvas);
  }
}

// ── Convenience: default launcher HTML usage ───
// This is what index.html calls:
//
//   import { StarshineEngine, Game } from './starshine_shell.js';
//   const engine = new StarshineEngine(canvas, Game.Sunshine);
//   await engine.init();
//   await engine.loadArc('/assets/bianco.szs', '/bianco.arc');
//   engine.addGravity({ type: 0, strength: -25 }); // global for Sunshine
//   engine.setMarioPosition(0, 200, 0);
//   engine.start();
