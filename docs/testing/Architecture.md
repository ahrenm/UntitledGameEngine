# Testing — Architecture

> Design record. Implementation deferred. See [README.md](README.md) for goals and principles.

This document defines the component layout of the automated testing system: what lives in **core
(unguarded)**, what lives in the **`TestHarness` plugin (guarded)**, the exact **hook points**,
and the **`test.active`** runtime contract.

Related engine docs: [architecture/LayerSystem.md](../architecture/LayerSystem.md) ·
[architecture/FrameLoop.md](../architecture/FrameLoop.md) ·
[systems/InputEvents.md](../systems/InputEvents.md) · [systems/DataStore.md](../systems/DataStore.md)

---

## Component Overview

```
                       ┌──────────────────────────────────────────────┐
                       │  TestHarness plugin  (guarded by             │
                       │  UGE_ENABLE_TESTING)                          │
                       │                                              │
                       │   TestHarnessLayer  (REGISTER_LAYER)         │
                       │     • drives record / replay                 │
                       │     • requests snapshots + compares          │
                       │     • owns the TestRunner + reporter         │
                       └───────────────▲──────────────────────────────┘
                                       │ uses core primitives
   ┌───────────────────────────────────┴───────────────────────────────────┐
   │  CORE (unguarded, reusable)                                            │
   │                                                                        │
   │  UGEApplication::Run()      — 2 guarded call-lines into the harness    │
   │  UGEApplication::dispatchEvent() — 1 guarded call-line (input capture) │
   │  SDLLayer                   — headless/offscreen render mode           │
   │  LayerRegistry / bootstrap  — layer skip-list                          │
   │  UGEDataLayer / DataStore    — snapshot emission + `test.active` flag   │
   │  LaunchSettings              — Headless / SkipLayers / Test* fields     │
   └────────────────────────────────────────────────────────────────────────┘
```

The **only** test-specific code compiled into a test build's *core call sites* is a handful of
one-line, `#ifdef UGE_ENABLE_TESTING`-guarded calls that forward to the harness. Everything
substantial lives in the plugin.

---

## The `TestHarness` Plugin

A new plugin under `source/GamePlugins/TestHarness/`, structured like the existing plugins
(`DemoGame`, `UIWindows`, `TeaPot3D`) and added to
[`source/GamePlugins/CMakeLists.txt`](../../source/GamePlugins/CMakeLists.txt). It is only built
when `UGE_ENABLE_TESTING` is defined, and is loaded like any other plugin via `UGESettings.toml`
`[Modules] Load`.

It owns a single layer:

### `TestHarnessLayer`

- Registers via `REGISTER_LAYER("test-harness", <loadOrder>, TestHarnessLayer)`. Load order is
  chosen so it ticks **last** (after `RmlUILayer`, e.g. `100.0f`) — it observes the fully-settled
  frame each tick and drives replay/snapshot after all other layers have updated.
- On `Create()` it checks the `test.active` DataStore flag:
  - **flag false** → it logs `"TestHarness: inactive (test.active=false); no-op"` and every
    per-frame method early-returns. The layer is present but inert.
  - **flag true** → it loads the run manifest, arms record or replay mode, and begins snapshotting.
- Contains the `TestRunner`, the input recorder/injector, the snapshot writer/comparer, and the
  reporter (all detailed in the sibling documents).

### Graceful degradation matrix

| Build | `test.active` | Behaviour |
|---|---|---|
| No `UGE_ENABLE_TESTING` | — | Plugin not built; core hooks compile to nothing. Zero cost. |
| `UGE_ENABLE_TESTING` | `false` | Layer present, logs once, no-ops all frame work. |
| `UGE_ENABLE_TESTING` | `true` | Full record/replay/compare active. |

---

## The `test.active` Runtime Flag

A single DataStore key, seeded from `LaunchSettings` **before** layers are built, so any core
layer can consult it during its own `Create()`:

- Key: `test.active` (`int`, `1`/`0`) — mirrors the `debug.show_collision` convention.
- Companion keys (all seeded from `LaunchSettings`, all transient):
  - `test.mode` (`string`: `"record"` | `"replay"`)
  - `test.manifest` (`string`: VFS or disk path to the run manifest TOML)
- Seeding point: `LaunchSettings` fields are populated from the command line in
  [`main.cpp`](../../source/Application/main.cpp) / [`ConfigLoader`](../../source/Application/ConfigLoader.cpp),
  then written into the `DataStore` when `UGEDataLayer` is created (or immediately after, before
  `SDLLayer`). Exact seeding site is finalized in [HeadlessExecution.md](HeadlessExecution.md).

