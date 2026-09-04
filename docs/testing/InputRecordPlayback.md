# Testing — Input Recording & Playback

> Design record. Implementation deferred. See [Architecture.md](Architecture.md) for the hook
> points referenced here.

Captures SDL input over time into a replayable TOML recording, and deterministically re-injects it
during a replay run. Because the engine is deterministic, replaying the same input from the same
initial state reproduces the same state trajectory — the premise the whole test system rests on.

Related: [systems/InputEvents.md](../systems/InputEvents.md) ·
[architecture/FrameLoop.md](../architecture/FrameLoop.md)

---

## Timebase — the tick index

Input is anchored to the **tick index**, not wall-clock time. Under headless replay the loop runs
a **fixed timestep** (see [HeadlessExecution.md](HeadlessExecution.md)), so "tick N" is an exact,
reproducible moment. Each recorded event stores:

- `tick` — integer tick index the event was observed on (0-based from run start).
- `dt` — the delta-time value for that tick (seconds). Recorded so a replay can reproduce the
  exact `deltaTime` fed to `Update()` even if it later runs at a different real cadence, and so
  state snapshots can share the same timebase (see [StateInstrumentation.md](StateInstrumentation.md)).

The first captured tick establishes tick 0; the recorder stamps every event with the current tick
counter maintained by the harness.

---

## Recording (record mode)

Armed when `test.mode = "record"` and `test.active = 1`.

- **Capture point:** `TestHarnessLayer::OnEventCaptured(SDL_Event&)`, called from the single
  guarded line in [`UGEApplication::dispatchEvent()`](../../source/UGECore/Private/UGEApplication.cpp).
  This sees **every** SDL event before layer dispatch.
- **Filtering:** by default record input-class events only — `SDL_EVENT_KEY_DOWN/UP`,
  `SDL_EVENT_MOUSE_MOTION`, `SDL_EVENT_MOUSE_BUTTON_DOWN/UP`, `SDL_EVENT_MOUSE_WHEEL`,
  `SDL_EVENT_TEXT_INPUT`, and `SDL_EVENT_QUIT`. Window/system events are excluded by default
  (a whitelist, configurable in the manifest).
- **Serialization:** each kept event is flattened into a TOML table with its type-relevant fields.
- **End of capture:** on `SDL_EVENT_QUIT` or an explicit stop, the recorder flushes the TOML file
  and writes the run manifest (below).

### Recording TOML schema

```toml
# recording.toml
[meta]
format_version = 1
engine_build   = "UGE dev 2026-08-30"
created        = "2026-08-30T12:00:00Z"
seed_manifest  = "tests/platformer_jump/manifest.toml"   # links to the run manifest
total_ticks    = 540

# One [[event]] per captured SDL event, in capture order.
[[event]]
tick = 12
dt   = 0.033333
type = "key_down"     # normalized name, not the raw SDL enum
key  = "Space"        # SDL_GetKeyName / scancode name
repeat = false

[[event]]
tick = 12
dt   = 0.033333
type = "mouse_motion"
x    = 640.0          # logical (reference-resolution) coordinates
y    = 360.0
xrel = 4.0
yrel = -2.0

[[event]]
tick = 45
dt   = 0.033333
type = "mouse_button_down"
button = "left"
x = 640.0
y = 360.0
```

**Coordinate space:** mouse coordinates are stored in **logical reference-resolution** pixels
(matching `SDL_SetRenderLogicalPresentation` in [`SDLLayer`](../../source/UGECore/Private/Layers/SDLLayer.cpp)),
so a recording replays identically regardless of the actual window/backbuffer size — important for
headless where the offscreen surface may differ.

---

## Playback (replay mode)

Armed when `test.mode = "replay"` and `test.active = 1`.

- **Load:** on `Create()` the harness parses the recording referenced by `test.manifest` into an
  ordered, tick-indexed queue.
- **Injection point:** `TestHarnessLayer::PreTick(dt)` — called from the guarded line at the top
  of the tick in [`UGEApplication::Run()`](../../source/UGECore/Private/UGEApplication.cpp),
  **before** the `Update()` pass. All events stamped with the current tick are reconstructed into
  `SDL_Event` structs and injected.
- **Injection mechanism:** `SDL_PushEvent` (preferred). Pushed events are drained by
  `SDLLayer::Update()`'s existing `SDL_PollEvent` loop and flow through the real
  `dispatchEvent` path — maximum fidelity, no bypass. `OnEventCaptured` recognizes synthesized
  events (or is simply inert in replay mode) so replay does not re-record itself.
- **`dt` override:** in replay, `PreTick` supplies the recorded `dt` for the tick so `Update()`
  integrates with the exact delta captured, decoupling correctness from real-time pacing.
- **Termination:** when the queue is exhausted (or the recorded `SDL_EVENT_QUIT` tick is reached),
  the harness stops the run and hands off to the reporter (see
  [TestRunnerAndReporting.md](TestRunnerAndReporting.md)).

### Injection fidelity notes

- Reconstruct `SDL_Event.key.timestamp`, `windowID`, etc. with sensible defaults; downstream code
  (RmlUi, scenes) keys off type/key/button/coords, which are faithfully restored.
- Mouse coordinates are mapped from logical back to render coordinates if any consumer needs raw
  window pixels; RmlUi's SDL backend already works in window space, so the harness applies the
  inverse of the logical-presentation transform when required.

---

## Run Manifest (ties input + baseline together)

A test is more than an input file. The **run manifest** is the top-level TOML a test run loads; it
references the recording and the state baseline and pins the initial conditions.

```toml
# tests/platformer_jump/manifest.toml
[test]
name        = "platformer_jump"
description = "Jump arc reproduces within epsilon"

[run]
init_script = "assets/lua/tests/platformer_jump_setup.lua"  # deterministic scene setup
headless    = true
skip_layers = []                 # e.g. ["rmlui"] to exclude UI (see HeadlessExecution.md)
max_ticks   = 600                # safety cap

[input]
recording = "tests/platformer_jump/recording.toml"

[baseline]
snapshot  = "tests/platformer_jump/baseline.toml"
epsilon   = 0.0001               # global epsilon (see StateInstrumentation.md)
```

- **Record workflow:** run with `--test-mode record --test-manifest <manifest>`; the harness
  writes `recording.toml` and (on request) the initial `baseline.toml`.
- **Replay workflow:** run with `--test-mode replay --test-manifest <manifest>`; the harness
  injects input and compares against the baseline.

Command-line → `LaunchSettings` mapping is defined in [HeadlessExecution.md](HeadlessExecution.md).

---

## Determinism Requirements (must hold for replay to be valid)

1. **Deterministic setup** — the scene must be built identically each run (fixed `init_script`, no
   wall-clock/random seeding unless seeded from the manifest).
2. **Fixed timestep** — replay uses the recorded `dt` per tick; no real-time catch-up bursts.
3. **No unrecorded input** — during replay, real OS input is ignored (or the run is headless with
   no window focus). Only injected events drive the sim.
4. **Stable iteration order** — already provided by the engine's push-order layer iteration and
   Box2D's deterministic stepping.

Violations surface as baseline mismatches, which is itself a useful signal that determinism has
regressed.

