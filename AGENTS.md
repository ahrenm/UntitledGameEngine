# AGENTS.md - Codebase Guide for AI Coding Agents

> **This file is the fast-access core.** It holds the essentials an agent needs on every task.
> For deep dives, follow the **[Documentation Map](#documentation-map)** into `docs/`.

## Project Overview

**Untitled Game Engine (UGE)** - C++23 desktop application using **SDL3 + RmlUi** for rendering
and UI, with **PhysicsFS** as a virtual filesystem for asset loading. All dependencies are
fetched at configure time via CMake `FetchContent` (SDL3, SDL3_image, RmlUi, PhysicsFS,
FreeType, Lua 5.4, sol2, **glm**, **tinyobjloader**, **Box2D**).

## Architecture at a Glance

Three subsystems (full detail: [docs/architecture/Overview.md](docs/architecture/Overview.md)):

1. **Application** (`source/Application/`) - Entry point; reads `UGESettings.toml`, loads plugin
   DLLs via `ModuleLoader`, calls `UGEApplication::Create()`. Builds `untitled.exe`. No engine
   logic.
2. **UGECore** (`source/UGECore/`) - Core engine library (`libUGECore.dll`): base classes,
   core layers, static registries, shared components (sprites, animation, collision, windows),
   Lua integration.
3. **Game Plugins** (`source/GamePlugins/`) - Loadable DLLs (`UIWindows`, `DemoGame`,
   `TeaPot3D`) that extend the engine, all discovered via static `REGISTER_*` macros at DLL load.

**Underpinning all**: the Assets & Data systems load binary resources and TOML data tables
through the `PhysFSLayer` VFS.

```
untitled.exe (Application)  ->  UGECore (libUGECore.dll)  <-  Plugins: UIWindows / DemoGame / TeaPot3D
                                        |
                     Assets & Data via PhysFSLayer VFS: /assets (binary) + /assets/DATA (TOML) + OVERRIDE/assets
```

### Layer load order (auto-instantiated via `LayerRegistry`, ascending)

| # | Layer | Role |
|---|---|---|
| 1 | `LoggingLayer` | Log-line buffer; pushed first. |
| 2 | `PhysFSLayer` | Virtual filesystem (assets.pak + OVERRIDE/assets). |
| 3 | `UGEDataLayer` | Unified metadata-driven `DataStore` (pushed by default). |
| 4.0 | `SDLLayer` | Window, renderer, background, event loop, 2-D camera. |
| 4.05 | `SceneManagerLayer` | Owns the single active `Scene`. |
| 4.1 | `PhysicsLayer` | Box2D world; step + contacts + debug draw. |
| 4.5 | `Render3DLayer` | (plugin, TeaPot3D) CPU-rasterised 3D viewport. |
| 5 | `LuaLayer` | sol2 VM + scriptable-object registry. |
| 6 | `RmlUILayer` | RmlUi context, documents, ViewModels. |
| 10+ | Plugin layers | Registered in plugin headers. |

**Frame loop** (`UGEApplication::Run()`): 30 FPS-capped. Each tick: `Update()` on every layer in
push order (input -> logic), then `BeginFrame()`, `Draw(dt)` on every layer in push order, then
`EndFrame()`. Details: [docs/architecture/FrameLoop.md](docs/architecture/FrameLoop.md).

## Golden Rules (apply on every task)

- **Layers derive from `AppLayer`**, are added via `pushLayer<T>()` (takes ownership, calls the
  virtual `RegisterWithServiceLocator()` hook). Access services anywhere with
  `ServiceLocator::Get<T>()` / `TryGet<T>()`. It is **non-owning** - never outlive the owning
  layer. In `Scene`/`ViewModel` code, prefer `GameObjectBase` accessors (`GetSDLLayer()`,
  `GetDataLayer()`, ...). Full detail: [docs/architecture/LayerSystem.md](docs/architecture/LayerSystem.md).
- **Factory pattern with `std::expected`**: constructors are private; use `::Create()` and
  propagate errors with `return std::unexpected(...)`.
- **Static registry macros go in a `public:` section of the class header** so registration fires
  at DLL load - never wire manually in `main()` / `Create()`:
  - `REGISTER_LAYER(name, loadOrder, Type)` -> `LayerRegistry`
  - `REGISTER_SCENE(name, Type)` -> `SceneRegistry`
  - `REGISTER_VIEWMODEL(name, Type)` -> `ViewModelRegistry`
- **VFS paths always start with `assets/...`** (no leading slash), e.g. `"assets/ui/main.rml"`.
  All file I/O (including RmlUi) goes through `PhysFSLayer`.
- **One-at-a-time containers**: exactly one `Scene` and one UI `Document` are live; loading a new
  one tears down the previous automatically.
- **Add/remove plugins by editing `UGESettings.toml`** - no DLL names are hardcoded in `main.cpp`.

## Coding Conventions

| Category | Convention | Example |
|---|---|---|
| Public functions | PascalCase | `LoadDocument()`, `SetBackground()` |
| Private functions | camelCase | `extractDataModelNames()`, `lastError()` |
| Private member variables | camelCase with `m_` | `m_window`, `m_pageViewModels` |
| Function parameters | camelCase | `void Log(std::string Msg)` |
| Local variables | camelCase | `auto Result = ...` |
| Macros / `constexpr` / `const` | ALL_CAPS with underscores | `REGISTER_VIEWMODEL`, `RMLUI_SDL_VERSION_MAJOR` |

Exceptions: public data members (e.g. `onLog`) are camelCase; third-party override methods (e.g.
`Rml::FileInterface::Open`) keep inherited names. Full notes:
[docs/reference/CodingConventions.md](docs/reference/CodingConventions.md).

## Build Essentials

**IDE**: CLion on Windows manages CMake and `cmake-build-debug/`. Reload via
**File -> Reload CMake Project** after editing `CMakeLists.txt` / `FetchContent` deps. Do **not**
run `cmake -B` manually inside CLion workflows.

For **agent/tool-driven terminal builds**, prepend CLion's MinGW `bin` to `PATH` first (its
cached GCC needs helper tools like `as.exe`), then use the absolute-path build form:

