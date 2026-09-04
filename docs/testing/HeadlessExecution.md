# Testing — Headless Execution

> Design record. Implementation deferred. Headless mode is a **reusable core feature**, not
> test-only — it is deliberately **unguarded** so a future server build can use it. Only the test
> *orchestration* that drives it is guarded. See [README.md](README.md#two-foundational-design-principles).

Runs the engine with full physics/animation simulation but no visible output, driven entirely from
the command line. **Windows is the primary target**; other platforms are documented at the end.

Related: [architecture/FrameLoop.md](../architecture/FrameLoop.md) ·
[reference/Configuration.md](../reference/Configuration.md) ·
[architecture/LayerSystem.md](../architecture/LayerSystem.md)

---

## Feasibility Assessment — what we *think* we can and can't do

The engine's render stack is tightly coupled to SDL and RmlUi:

- [`SDLLayer::Create()`](../../source/UGECore/Private/Layers/SDLLayer.cpp) hard-requires
  `SDL_Init(SDL_INIT_VIDEO)`, `SDL_CreateWindow`, and `SDL_CreateRenderer`. There is currently no
  path that produces a null window/renderer.
- [`RmlUILayer::Create()`](../../source/UGECore/Private/Layers/RmlUILayer.cpp) fetches the
  `SDL_Window*` and `SDL_Renderer*` from `SDLLayer` and **fails** if either is null. Its
  `RenderInterface_SDL` and `SystemInterface_SDL` bind directly to those handles, and the ctor
  calls `SDL_GetWindowSize`.

**Conclusion:** we cannot simply null out video and keep RmlUi. Two viable strategies:

### Strategy A — Offscreen render (PRIMARY, keeps UI testable)

Keep the full stack alive but invisible:

- Create the window **hidden**: add `SDL_WINDOW_HIDDEN` to the `SDL_CreateWindow` flags when
  `LaunchSettings::Headless` is set.
- Keep the SDL renderer (software renderer is fine and display-independent). RmlUi initializes and
  runs normally against it — **UI stays under test**.
- Suppress presentation: `SDLLayer::EndFrame()` skips `SDL_RenderPresent` in headless mode (the
  GPU/compositor never sees a frame; all `Update()`/`Draw()` logic still executes).
- Optionally set the SDL `dummy` video driver (`SDL_HINT_VIDEO_DRIVER = "dummy"`) for fully
  display-server-independent runs — chiefly relevant to Linux CI.

**We believe A works on Windows today** with only the hidden-window + skip-present changes. This is
the recommended default because it preserves the entire pipeline, including RmlUi, for testing.

### Strategy B — Skip the UI layer entirely (FALLBACK, excludes UI)

If RmlUi proves unhappy headless (or a run genuinely doesn't need UI), exclude it at bootstrap:

- A new **layer skip-list** honored by the bootstrap loop in
  [`UGEApplication::Create()`](../../source/UGECore/Private/UGEApplication.cpp). Both `sdl` and
  `rmlui` are `REGISTER_LAYER`-driven and pushed via
  [`LayerRegistry::Names()`](../../source/UGECore/Private/LayerRegistry.cpp), so skipping a name is
  a clean, localized change.
- Skipping `rmlui` yields a guaranteed no-UI run; the trade-off is that **UI is excluded from
  testing** for that run. (`sdl` cannot currently be skipped while any layer depends on its
  renderer — physics/scene draw and RmlUi all need it — so the fallback skips `rmlui`, not `sdl`.)

**What we can't (yet) do:** fully remove SDL video and still run scene/physics **rendering**.
Simulation-only (physics + animation state, no `Draw`) without any SDL video would require
decoupling `Draw` from the renderer — noted as future work, not needed for the first cut because
Strategy A already gives a no-output run.

---

## Loop Changes — fixed timestep (core, unguarded)

The current [`UGEApplication::Run()`](../../source/UGECore/Private/UGEApplication.cpp) is a
30 FPS-capped real-time loop. Under headless it should run a **fixed timestep with no real-time
delay** so simulation advances as fast as possible and every tick's `dt` is identical and
reproducible:

- Headed (default): existing real-time paced loop.
- Headless: `deltaTime` is a constant (e.g. `1/30 s`), `SDL_DelayNS` is skipped, and the loop
  advances tick-by-tick. This is a **core** behavior (a server would want it too), selected by
  `LaunchSettings::Headless`, not by a test `#ifdef`.

The recorded `dt` in a replay (see [InputRecordPlayback.md](InputRecordPlayback.md)) matches this
fixed value, keeping input, simulation, and snapshots on one timebase.

---

## Command Line → LaunchSettings

CLI flags are the entry point and flow into `LaunchSettings` (parsed in
[`main.cpp`](../../source/Application/main.cpp), which already receives `argc/argv`). Precedence:
**CLI overrides `UGESettings.toml` overrides built-in defaults.**

### New `LaunchSettings` fields (core, unguarded)

```cpp
struct LaunchSettings {
    // ...existing fields...
    bool                     Headless   = false;   // offscreen render (Strategy A)
    std::vector<std::string> SkipLayers;           // bootstrap skip-list (Strategy B)
    // Test seeding (plain data; no test-code dependency) — written into DataStore as test.*
    bool                     TestActive   = false;  // → test.active
    std::string              TestMode;              // → test.mode  ("record"|"replay")
    std::string              TestManifest;          // → test.manifest (path)
};
```

`Headless` and `SkipLayers` are general engine settings. The `Test*` fields are inert plain data
until the (guarded) harness reads the `test.*` DataStore keys they seed.

### Flags

| Flag | Effect |
|---|---|
| `--headless` | `LaunchSettings::Headless = true` |
| `--skip-layer <name>` (repeatable) | append to `SkipLayers` (e.g. `--skip-layer rmlui`) |
| `--test-mode <record\|replay>` | `TestMode`; implies `TestActive = true` |
| `--test-manifest <path>` | `TestManifest` |
| `--test` | `TestActive = true` (mode read from manifest) |

Argument parsing lives in the Application target (`main.cpp`/`ConfigLoader`); it is small and
unguarded (parsing generic settings), and only *populates* the `Test*` plain-data fields.

### Seeding `test.*` into the DataStore

After `UGEDataLayer` is created and before `SDLLayer`, the seed values are written:

```
test.active   = LaunchSettings::TestActive ? 1 : 0
test.mode     = LaunchSettings::TestMode
test.manifest = LaunchSettings::TestManifest
```

This ordering lets core layers consult `test.active` during their own `Create()` if ever needed,
and lets the `TestHarnessLayer` read fully-seeded values at its (late) load order.

---

## `UGESettings.toml` additions

Mirror the CLI as optional keys so runs can be configured without flags (CLI still overrides):

```toml
Headless   = false
SkipLayers = []            # e.g. ["rmlui"]

[Test]
Active   = false
Mode     = "replay"
Manifest = "tests/platformer_jump/manifest.toml"
```

Parsed by [`ConfigLoader.cpp`](../../source/Application/ConfigLoader.cpp) alongside the existing
`readOpt` / array handling.

---

## Platform Notes

| Platform | Status | Notes |
|---|---|---|
| **Windows** | **Primary / supported** | `SDL_WINDOW_HIDDEN` + skip-present works without a display server. Strategy A is the default. |
| Linux | Documented, future | Use `dummy` video driver or run under `xvfb`. Software renderer keeps RmlUi alive. |
| macOS | Documented, future | Hidden window + software renderer expected to work; unverified. |

The first implementation targets Windows only; the platform-independence of logical coordinates and
the software renderer is designed in so later platforms need configuration, not redesign.

