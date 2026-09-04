# Automated Testing System — Design Record

> **Status:** Design / planning. **Implementation is deferred.** This directory captures the
> goals, architecture, and phased implementation steps agreed during the testing-system planning
> session. No test code exists yet; these documents are the contract the implementation will
> follow.

The Untitled Game Engine (UGE) is **certified deterministic**. That property is the foundation
for an automated testing system built on **record → replay → compare**: capture real input and a
baseline of game state over time, replay the input headlessly, and assert that the resulting
state matches the baseline within a tolerance.

---

## Goals

1. **Record user input over time** — capture SDL input, tagged per tick, to a replayable file.
2. **Playback of sessions** — deterministically re-inject a recording to reproduce a run.
3. **Headless execution** — full physics/animation simulation, no visible output, CLI-driven.
4. **State instrumentation** — snapshot game state over time as the pass/fail baseline, with a
   tweakable tolerance (global epsilon) because the engine is deterministic.
5. **Test output** — console + file reporting, with command-line control and exit codes.
6. **AI Agent skills** — documented skills so agents can author, run, and triage tests.

## Non-Goals (for the first cut)

- No GPU pixel-diffing / screenshot comparison. State comparison only.
- No cross-platform CI matrix yet — **Windows is the primary target**; other platforms are
  documented for future work.
- No networked / multi-instance test orchestration.

---

## Two Foundational Design Principles

### 1. Opt-in at build **and** run time

- **Build time:** all test *orchestration* compiles out behind the `UGE_ENABLE_TESTING`
  compile definition. A non-test build carries zero test-orchestration cost.
- **Run time:** even in a test-enabled build, testing is inert until switched on. A single
  `test.active` flag lives in the `DataStore` (seeded from `LaunchSettings`). The
  `TestHarnessLayer` **logs and gracefully no-ops** when the flag is false or when built without
  `UGE_ENABLE_TESTING`.

### 2. Reusable primitives in core (unguarded) — orchestration in the plugin (guarded)

Core API enhancements needed to support testing are **not** `#ifdef`-guarded, *provided their
implementation carries no dependency on test infrastructure*. They are general engine features
that may find reuse elsewhere (for example a future **headless server** build).

| Capability | Home | Guarded? | Rationale |
|---|---|---|---|
| Headless / offscreen render mode | Core (`SDLLayer`, `LaunchSettings`) | **No** | General engine feature; reusable by a server build. |
| Layer-bootstrap skip-list | Core (`LayerRegistry` / `UGEApplication`) | **No** | General composition control. |
| DataStore snapshot emission | Core (`UGEDataLayer`) | **No** | General introspection/serialization feature. |
| Delta-time already surfaced to the loop | Core (`UGEApplication::Run`) | **No** | Already exists. |
| Record/replay driving, snapshot compare, runner | `TestHarness` plugin | **Yes** | Pure test orchestration. |
| Hooks in `Run()` / event pump | Core call sites | **Yes** (single lines) | Only the *call into* the harness is guarded. |

**Rule of thumb:** if a feature is useful to a non-test build, it lives unguarded in core; the
test-specific *decisions* that drive it live in guarded/plugin code.

---

## File-Format Decision — TOML everywhere

All test artifacts (input recordings, state baselines, run manifests, reports) are **TOML**,
matching the engine's existing data conventions (`UGESettings.toml`, `assets/DATA/*.toml`). TOML
is diffable, human-readable, and agent-editable — all valuable for authoring and triaging tests.

---

## Document Map

| Doc | Read when… |
|---|---|
| [Architecture.md](Architecture.md) | You need the component layout: plugin vs. core, the guarded hooks, the `test.active` contract. |
| [InputRecordPlayback.md](InputRecordPlayback.md) | Working on capturing or injecting SDL input; the recording TOML schema. |
| [HeadlessExecution.md](HeadlessExecution.md) | Running without a visible window; offscreen render vs. layer-skip; CLI→`LaunchSettings`. |
| [StateInstrumentation.md](StateInstrumentation.md) | Snapshotting state, delta capture, the baseline TOML schema, epsilon comparison. |
| [TestRunnerAndReporting.md](TestRunnerAndReporting.md) | Console/file reporting, exit codes, command-line control. |
| [AIAgentSkills.md](AIAgentSkills.md) | Defining agent skills to author, run, and triage tests. |

---

## Scope Note

**Windows is the primary and only actively-supported target.** Linux/macOS notes appear where
relevant (chiefly headless execution) so the design does not paint itself into a corner, but they
are explicitly out of scope for the first implementation.