Because this is a plain DataStore flag, non-test code can cheaply branch on it (for example a
layer choosing not to spawn a background thread) without linking any test code.

---

## Hook Points (core call sites)

All hooks are single, `#ifdef UGE_ENABLE_TESTING`-guarded lines that forward into the harness via
`ServiceLocator::TryGet<TestHarnessLayer>()`. They do nothing meaningful on a non-test build (they
don't compile) and no-op at runtime when the harness is inactive.

### 1. Frame loop — [`UGEApplication::Run()`](../../source/UGECore/Private/UGEApplication.cpp)

The existing loop already computes `deltaTime`. Two guarded touch points:

```cpp
// inside the tick, after computing deltaTime, before/after the Update pass
#ifdef UGE_ENABLE_TESTING
    if (m_testHarness) m_testHarness->PreTick(deltaTime);   // inject replayed input, advance clock
#endif

    for (auto& Layer : m_layers)
        Layer->Update();

    // ...BeginFrame / Draw / EndFrame...

#ifdef UGE_ENABLE_TESTING
    if (m_testHarness) m_testHarness->PostTick(deltaTime);  // snapshot + compare + advance replay
#endif
```

`m_testHarness` is a cached `TestHarnessLayer*` (null in non-test builds; the member itself is
guarded). See [HeadlessExecution.md](HeadlessExecution.md) for how the loop's frame pacing changes
under headless (fixed-timestep) mode — that change is a **core** feature, not guarded.

### 2. Event pump — [`UGEApplication::dispatchEvent()`](../../source/UGECore/Private/UGEApplication.cpp)

One guarded line to record real input during a record session:

```cpp
void UGEApplication::dispatchEvent(SDL_Event& Event)
{
#ifdef UGE_ENABLE_TESTING
    if (m_testHarness) m_testHarness->OnEventCaptured(Event);   // record mode only
#endif
    for (auto& Layer : m_layers | std::views::reverse)
        // ...existing dispatch...
}
```

Injection during **replay** does not happen here; the harness injects synthesized events at
`PreTick` so they flow through the normal `dispatchEvent` path as if real. See
[InputRecordPlayback.md](InputRecordPlayback.md).

---

## Core Primitives Introduced (unguarded)

These are added to core because they are independently useful (notably to a future server build):

| Primitive | Location | Purpose |
|---|---|---|
| `LaunchSettings::Headless` (bool) | `LaunchSettings.h` | Select offscreen render mode. |
| `LaunchSettings::SkipLayers` (`vector<string>`) | `LaunchSettings.h` | Names to exclude at bootstrap. |
| `LaunchSettings::Test*` fields | `LaunchSettings.h` | Seed `test.active` / `test.mode` / `test.manifest`. |
| Layer skip-list honoring | `UGEApplication::Create()` bootstrap loop | Skip named layers from `LayerRegistry::Names()`. |
| Offscreen window/renderer path | `SDLLayer::Create()` / `EndFrame()` | Hidden window + suppressed present. |
| `DataStore` snapshot walk | `UGEDataLayer` | Serialize changed entries to TOML. |

The **decisions** that use these (which layers to skip for a UI-less run, when to snapshot, what
epsilon to compare with) live in the guarded plugin.

---

## Sequence — one replay tick

```
Run() loop
  │
  ├─ PreTick(dt)          [guarded → harness]
  │     └─ synth SDL_Events for this tick → SDL event queue / direct dispatch
  │
  ├─ Layer::Update() ×N   [normal engine tick; physics/anim step with dt]
  │     └─ dispatchEvent() sees injected events (OnEventCaptured no-ops in replay)
  │
  ├─ BeginFrame/Draw/EndFrame  [EndFrame suppresses present in headless]
  │
  └─ PostTick(dt)         [guarded → harness]
        ├─ capture DataStore delta snapshot (if changed) w/ accumulated duration
        └─ compare against baseline within global epsilon → record pass/fail
```

---

## Open Implementation Questions (tracked, not blocking)

- Exact load-order value for `TestHarnessLayer` (proposed `100.0f`).
- Whether `PreTick` injects via `SDL_PushEvent` (flows through real pump) or calls
  `dispatchEvent` directly (bypasses `SDL_PollEvent`). Leaning `SDL_PushEvent` for fidelity — see
  [InputRecordPlayback.md](InputRecordPlayback.md).
- Whether `m_testHarness` is cached in `UGEApplication` or fetched per-tick via `ServiceLocator`.
  Cached pointer preferred to avoid per-frame lookup.

