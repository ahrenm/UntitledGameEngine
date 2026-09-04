# Testing — Test Runner & Reporting

> Design record. Implementation deferred. The runner and reporter live in the guarded
> `TestHarness` plugin. See [Architecture.md](Architecture.md).

Defines how a test run is driven, how results reach the console and disk, and how the process exit
code communicates pass/fail to callers (CI, agents, scripts).

Related: [HeadlessExecution.md](HeadlessExecution.md) ·
[InputRecordPlayback.md](InputRecordPlayback.md) ·
[StateInstrumentation.md](StateInstrumentation.md)

---

## Run Lifecycle

The `TestRunner` (owned by `TestHarnessLayer`) drives a single run:

```
1. Arm      — read test.* flags; load manifest; load recording (+ baseline for replay).
2. Setup    — run manifest init_script for deterministic scene construction.
3. Drive    — per tick: PreTick injects input & dt; engine ticks; PostTick snapshots/compares.
4. Stop     — recorded QUIT reached, queue exhausted, max_ticks hit, or a fatal mismatch policy.
5. Report   — emit console + file report; set exit code; request application shutdown.
```

A run always terminates deterministically: it never depends on a human closing a window. `max_ticks`
in the manifest is a hard safety cap that fails the run if exceeded (guards against a scene that
never reaches its recorded QUIT).

### Single-run vs. batch

The first cut runs **one manifest per process invocation** (simplest, cleanest isolation — a fresh
engine per test). A batch is achieved by an external script iterating manifests; a future in-process
batch mode is noted but not designed here.

---

## Console Output

Human- and agent-readable, streamed as the run progresses and summarized at the end.

```
[TEST] platformer_jump  (replay, headless)
[TEST]   manifest : tests/platformer_jump/manifest.toml
[TEST]   recording: 540 ticks, 128 events
[TEST]   baseline : 47 frames (1 keyframe, 46 deltas), epsilon 0.0001
[TEST]   running..................................................  540/540 ticks
[TEST] FAIL  2 mismatches
[TEST]   tick 132  player.pos.y  expected 318.40  actual 402.75  |Δ|84.35 > 0.0001
[TEST]   tick 133  player.vel.y  expected 415.00  actual 470.10  |Δ|55.10 > 0.0001
[TEST] result: FAIL  (elapsed 0.41s sim, 540 ticks)
```

- Lines are prefixed `[TEST]` and routed through `LoggingLayer` (so they appear in the engine log
  buffer too) **and** to `stdout` for CLI/CI capture.
- Progress is throttled (one dot per N ticks) to stay readable under fast headless runs.
- A **PASS** run prints a one-line green-path summary; a **FAIL** lists up to a configurable number
  of mismatches (default 20) then a count of the remainder.

---

## File Report (TOML)

Written next to the manifest (or to `--test-report <path>`), in TOML to match all other artifacts.

```toml
# result.toml
[result]
name     = "platformer_jump"
mode     = "replay"
headless = true
verdict  = "fail"            # "pass" | "fail" | "error"
ticks    = 540
sim_seconds = 0.41
epsilon  = 0.0001
mismatch_count = 2

[[mismatch]]
tick     = 132
key      = "player.pos.y"
expected = 318.40
actual   = 402.75
delta    = 84.35

[[mismatch]]
tick     = 133
key      = "player.vel.y"
expected = 415.00
actual   = 470.10
delta    = 55.10

[environment]
engine_build = "UGE dev 2026-08-30"
platform     = "windows"
created      = "2026-08-30T12:04:11Z"
```

- `verdict = "error"` is distinct from `"fail"`: **error** = the run could not be evaluated
  (missing recording, setup script failed, `max_ticks` exceeded); **fail** = it ran and state
  diverged.

---

## Command-Line Control & Exit Codes

CLI flags are parsed in the Application target and flow into `LaunchSettings`
(see [HeadlessExecution.md](HeadlessExecution.md)). Testing-specific flags:

| Flag | Effect |
|---|---|
| `--test` | Activate testing (`test.active=1`); mode from manifest. |
| `--test-mode <record\|replay>` | Set mode explicitly. |
| `--test-manifest <path>` | Manifest to run. |
| `--test-report <path>` | Override report output path. |
| `--test-epsilon <float>` | Override the manifest/global epsilon. |
| `--headless` | Run offscreen (recommended for replay). |

### Exit codes

The process exit code is the primary machine-readable signal (drives CI and agents):

| Code | Meaning |
|---|---|
| `0` | PASS — all compared ticks within epsilon. |
| `1` | FAIL — state diverged from baseline. |
| `2` | ERROR — run could not be evaluated (bad manifest, setup failure, timeout). |
| `3` | USAGE — invalid flags / missing required arguments. |

Because `main()` returns an `int`, the harness communicates its verdict back up through
`UGEApplication::Run()` to `main` (a small return-channel: the harness stores the verdict in a
`test.result` DataStore key that `main.cpp` reads after `Run()` returns, keeping the plumbing
unguarded and data-only).

---

## Interaction with the Engine Shutdown Path

- The harness requests shutdown by setting the SDL running flag false (pushing `SDL_EVENT_QUIT`),
  reusing the existing loop-exit path in
  [`UGEApplication::Run()`](../../source/UGECore/Private/UGEApplication.cpp) — no new shutdown
  mechanism.
- Reporting happens in `PostTick` on the terminating tick (or a final flush in the harness
  destructor) so the report is written even if the run ends via QUIT.