```powershell
$env:PATH = 'C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin;' + $env:PATH
cmake --build C:\Users\ahren\CLionProjects\untitled\cmake-build-debug --target all -j 62
```

`untitled.exe` links `libUGECore.dll`. Public headers live under `source/UGECore/Public/`
(include root), e.g. `#include <Layers/RmlUILayer.h>`. Full workflow, compile definitions, and
the MinGW symbol-export note: [docs/reference/BuildWorkflow.md](docs/reference/BuildWorkflow.md).

## Documentation Map

Deep-dive documentation lives in [`docs/`](docs/README.md). Read the relevant file when a task
touches that system.

### Architecture
| Doc | Read when... |
|---|---|
| [docs/architecture/Overview.md](docs/architecture/Overview.md) | You need the big picture: subsystems, DLL boundaries, module + lifecycle diagrams. |
| [docs/architecture/LayerSystem.md](docs/architecture/LayerSystem.md) | Working with `AppLayer`, `ServiceLocator`, `pushLayer`, `REGISTER_*` registries, or `Create()`. |
| [docs/architecture/FrameLoop.md](docs/architecture/FrameLoop.md) | You need tick ordering, the 30 FPS cap, or Update/Draw phases. |

### Systems
| Doc | Read when... |
|---|---|
| [docs/systems/AssetSystem.md](docs/systems/AssetSystem.md) | Loading assets, mounting the VFS, `OVERRIDE/assets`, `PhysFSLayer` helpers. |
| [docs/systems/DataStore.md](docs/systems/DataStore.md) | Reading/writing the unified `DataStore`, `DATA_BIND*`, TOML datasets, `Vec2`. |
| [docs/systems/LuaIntegration.md](docs/systems/LuaIntegration.md) | Exposing C++ to Lua, `IScriptableObject`, tick functions, init script. |
| [docs/systems/SceneSystem.md](docs/systems/SceneSystem.md) | Authoring scenes; scene lifecycle, teardown ordering. |
| [docs/systems/UISystem.md](docs/systems/UISystem.md) | RmlUi documents, ViewModels, fragments, the window framework. |
| [docs/systems/Physics.md](docs/systems/Physics.md) | Box2D bodies, handles, contacts, `physics.*` tunables. |
| [docs/systems/Collision.md](docs/systems/Collision.md) | AABB helpers, debug overlay, broad-phase grid. |
| [docs/systems/SpriteSystem.md](docs/systems/SpriteSystem.md) | Sprites, sprite sheets, animated sprites. |
| [docs/systems/Animation.md](docs/systems/Animation.md) | TOML-driven, state-selected animation controllers. |
| [docs/systems/Rendering3D.md](docs/systems/Rendering3D.md) | The TeaPot3D CPU-rasterised 3D viewport. |
| [docs/systems/InputEvents.md](docs/systems/InputEvents.md) | SDL event dispatch, consumption, propagation, `GameObjectBase` accessors. |

### Reference
| Doc | Read when... |
|---|---|
| [docs/reference/Configuration.md](docs/reference/Configuration.md) | Editing `UGESettings.toml` or `LaunchSettings`. |
| [docs/reference/BuildWorkflow.md](docs/reference/BuildWorkflow.md) | Building; compile definitions; MinGW/linker notes. |
| [docs/reference/CodingConventions.md](docs/reference/CodingConventions.md) | Writing new code - naming rules and exceptions. |
| [docs/reference/KeyFiles.md](docs/reference/KeyFiles.md) | Locating the header/source for any subsystem. |
| [docs/reference/LuaApiReference.md](docs/reference/LuaApiReference.md) | Looking up any Lua-exposed table or function. |

### Design Records
| Doc | Purpose |
|---|---|
| [docs/DataStore_Design.md](docs/DataStore_Design.md) | Authoritative `DataStore` design record. |
| [docs/DataStore_Refactor_Tasks.md](docs/DataStore_Refactor_Tasks.md) | `DataStore` refactor task breakdown. |

### Testing (design record — implementation deferred)
| Doc | Read when... |
|---|---|
| [docs/testing/README.md](docs/testing/README.md) | Goals, opt-in build/run philosophy, and the core-vs-orchestration split for automated testing. |
| [docs/testing/Architecture.md](docs/testing/Architecture.md) | The `TestHarness` plugin, the guarded `Run()`/event-pump hooks, and the `test.active` flag. |
| [docs/testing/InputRecordPlayback.md](docs/testing/InputRecordPlayback.md) | Recording/replaying SDL input; recording + run-manifest TOML schema. |
| [docs/testing/HeadlessExecution.md](docs/testing/HeadlessExecution.md) | Offscreen render vs. layer-skip; RmlUi-headless feasibility; CLI→`LaunchSettings`. |
| [docs/testing/StateInstrumentation.md](docs/testing/StateInstrumentation.md) | Delta snapshots with frame collapsing; baseline TOML; global-epsilon compare. |
| [docs/testing/TestRunnerAndReporting.md](docs/testing/TestRunnerAndReporting.md) | Console/file reports, exit codes, command-line control. |
| [docs/testing/AIAgentSkills.md](docs/testing/AIAgentSkills.md) | Authoring, running, and triaging tests as an agent. |

