# Testing — State Instrumentation

> Design record. Implementation deferred. Snapshot **emission** from the `DataStore` is a
> **reusable core feature** (general introspection/serialization) and is therefore **unguarded**;
> the **comparison and pass/fail decision** are test orchestration and live in the guarded plugin.

State snapshots are the pass/fail baseline. During a **record** run the harness writes a baseline;
during a **replay** run it captures the live state and compares against that baseline within a
**global epsilon**. Because the engine is certified deterministic, an exact replay should match the
baseline to within floating-point tolerance.

Related: [systems/DataStore.md](../systems/DataStore.md) ·
[systems/Physics.md](../systems/Physics.md) · [systems/Animation.md](../systems/Animation.md)

---

## What is captured

The [`DataStore`](../systems/DataStore.md) is the single unified source of truth, so it is the
snapshot surface. Physics and animation already mirror live body/controller state into the store
(e.g. `PhysicsLayer::EnableStateSync` writes `<prefix>.*` position/velocity keys), so snapshotting
the store transitively captures simulation state.

- **Included by default:** all non-dataset entries (transient + persistent) — i.e. live runtime
  state, excluding the static `assets/DATA/*.toml` dataset entries which never change.
- **Configurable scope:** the run manifest may restrict capture to key prefixes (e.g.
  `["player.", "physics."]`) to keep baselines focused and robust.
- **Excluded by default:** dataset-sourced entries (`DataSource::TomlDataset`) and any key matching
  a manifest `ignore` list (for intentionally non-deterministic values, though ideally none exist).

Capture uses the existing `DataStore::ForEach` / `DataMeta` provenance to filter by source.

---

## Timebase & cadence — delta capture with frame collapsing

Snapshots are taken at `TestHarnessLayer::PostTick(dt)` (after every layer has updated). To keep
baselines small **we capture on change and collapse unchanged spans into a duration** rather than
writing a full snapshot every tick.

### Algorithm

1. **Tick 0 — keyframe.** Write a full snapshot of every in-scope key (the baseline's initial
   state). Record the starting `tick` and `dt`.
2. **Each subsequent tick — delta.** Diff the current in-scope entries against the last emitted
   values:
   - **No key changed** → emit nothing; increment a `hold` counter and accumulate elapsed time
     (`+= dt`). The previous record's collapsed span grows.
   - **One or more keys changed** → close the current hold span (record how many ticks / how much
     time it spanned), then emit a **delta record** containing only the changed keys, stamped with
     the current `tick`, `dt`, and the accumulated `time` offset from run start.
3. **End of run** → close the final span.

This means a scene that sits still for 100 ticks produces **one** collapsed span, not 100 identical
snapshots — while an active jump arc produces a dense series of small deltas. `dt` is captured (from
the same value fed to `Update()`) so the collapsed duration is exact and shared with the input
recording's timebase (see [InputRecordPlayback.md](InputRecordPlayback.md)).

---

## Baseline TOML schema

```toml
# baseline.toml
[meta]
format_version = 1
engine_build   = "UGE dev 2026-08-30"
epsilon        = 0.0001          # global epsilon (overridable by manifest)
total_ticks    = 540
timebase       = "fixed"         # fixed timestep (headless)

# ── Tick 0: full keyframe ─────────────────────────────────────────────
[[frame]]
tick = 0
dt   = 0.033333
time = 0.0
hold_ticks = 1                   # this state held for 1 tick before the next change
kind = "keyframe"
[frame.state]
"player.pos"        = { x = 100.0, y = 0.0 }
"player.vel"        = { x = 0.0, y = 0.0 }
"physics.gravity.y" = -9.81
"player.grounded"   = 1

# ── A collapsed span: nothing changed for 87 ticks (~2.9 s) ───────────
[[frame]]
tick = 1
dt   = 0.033333
time = 0.033333
hold_ticks = 87                  # unchanged for 87 ticks; duration ≈ 87 * dt
kind = "hold"                    # no [frame.state] — identical to previous

# ── A delta: jump begins at tick 88 ──────────────────────────────────
[[frame]]
tick = 88
dt   = 0.033333
time = 2.933304
hold_ticks = 1
kind = "delta"
[frame.state]                    # ONLY changed keys
"player.vel"      = { x = 0.0, y = 420.0 }
"player.grounded" = 0
```

- **Value types** mirror `DataValue`: `int`, `float`, `string`, and `Vec2` (`{ x, y }`), matching
  the DataStore's existing TOML round-trip (`key = { x, y }`).
- A `hold` frame carries no `[frame.state]`; the comparer reuses the last known values for the
  span's duration.
- Reconstructing the full state at any tick = keyframe + apply deltas in order up to that tick.

---

## Comparison — global epsilon

During replay the harness reconstructs the expected state at each tick from the baseline and
compares against the live DataStore:

- **Numeric (`float`, and each `Vec2` component):** pass if `abs(actual - expected) <= epsilon`.
  A single **global epsilon** applies to all numeric keys (from `manifest.baseline.epsilon`,
  default `0.0001`). Per-key tolerances are intentionally out of scope for now.
- **Integer:** exact match required (epsilon does not apply; ints are discrete state like
  `grounded`, counts, enum-ish flags).
- **String:** exact match required.
- **Missing / extra keys:** a key present in the baseline but absent live (or vice-versa) is a
  **failure**, reported with the key name and tick.

### Failure record

Each mismatch is captured for the report (see
[TestRunnerAndReporting.md](TestRunnerAndReporting.md)):

```
tick 132  key "player.pos.y"  expected 318.40  actual 402.75  |Δ| 84.35  > epsilon 0.0001
```

The run's verdict is **PASS** only if every compared tick matches for every in-scope key.

---

## Capture vs. compare split (core vs. plugin)

| Responsibility | Home | Guarded? |
|---|---|---|
| Walk `DataStore`, filter by `DataMeta`, serialize entries to TOML | Core (`UGEDataLayer`) | No |
| Diff current vs. previous emitted values | Plugin (harness) | Yes |
| Collapse holds, write baseline TOML | Plugin (harness) | Yes |
| Reconstruct expected state, epsilon compare, verdict | Plugin (harness) | Yes |

The core exposes a snapshot/serialize helper (useful to any tool, e.g. a save-state inspector or
server telemetry); the harness owns the *policy* of when to snapshot and how to judge it.

---

## Why delta capture matters for determinism proof

A dense, every-tick baseline is the strongest determinism proof but is large and noisy in diffs.
Delta capture keeps the signal — every *change* is recorded with its exact tick and time — while
collapsing quiescent spans. A regression in determinism shows up as either a value drift beyond
epsilon **or** a change appearing on a different tick than the baseline recorded (a collapsed span
that ends early/late), both of which the comparer flags precisely.

