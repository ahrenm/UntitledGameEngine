# UGE Documentation

Programmer & systems documentation for the **Untitled Game Engine (UGE)** — a C++23 desktop
engine built on **SDL3 + RmlUi** with **PhysicsFS** as a virtual filesystem.

For a fast, self-contained orientation aimed at AI coding agents, start with
[`AGENTS.md`](../AGENTS.md) at the repository root. This library holds the deep-dive detail
that `AGENTS.md` links out to.

---

## Table of Contents

### Architecture
| Doc | Read this when… |
|---|---|
| [architecture/Overview.md](architecture/Overview.md) | You need the big picture — subsystems, DLL boundaries, the module diagram. |
| [architecture/LayerSystem.md](architecture/LayerSystem.md) | Working with `AppLayer`, `ServiceLocator`, `pushLayer`, or the `REGISTER_*` registries. |
| [architecture/FrameLoop.md](architecture/FrameLoop.md) | You need to understand tick ordering, the 30 FPS cap, or `Update`/`Draw` phases. |

### Systems
| Doc | Read this when… |
|---|---|
| [systems/AssetSystem.md](systems/AssetSystem.md) | Loading assets, mounting the VFS, or using `OVERRIDE/assets`. |
| [systems/DataStore.md](systems/DataStore.md) | Reading/writing the unified `DataStore`, bindings, TOML datasets. |
| [systems/LuaIntegration.md](systems/LuaIntegration.md) | Exposing C++ to Lua, tick functions, or the init script. |
| [systems/SceneSystem.md](systems/SceneSystem.md) | Authoring scenes and understanding scene lifecycle. |
| [systems/UISystem.md](systems/UISystem.md) | Building RmlUi documents, ViewModels, fragments, or windows. |
| [systems/Physics.md](systems/Physics.md) | Using Box2D bodies, contacts, or tuning the physics world. |
| [systems/Collision.md](systems/Collision.md) | AABB collision helpers, the debug overlay, or broad-phase grids. |
| [systems/SpriteSystem.md](systems/SpriteSystem.md) | Drawing sprites, sprite sheets, or animated sprites. |
| [systems/Animation.md](systems/Animation.md) | Building TOML-driven, state-selected animation controllers. |
| [systems/Rendering3D.md](systems/Rendering3D.md) | Using the TeaPot3D CPU-rasterised 3D viewport. |
| [systems/InputEvents.md](systems/InputEvents.md) | Handling SDL events and understanding propagation. |

### Reference
| Doc | Read this when… |
|---|---|
| [reference/Configuration.md](reference/Configuration.md) | Editing `UGESettings.toml` or `LaunchSettings`. |
| [reference/BuildWorkflow.md](reference/BuildWorkflow.md) | Building from CLion or a terminal; linker/MinGW notes. |
| [reference/CodingConventions.md](reference/CodingConventions.md) | Writing new code — naming rules and exceptions. |
| [reference/KeyFiles.md](reference/KeyFiles.md) | Locating the header/source for a subsystem. |
| [reference/LuaApiReference.md](reference/LuaApiReference.md) | Looking up any Lua-exposed table or function. |

### Testing (design record — implementation deferred)
| Doc | Read this when… |
|---|---|
| [testing/README.md](testing/README.md) | You need the goals, opt-in philosophy, and the core-vs-orchestration split for automated testing. |
| [testing/Architecture.md](testing/Architecture.md) | Working with the `TestHarness` plugin, the guarded hooks, or the `test.active` flag. |
| [testing/InputRecordPlayback.md](testing/InputRecordPlayback.md) | Recording or replaying SDL input; the recording/manifest TOML schema. |
| [testing/HeadlessExecution.md](testing/HeadlessExecution.md) | Running offscreen; RmlUi-headless feasibility; CLI→`LaunchSettings`. |
| [testing/StateInstrumentation.md](testing/StateInstrumentation.md) | Delta state snapshots, the baseline TOML schema, and epsilon comparison. |
| [testing/TestRunnerAndReporting.md](testing/TestRunnerAndReporting.md) | Console/file reporting, exit codes, and command-line control. |
| [testing/AIAgentSkills.md](testing/AIAgentSkills.md) | Authoring, running, or triaging tests as an agent. |

### Design Records
| Doc | Purpose |
|---|---|
| [DataStore_Design.md](DataStore_Design.md) | Authoritative design record for the unified metadata-driven `DataStore`. |
| [DataStore_Refactor_Tasks.md](DataStore_Refactor_Tasks.md) | Implementation task breakdown for the `DataStore` refactor. |

