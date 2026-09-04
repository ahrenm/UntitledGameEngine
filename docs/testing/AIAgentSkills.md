# Testing — AI Agent Skills

> Design record. Implementation deferred. Defines the agent-facing "skills" (repeatable
> procedures) for authoring, running, and triaging automated tests once the system is built.

These skills assume the system described in the sibling documents exists: a `TestHarness` plugin,
TOML recordings/baselines/manifests, headless execution, and exit-code-based verdicts. They are
written so an AI coding agent can execute them with the tools it has (terminal, file editing).

Related: [TestRunnerAndReporting.md](TestRunnerAndReporting.md) ·
[HeadlessExecution.md](HeadlessExecution.md) · [../../AGENTS.md](../../AGENTS.md)

---

## Prerequisites an agent must know

- Test-enabled build: configure/build with `UGE_ENABLE_TESTING` defined (a CMake option/preset,
  finalized at implementation). A normal build has **no** test capability by design.
- The `untitled.exe` in `cmake-build-debug/` is the harness host; the `TestHarness` module must be
  listed in `UGESettings.toml` `[Modules] Load` (or a test-specific settings file).
- All artifacts are TOML; exit codes are the source of truth (`0` pass, `1` fail, `2` error,
  `3` usage — see [TestRunnerAndReporting.md](TestRunnerAndReporting.md)).

---

## Skill 1 — Author a new test (record)

**Goal:** capture a new input recording and its baseline for a scenario.

1. Create a manifest `tests/<name>/manifest.toml` with a deterministic `run.init_script`, `headless`,
   optional `skip_layers`, and `max_ticks` (schema in
   [InputRecordPlayback.md](InputRecordPlayback.md#run-manifest-ties-input--baseline-together)).
2. Author a deterministic setup Lua script that builds the scene identically every run (no
   wall-clock/random unless seeded from the manifest).
3. Run in record mode (usually **headed**, so a human can perform the input):
   ```powershell
   .\untitled.exe --test-mode record --test-manifest tests\<name>\manifest.toml
   ```
4. Perform the interaction; close the window (or trigger the recorded QUIT) to flush
   `recording.toml` and the initial `baseline.toml`.
5. Commit `manifest.toml`, `recording.toml`, `baseline.toml`.

**Verification:** immediately replay (Skill 2) and confirm exit code `0` — a freshly recorded test
must pass against its own baseline.

---

## Skill 2 — Run a test (replay) and read the verdict

**Goal:** execute a test headlessly and determine pass/fail.

```powershell
.\untitled.exe --test-mode replay --headless --test-manifest tests\<name>\manifest.toml
echo "exit=$LASTEXITCODE"
```

- `exit=0` → PASS. Done.
- `exit=1` → FAIL. Proceed to Skill 3.
- `exit=2` → ERROR. The run couldn't be evaluated — check the console log for a missing file,
  setup-script error, or `max_ticks` timeout; fix the harness input, not the engine.
- `exit=3` → USAGE. Fix the command-line flags.

Parse `tests/<name>/result.toml` for structured details rather than scraping stdout.

---

## Skill 3 — Triage a failing test

**Goal:** decide whether a FAIL is a real regression or an expected behavior change.

1. Read `result.toml`; note the **first** mismatch's `tick` and `key` (earliest divergence is the
   root cause; later ones are usually downstream).
2. Map the `key` to a subsystem (e.g. `player.pos.*`/`physics.*` → physics or movement;
   `ui.*` → UI/ViewModel). Use [reference/KeyFiles.md](../reference/KeyFiles.md).
3. Inspect recent changes to that subsystem (git history / diff).
4. Decide:
   - **Regression** → the code change broke determinism or behavior; fix the code, re-run (Skill 2).
   - **Intended change** → the new behavior is correct; **re-baseline** (Skill 4).
5. If `|Δ|` is tiny and just over epsilon across many keys, suspect a determinism/precision issue or
   an epsilon that is too tight — consider `--test-epsilon` to confirm before changing the manifest.

---

## Skill 4 — Re-baseline an intentionally changed test

**Goal:** accept new correct behavior as the baseline.

1. Confirm (Skill 3) the change is intended.
2. Re-record the baseline against the **existing** recording (input unchanged, state expectations
   refreshed):
   ```powershell
   .\untitled.exe --test-mode record --headless --test-manifest tests\<name>\manifest.toml
   ```
   (Record mode replays the existing `recording.toml` if present and rewrites `baseline.toml`; if
   the input itself changed, re-author via Skill 1.)
3. Diff the new `baseline.toml` — the TOML delta format makes the behavioral change reviewable.
4. Replay (Skill 2) to confirm exit `0`; commit the updated baseline with a message explaining the
   intended behavior change.

---

## Skill 5 — Add tests to CI / batch execution

**Goal:** run the suite and fail the build on any regression.

1. Iterate every `tests/**/manifest.toml`.
2. Replay each headlessly; collect exit codes.
3. The suite passes only if **all** exit codes are `0`; surface each non-zero with its
   `result.toml` summary.

```powershell
$fail = 0
Get-ChildItem -Recurse -Filter manifest.toml tests | ForEach-Object {
    .\untitled.exe --test-mode replay --headless --test-manifest $_.FullName
    if ($LASTEXITCODE -ne 0) { $fail++; Write-Host "FAIL: $($_.FullName)" }
}
exit $fail
```

---

## Skill authoring guidance (for maintaining these skills)

- Prefer **exit codes and `result.toml`** over stdout scraping — they are stable contracts.
- Always run replay **headless** for speed and display-independence; record may be headed.
- Never edit `recording.toml`/`baseline.toml` by hand to force a pass — re-baseline via Skill 4.
- Keep setup scripts deterministic; a flaky test is a determinism bug to fix, not tolerate.

