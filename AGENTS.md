# AGENTS.md � Codebase Guide for AI Coding Agents

## Project Overview
**Untitled Game Engine** � C++23 desktop application using **SDL3 + RmlUi** for rendering and UI, with **PhysicsFS** as a virtual filesystem for asset loading. All dependencies are fetched at configure time via CMake `FetchContent` (SDL3, SDL3_image, RmlUi, PhysicsFS, FreeType, Lua 5.4, sol2, **glm**, **tinyobjloader**).

## Architecture

### Codebase Structure

The project is organized into three broad **subsystems**:

1. **Application** (`source/Application/`) — Platform-specific entry point and launching code. Reads `UGESettings.toml`, loads plugin DLLs via `ModuleLoader`, and calls `UGEApplication::Create()` to bootstrap the engine. This is where the executable (`untitled.exe`) is built; no engine logic lives here.

2. **UGECore** (`source/UGECore/`) — The core engine library (shared DLL `libUGECore.dll`). Contains:
   - Base classes (`AppLayer`, `SceneObject`, `ViewModel`, `GameObjectBase`)
   - Core layers (`LoggingLayer`, `PhysFSLayer`, `UGEDataLayer`, `SDLLayer`, `LuaLayer`, `RmlUILayer`)
   - Static registries (`LayerRegistry`, `SceneRegistry`, `ViewModelRegistry`)
   - Shared components (sprite system, box collision, window view-model framework)
   - Lua scripting infrastructure and Lua API surfaces

3. **Game Plugins** (`source/GamePlugins/`) — Loadable DLL modules that extend the engine. Each plugin can contain:
   - **Game Scenes & ViewModels** + supporting code (e.g., `DemoGame/Platformer/` — platformer scene, character physics, HUD)
   - **Shared/Reusable Components** (e.g., `UIWindows/` — base window model, derived window implementations)
   - **Custom Layers** (e.g., `TeaPot3D/Render3DLayer` — alternative rendering backend)
   - All registered via static `REGISTER_LAYER` / `REGISTER_SCENE` / `REGISTER_VIEWMODEL` macros; discovered automatically at DLL load

**Underpinning All**: The **Assets & Data** systems provide binary resources (textures, animations, 3D models) and arbitrary TOML data tables (game config, tile maps, entity properties). Both are loaded through the virtual filesystem (`PhysFSLayer`) at runtime.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          untitled.exe (Application)                      │
│                     Reads UGESettings.toml, loads plugins               │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │
                                 ↓
                    ┌────────────────────────────┐
                    │   UGECore (libUGECore.dll) │
                    │ - Base classes & registries│
                    │ - Core layers (1–6)        │
                    │ - Shared components        │
                    │ - Lua integration          │
                    └────────────────────────────┘
                                 ↑
                ┌────────────────┼────────────────┐
                │                │                │
        ┌──────────────┐  ┌─────────────┐  ┌─────────────┐
        │ UIWindows.   │  │ DemoGame.   │  │ TeaPot3D.   │
        │ dll          │  │ dll         │  │ dll         │
        ├──────────────┤  ├─────────────┤  ├─────────────┤
        │ • Window VM  │  │ • Platformer│  │ • Render3D  │
        │   (reusable) │  │   Scene     │  │   Layer     │
        │              │  │ • Character │  │ • Teapot    │
        │              │  │ • HUD VM    │  │   Demo      │
        │              │  │ • Other VMs │  │ • Viewport  │
        └──────────────┘  └─────────────┘  └─────────────┘
             (Shared)       (Game content)    (Rendering)
                                 │
                ┌────────────────┼────────────────┐
                │                │                │
         ┌──────────────┐ ┌──────────────┐ ┌─────────────┐
         │ /assets/     │ │ /assets/DATA/│ │ OVERRIDE/   │
         │ (binary)     │ │ (TOML)       │ │ assets/     │
         │ - sprites    │ │ - game data  │ │ (runtime)   │
         │ - textures   │ │ - tile maps  │ │ - overrides │
         │ - 3D models  │ │ - config     │ │ - dev files │
         └──────────────┘ └──────────────┘ └─────────────┘
         (Assets & Data System — accessed via PhysFSLayer VFS)
```

### Layer Hierarchy & Lifecycle

```
main.cpp
  +-- [Plugin Loading]           ? Loads DLLs listed in UGESettings.toml [Modules] via ModuleLoader
                                   (Static REGISTER_LAYER/REGISTER_SCENE initialisers fire at DLL load)
  +-- UGEApplication::Create()   ? factory returning std::expected; owns all subsystems; reads from UGEApplication::Settings
        +-- [Core Layers (Load Order 1–7, auto-instantiated via LayerRegistry)]
        +-- LoggingLayer (1)      ? owns the log-line buffer; must be pushed first
                                   (REGISTER_LAYER in UGECore defines; instantiated by Create())
        +-- PhysFSLayer (2)       ? virtual filesystem (assets.pak + OVERRIDE/assets/); reads ExecutablePath + MountPoints from UGEApplication::Settings and mounts during Create(); non-existent paths are skipped with a warning (not fatal)
        +-- UGEDataLayer (3)      ? key-value state bus; pushed by default after PhysFSLayer
        +-- SDLLayer (4)          ? window, renderer, background texture, event loop; reads window config from UGEApplication::Settings; InitCollisionHooks() called automatically in Create()
          +-- Scene              ? The active scene; only one at a time; owned by SDLLayer; scene teardown is automatic on load
                                   (REGISTER_SCENE in plugin headers; loaded dynamically via SDL.LoadScene() or SDLLayer::LoadScene())
        +-- [Render3DLayer (4.5)] ? CPU-rasterised 3D viewport; lives in TeaPot3D.dll (plugin), not UGECore; no-op until Activate(); fetches SDL_Renderer via ServiceLocator
                                   (REGISTER_LAYER in TeaPot3D.dll; loaded with plugin DLL)
        +-- LuaLayer (5)          ? sol::state VM + ScriptableObject registry; implements IEventHandler (currently non-consuming input handler)
        +-- RmlUILayer (6)        ? RmlUi context, document loading, per-frame render; Create() fetches SDL window/renderer from SDLLayer via ServiceLocator
          +-- Document           ? The active UI document; only one at a time; document teardown is automatic on load
            +-- Fragment(n)      ? Optional fragments injected via data-fragment; owned by RmlUILayer; instantiated automatically based on data-fragment attributes in the markup
          +-- ViewModel(n)       ? Owned by RmlUILayer; instantiated automatically based on data-model attributes in the markup
                                   (REGISTER_VIEWMODEL in plugin headers; instantiated by LoadDocument() via ViewModelRegistry)
        +-- [Plugin Layers (Load Order 10.0+, auto-instantiated via LayerRegistry)]
                                   (REGISTER_LAYER in plugin headers; static inline registration fires at DLL load, Create() loop pushes by load order)
```

All subsystems derive from **`AppLayer`** (`Layers/AppLayer.h`) and are stored in `UGEApplication::m_layers` via `pushLayer<T>()`, which takes ownership, calls `RegisterWithServiceLocator()` (virtual hook on `AppLayer`) where each concrete layer registers itself with `ServiceLocator::Provide(this)`. Use `ServiceLocator::Get<T>()` to access services anywhere; use `TryGet<T>()` when the service may not be present.

**Scenes** are gameplay containers owned exclusively by `SDLLayer`. At most one `SceneObject` is live at a time; loading a new scene automatically tears down the previous one. A scene's lifetime spans from its construction (inside `SDLLayer::LoadScene()`) until the next scene is loaded or the application shuts down. Within the frame loop, `SDLLayer` calls `Scene::Update()` during the `Update` pass and `Scene::Tick(float deltaTime)` during the `Tick` pass, after the background blit and before the SDL present — so scene geometry renders beneath any RmlUi overlay. Scenes can optionally inherit `IEventHandler` to receive SDL events before they reach the application dispatch chain, and `IScriptableObject` to expose a Lua API that is automatically registered/unregistered alongside the scene's lifetime. Access the currently active scene via `SDLLayer::ActiveScene()` (returns `SceneObject*`, or `nullptr`). Example reference: `source/GamePlugins/DemoGame/Platformer/PlatformerScene.h`.

`ViewModel` implementations are **not** layers. They self-register with `REGISTER_VIEWMODEL("model-name", Type)` in the header (`public:` section), are discovered from `data-model="..."` during `RmlUILayer::LoadDocument()`, and are created/destroyed with the page lifecycle. Use `ServiceLocator::Provide(this)` inside `RegisterWith()` only when global lookup is intentionally needed. Example references: `source/GamePlugins/DemoGame/LuaConsoleViewModel.h` (window-style VM, input + log binding) and `source/GamePlugins/DemoGame/Platformer/PlatformerHudViewModel.h` (HUD score binding).

**Frame loop** (`UGEApplication::Run()`): runs a **30 FPS-capped loop** using `SDL_GetTicksNS()` / `SDL_DelayNS()`. Each tick: `Update()` is called on each layer in push order (input ? logic), then `m_sdlLayer->BeginFrame()` clears the renderer, `Tick()` is called on each layer in **push order** (SDLLayer blits background + scene first, RmlUILayer composites UI on top, `m_sdlLayer->EndFrame()` presents last). If execution falls more than one full frame behind, `NextFrame` is reset to avoid catch-up bursts.

`UGEDataLayer` is a central key-value state bus and TOML dataset store pushed **by default** in `UGEApplication::Create()` (immediately after `PhysFSLayer`, before `LuaLayer`). It exposes three public store objects:
- **`State`** (`PersistentStore`) � survives sessions; serialised via `Save`/`Load`
- **`Transient`** (`TransientStore`) � runtime-only; supports `Remove(key)`
- **`Data`** (`DatasetStore`) � read-only; all `*.toml` files in `assets/DATA/` are parsed at layer creation time (not lazily on first `Update()`). See Key Conventions for full API.

## Asset System
- Assets live in `assets/` at the source root.
- On each build, `pack_assets.cmake` packs `assets/` into `cmake-build-debug/assets.pak`.
- **`OVERRIDE/assets`** (source root) is copied via `cmake -E copy_directory_if_different` to `cmake-build-debug/OVERRIDE/assets/` on every build – commit source-tracked overrides here.
- At runtime, `assets.pak` is mounted to VFS root `/` with **prepend=true**, and `OVERRIDE/assets/` (inside the build output dir) is also mounted to `/assets` with **prepend=true** (higher priority, last-mounted wins) – drop files there to override without rebuilding the pak. If a mount path does not exist on disk, `PhysFSLayer` logs a warning and skips it (non-fatal).
- All file I/O (including RmlUi `.rml`/`.rcss`/fonts) goes through `PhysFSLayer` via `PhysFSFileInterface` (implements `Rml::FileInterface`).
- Virtual paths always start with `assets/…` (no leading slash), e.g. `"assets/ui/main.rml"`.

## Launch Configuration

**`LaunchSettings`** is a plain struct (see `LaunchSettings.h`) populated from `UGESettings.toml` at startup by `ConfigLoader` and passed directly to `UGEApplication::Create()`. Fields: `WindowTitle`, `WindowWidth`, `WindowHeight`, `InitScript`, `MountPoints`, `Modules` (platform-independent plugin names loaded via `ModuleLoader`).

The **`UGESettings.toml`** file in the executable directory is the single source of runtime configuration:
```toml
WindowTitle = "Untitled Game Engine"
WindowWidth = 1600
WindowHeight = 1200
InitScript = "assets/lua/initApp.lua"

[Modules]
Load = ["UIWindows", "DemoGame", "TeaPot3D"]

[[MountPoints]]
RealPath = "assets.pak"
VirtualPath = "/"
Prepend = true

[[MountPoints]]
RealPath = "OVERRIDE/assets"
VirtualPath = "/assets"
Prepend = true
```

`main()` calls `LoadLaunchSettingsFromAppDirectory()` (reads `UGESettings.toml`), then `ModuleLoader::LoadModulesRelativeToExecutable(settings.Modules, error)` to load all plugin DLLs (firing `REGISTER_LAYER` / `REGISTER_VIEWMODEL` / `REGISTER_SCENE` static initialisers), then `UGEApplication::Create(Argc, Argv, settings)`. No DLL names are hardcoded in `main.cpp`; add/remove plugins by editing `UGESettings.toml`. `InitScript` defaults to `"assets/lua/initApp.lua"`; set to `""` to skip Lua initialisation. `PhysFSLayer::Create()` mounts every `MountPoints` entry; non-existent paths are skipped with a warning.

## Build & Workflow

**IDE**: CLion on Windows. CMake is managed by CLion � reload the CMake project via **File ? Reload CMake Project** (or the CMake tool window) after editing `CMakeLists.txt` or any `FetchContent` dependency. Do **not** run `cmake -B` manually unless working outside CLion, as CLion manages its own build directory (`cmake-build-debug/`).

```powershell
# Configure (first time or after CMakeLists changes � prefer CLion's CMake reload instead)
cmake -B cmake-build-debug -S .

# Required bootstrap for agent/tool-driven terminal builds: CLion's cached GCC
# expects helper tools like as.exe to be discoverable on PATH.
$env:PATH = 'C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin;' + $env:PATH

# Preferred full build command for agent/tool-driven sessions on this workspace.
# Use the absolute build directory and explicit target/job count format.
cmake --build C:\Users\ahren\CLionProjects\untitled\cmake-build-debug --target all -j 62

# Shorter equivalent when already operating from the project root.
cmake --build cmake-build-debug

# Repack assets only, without recompiling
cmake --build cmake-build-debug --target pack_assets

# Run
./cmake-build-debug/untitled.exe
```

When an AI agent needs to build from a terminal, first prepend `C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin` to `PATH`, then prefer the exact absolute-path build form above instead of composing a relative build command with shell piping. This matches the known-good workflow already verified in this workspace: without the MinGW `bin` directory on `PATH`, CLion's bundled `g++.exe` may fail because helper tools like `as.exe` are not discoverable in agent terminal sessions.

The project builds a **`UGECore` shared library** (`libUGECore.dll`) that is consumed by `untitled.exe`. RmlUi backend sources are compiled into `UGECore` (not directly into the executable):
- `_deps/rmlui-src/Backends/RmlUi_Platform_SDL.cpp`
- `_deps/rmlui-src/Backends/RmlUi_Renderer_SDL.cpp`

The macro `RMLUI_SDL_VERSION_MAJOR=3` is defined on `UGECore` (see `source/UGECore/CMakeLists.txt`). Public headers are under `source/UGECore/Public/`; use `#include <Layers/RmlUILayer.h>` etc. (the `Public/` dir is the include root).

## Coding Conventions

| Category | Convention | Example |
|---|---|---|
| **Public functions** | PascalCase | `LoadDocument()`, `SetBackground()`, `RegisterAll()` |
| **Private functions** | camelCase | `extractDataModelNames()`, `processFragments()`, `lastError()` |
| **Private member variables** | camelCase with `m_` prefix | `m_window`, `m_logFn`, `m_pageViewModels` |
| **Function parameters** | camelCase | `void Log(std::string Msg)`, `bool Mount(const char* RealPath, ...)` |
| **Local variables** | camelCase | `auto Result = ...`, `for (auto& Layer : m_layers)` |
| **Macros** | ALL_CAPS with underscores | `REGISTER_VIEWMODEL(...)`, `RMLUI_SDL_VERSION_MAJOR` |
| **`constexpr` / `const` expressions** | ALL_CAPS with underscores | `RMLUI_SDL_VERSION_MAJOR` |

> **Notes:**
> - Public data members (e.g. `onLog` in `LoggingLayer`) are excluded from the PascalCase rule � they follow camelCase.
> - Override methods that must match a third-party virtual interface (e.g. `Rml::FileInterface`: `Open`, `Close`, `Read`, `Seek`, `Tell`, `Length`) keep their inherited names.
> - Private helper methods inside `.cpp` files (lambdas, free functions with internal linkage) follow the camelCase convention.

## Key Conventions

### UGECore — Engine Infrastructure

- **Factory pattern with `std::expected`**: subsystem constructors are private; use `::Create()` and propagate errors with `return std::unexpected(...)`. All layers return `std::expected<std::unique_ptr<ConcreteLayer>, std::string>` except `SDLLayer::Create()` which returns `std::expected<std::unique_ptr<AppLayer>, std::string>`. `RmlUILayer::Create()` takes no SDL parameters and fetches `SDLLayer` via ServiceLocator. Both `SDLLayer` and `RmlUILayer` wire their log sinks inside `Create()` by fetching `LoggingLayer` from `ServiceLocator`.
- **`pushLayer<T>()`**: the single idiom to add a layer – takes ownership, calls `Layer->RegisterWithServiceLocator()` (virtual hook), and returns a raw observer pointer. Registration is **fully delegated** to `RegisterWithServiceLocator()`; `pushLayer` itself does not call `ServiceLocator::Provide`. Every concrete layer's `RegisterWithServiceLocator()` calls `ServiceLocator::Provide(this)` to register under its concrete type.
- **ServiceLocator is non-owning**: lifetime is managed by `UGEApplication`'s `m_layers` vector. Never store raw pointers longer than the owning layer lives. Additional methods: `Has<T>()` (returns bool), `Provide<T>(T*)` (can register multiple instances; `Get<T>()` returns the most recently provided), `Remove<T>()` (single-type deregister), `Clear()` (deregister all instances).
- **Static registry macro pattern (`REGISTER_LAYER`, `REGISTER_SCENE`, `REGISTER_VIEWMODEL`)**: place registration macros in a `public:` section of the class **header** so static-inline registration runs at DLL load. Factories are resolved through `LayerRegistry` / `SceneRegistry` / `ViewModelRegistry`; avoid manual wiring in `main()` or `UGEApplication::Create()`. Example references: `source/GamePlugins/TeaPot3D/Render3DLayer/Render3DLayer.h`, `source/GamePlugins/DemoGame/Platformer/PlatformerScene.h`, `source/GamePlugins/DemoGame/LuaConsoleViewModel.h`.
- **Log routing**: `LoggingLayer` owns the log buffer. `SDLLayer` and `RmlUILayer` wire their log sinks automatically in `Create()` via `ServiceLocator::TryGet<LoggingLayer>()`. Log wiring to the UI is handled automatically inside `LuaConsoleViewModel::RegisterWith()` — it fetches `ServiceLocator::Get<LoggingLayer>()` and calls `BindLogData(Log.Lines())`, assigning the result to `Log.onLog`.
- **`AppLayer::Log()`**: the protected `Log(std::string Msg)` method on `AppLayer` routes a message to `LoggingLayer` via `ServiceLocator`; it no-ops gracefully if `LoggingLayer` is not yet registered. Use it inside any layer's implementation instead of accessing `ServiceLocator` directly for logging.

### UGECore — Asset & Data Layers

- **`PhysFSLayer` helpers**: beyond `Mount()` / `Unmount()`, use `ReadFile(path)` ? `std::vector<std::byte>`, `OpenAsIOStream(path)` ? `SDL_IOStream*`, `Exists(path)` ? `bool`, `ListFiles(dir)` ? `std::vector<std::string>`, `LogFiles(dir = "")` ? logs all VFS files to `LoggingLayer` (called in `UGEApplication::Create()` after mounting). Free helper `LoadTextureFromPhysFS(VirtualPath)` ? `std::expected<UniqueTexture, std::string>` (declared in `Layers/SDLLayer.h`, resolves renderer and PhysFS via ServiceLocator) loads an image from the VFS into an SDL texture.
- **`UGEDataLayer` state bus and dataset store**: always available (pushed by default in `UGEApplication::Create()`). Three public store objects accessed directly on the layer:
  - **`layer.State`** (`PersistentStore`) – `Set(key, val)`, `Get(key)` ? `const Value*`, `Has(key)`, `Subscribe(key, cb)`, `Unsubscribe(token)`, `Save(FilePath)`, `Load(FilePath)` (real OS paths). Typed helpers return `std::optional`: use `std::get<int/float/std::string>(*layer.State.Get(key))` or bind reactively.
  - **`layer.Transient`** (`TransientStore`) – same as State plus `Remove(key)`; never serialised.
  - **`layer.Data`** (`DatasetStore`) – read-only TOML datasets. `HasDataset(name)`, `GetInt(dataset, path)`, `GetFloat(dataset, path)`, `GetString(dataset, path)` return `std::optional`; `GetRows(dataset, arrayKey)` ? `const DataTable*` (each `DataRow` is `unordered_map<string, AppStateValue>`); `LoadFile(vfsPath)` loads a single TOML file. All `*.toml` files under `assets/DATA/` are loaded at layer creation time.
  Keys use dot-notation by convention (`"ui.console.visible"`); storage is flat. Use the `APPSTATE_BIND("key", defaultValue, callback)` macro to inject a default into `layer.State`, subscribe, and obtain an `AppStateBinding` RAII handle; use `APPSTATE_BIND_TRANSIENT` for `layer.Transient`; use `APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT(Binding_, Key_, Member_)` for the common pattern of binding a transient float key directly to a `float` member variable (subscribes, then seeds `Member_` from the current stored value immediately). Always store `AppStateBinding` as a member variable – it auto-unsubscribes on destruction; call `binding.Release()` to unsubscribe early, or test liveness with `binding.IsActive()`. `binding.SetValue(AppStateValue{...})` writes a new value to the bound key and fires all subscribers; `binding.GetValue()` returns `const AppStateValue*` to read the current stored value (returns `nullptr` if the binding is inactive or the key is absent). Prefix subscriptions (`SubscribePrefix("ui.", cb)`) fire for all keys starting with the prefix across both `State` and `Transient`.
  **Dataset store example**: `layer.Data.GetFloat("platformerData", "physics.gravity")`, `layer.Data.GetString("platformerData", "assets.background")`, `layer.Data.GetRows("platformerMap", "tiles")`.

### UGECore — Lua Integration

- **`IScriptableObject` mixin**: any class that should expose itself to Lua inherits `IScriptableObject` (include `<IScriptableObject.h>`) and implements `RegisterObject(sol::state& Lua)`. Register the C++ API using `Lua.new_usertype<T>(...)` and expose the instance via `Lua["Name"] = this`. For objects that are created and destroyed at runtime (e.g. scene-lifetime services), also override `UnregisterObject(sol::state& Lua)` to nil out the globals: `Lua["Name"] = sol::nil`. Examples: `SDLLayer`, `RmlUILayer`. (`ScriptableObject.h` is a backward-compat shim; prefer `<IScriptableObject.h>` in new code.)
- **`LuaLayer` script registration pattern**: `LuaLayer` owns `IScriptableObject` registration (previously `ScriptAPILayer`, which no longer exists). Two registration paths:
  - **Startup**: `UGEApplication::Create()` iterates all pushed layers via `dynamic_cast<IScriptableObject*>` and calls `luaLayer->Register(Scriptable)` on each. No manual wiring needed for layers pushed before the init script runs.
  - **Runtime (dynamic)**: call `ServiceLocator::Get<LuaLayer>().Register(obj)` to immediately invoke `RegisterObject()` and begin tracking; call `Unregister(obj)` to invoke `UnregisterObject()` and stop tracking. Use this for `IScriptableObject`s whose lifetime is shorter than the application (e.g. scene objects). `SDLLayer` calls these automatically when loading/unloading `IScriptableObject` scenes.
- **`LuaLayer` tickable functions**: `AddTickFunction(sol::protected_function Fn, const char* Name)` ? `uint32_t` registers a Lua function to be called every `Update()` (returns a tick ID); `RemoveTick(uint32_t Id)` unregisters by ID. Duplicate name registrations are silently ignored (returns existing ID). Exposed to Lua via the `UGE` table – see Lua API surfaces below.
- **`LuaLayer` `print()` redirection**: `LuaLayer` overrides Lua's `print()` to forward output to `LoggingLayer` with a `[Lua] ` prefix. Errors from `Execute()` / `ExecuteFile()` are logged and returned as `std::unexpected` so callers can decide whether to abort.

### UGECore — Shared Components

- **`BoxCollision`**: AABB helper at `<GameClasses/BoxCollision.h>` used for tile/player collision. `Intersects(Other)` for overlap detection; `OverlapX(Other)` / `OverlapY(Other)` return the MTV depth for axis-aligned push-back. Edge accessors: `Left()`, `Top()`, `Right()`, `Bottom()`, `CenterX()`, `CenterY()`. Query registration state: `IsRegistered()`, `RegistryId()`. **`Enabled` flag**: when `false`, all intersection tests return `false` – toggle freely at runtime (e.g. to deactivate a collected coin). **`UserData` convention**: every instantiator of a `BoxCollision` member **must** set `Bounds.UserData = this` (or equivalent) immediately after construction, so that `BoxCollisionGrid::Intersects()` results can be cast back to the owning type at the call site (e.g. `static_cast<Tile*>(hit->UserData)`). `UserData` is intentionally **not** propagated by copy or move – it is the identity of the owning object and must always be stamped by that object. Classes that call `UpdateCollision()` should set `UserData` inside that helper (see `Tile::UpdateCollision()`). **Auto-registration**: the value constructor `BoxCollision(X, Y, W, H [, Label])` registers with `BoxCollisionRegistry::Active()` when one is set (i.e. after `SDLLayer::InitCollisionHooks()` – called automatically in `SDLLayer::Create()`). Copy construction does **not** register (copies are for temporary collision math only). Move construction transfers the existing registration to the new address. Destruction automatically deregisters. Default construction does not register – use default + assignment for temporary boxes.
- **`BoxCollisionRegistry`**: Non-owning debug overlay registry at `<GameClasses/BoxCollisionRegistry.h>`. Owned by `SDLLayer` (as `m_collisionRegistry`). `SDLLayer::ShowCollisionBoxes(bool)` / `IsShowingCollisionBoxes()` toggle the overlay rendered in `Tick()`. Explicit opt-in (for boxes that should not auto-register): `SDLLayer::RegisterCollision(Box*, Label)` → `CollisionHandle` (RAII, deregisters on destruction). Access the registry directly via `SDLLayer::CollisionRegistry()`. Lua: `SDL.ShowCollision()` toggles the overlay and logs the state.
- **`BoxCollisionGrid`**: Broad-phase uniform spatial grid for AABB queries at `<GameClasses/BoxCollisionGrid.h>`. STATIC after construction — build once with a `std::vector<BoxCollision*>` and a coarseness value (cell side-length in reference-space units). Query: `grid.Intersects(&playerBox, hits)` fills `hits` with candidate overlapping boxes; do narrow-phase against results. The grid stores non-owning pointers — source vector and `BoxCollision` objects must outlive the grid. Used by `TileSet::CollisionGrid` inside `TileWorld`.
- **Sprite system**: Three reusable rendering helpers live under `source/UGECore/Public/Sprite/` (include root `Sprite/`):
  - **`Sprite`** (`<Sprite/Sprite.h>`) – owns a single SDL texture loaded from the VFS. `Sprite::Load(Renderer, Physfs, "assets/…", PaddingH, PaddingV)` ? `std::expected<Sprite, std::string>`. `Draw(Renderer, SDL_FRect, FlipMode)`. Optional `PaddingH`/`PaddingV` shrink the destination rect at draw time while sampling the full source texture. Query dimensions via `Width()`, `Height()`, `RenderedW()`, `RenderedH()`.
  - **`SpriteSheet`** (`<Sprite/SpriteSheet.h>`) – owns a single texture divided into a uniform grid of frames (left-to-right, top-to-bottom, 0-indexed). `SpriteSheet::Load(Renderer, Physfs, "assets/…", FrameW, FrameH, PaddingH, PaddingV)`. `Draw(Renderer, FrameIndex, Dest, FlipMode)`. Query via `FrameCount()`, `FrameW()`, `FrameH()`, `RenderedW()`, `RenderedH()`.
  - **`AnimatedSprite`** (`<Sprite/AnimatedSprite.h>`) – stateful animation controller that holds a **non-owning** `const SpriteSheet*` plus a frame sub-range `[StartFrame, StartFrame+FrameCount)`. Call `Update(float DeltaSeconds)` once per frame in `Scene::Update()`; call `Draw(Renderer, Dest, FlipMode)` in `Scene::Tick()`. `Reset()` rewinds to the first frame. The owning scene must ensure the `SpriteSheet` outlives the `AnimatedSprite`.
### Game Plugins — UI & ViewModels

- **Window system (desktop UI windows)**: A reusable framework implemented in `UIWindows.dll` for building draggable, resizable UI windows. The architecture separates **runtime state** (position, visibility, z-order) from **widget markup** – state changes are stored transient-only and updates are driven reactively via RmlUi data-binding. **Core class**:
  - **`WindowViewModel`** (`source/GamePlugins/UIWindows/WindowViewModel.h`; include root `source/GamePlugins/UIWindows/`) – base class for all window data-models. Derives from `ViewModel`. Manages position, visibility, and z-order, all stored under `uiRuntime.<activeDoc>.<windowTag>.*` transient keys. Public API: `Show()` / `Hide()` / `IsVisible()`, `SetPosition(Left, Top)` / `SetLeft()` / `SetTop()`, `SetZOrder()` / `RaiseWindowToFront()`, `LoadRuntimeState()` / `SaveRuntimeState()`, `ClampToViewport()`. Transient-backed bindings are created during initialization for `posX`, `posY`, `z`, and `visible`; those callbacks update local members and call `OnRuntimeStateChanged()`. Shared protected helpers for derived windows:
    - `BindCommonWindowVars(Ctor)` – binds `panel_left` / `panel_top` / `panel_z` / `panel_visible`
    - `BindCloseEvent(Ctor [, EventName])` – binds a close callback that calls `Hide()`
    - `SyncWindowModel(Model [, ExtraDirtyVars])` – stores model handle and dirties common + extra variables
    - `HandleWindowDragEvent(Event, DragHandleId)` – reusable drag-to-move behavior
    - `setTransient(Suffix, Value)` – write runtime keys directly
    Include as `#include <WindowViewModel.h>` (requires linking against `UIWindows`).
  - **Runtime state contract**: `RmlUILayer` writes `uiRuntime.activeDoc` to the current page slug (normalized from file path, e.g., `"platformer"` from `assets/ui/platformer.rml`). `WindowViewModel` loads/stores per-window state under:
    - `uiRuntime.<activeDoc>.<windowTag>.posX` / `.posY` – position in reference pixels
    - `uiRuntime.<activeDoc>.<windowTag>.z` – z-order (stacking) index
    - `uiRuntime.<activeDoc>.<windowTag>.visible` – visibility flag (`1` = visible, `0` = hidden)
    - `uiRuntime.<activeDoc>.currentWinZMax` – shared per-page z-order counter
  - **Derived constructor pattern**: keep constructors minimal and pass a static tag to base. Apply optional window-specific defaults via transient writes only.
    ```cpp
    MyWindowViewModel::MyWindowViewModel()
        : WindowViewModel("my-window")
    {
        // Optional default runtime values:
        // setTransient(".visible", AppStateValue{1});
    }
    ```
  - **Derived `RegisterWith()` pattern**: bind common panel fields through base helpers, bind window-specific fields locally, then synchronize via `SyncWindowModel`.
    ```cpp
    void MyWindowViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
    {
        SetContext(Context);
        auto Ctor = Context->CreateDataModel(ModelName);

        BindCommonWindowVars(Ctor);
        Ctor.Bind("my_value", &m_myValue);
        BindCloseEvent(Ctor); // optional if markup uses onClose

        auto Model = Ctor.GetModelHandle();
        SyncWindowModel(Model, {"my_value"});
    }
    ```
  - **Example reference**: `source/GamePlugins/DemoGame/LuaConsoleViewModel.h` shows a complete derived implementation with extra data bindings, submit/close callbacks, and drag handling.
  - **Constructor restore pattern**: base constructor calls `LoadRuntimeState()`, seeds transient via `SaveRuntimeState()`, then establishes bindings. This restores position/z/visibility and keeps runtime state synchronized for all derived windows.

- **UI documents**: `.rml` / `.rcss` files live under `assets/ui/`. `RmlUILayer::LoadDocument()` queues a path that is processed on the next `Update()` (deferred); any previously loaded page and its ViewModels are torn down first. Only **one page** may be live at a time. The loader automatically scans the file for `data-model="..."` attributes, instantiates the registered `ViewModel` via `ViewModelRegistry`, and calls `RegisterWith(context, modelName)` **before** RmlUi parses the markup. No manual `RegisterWith()` call is needed in `UGEApplication.cpp`.
- **data-fragment support**: Elements with `data-fragment="assets/ui/frag_foo.rml"` are found after load, the fragment file is read from PhysFS, its `data-model` attributes are wired to ViewModels, and the markup is injected via `SetInnerRML`. Use this to split large documents into reusable fragments (e.g. `assets/ui/frag_luaConsole.rml`).
- **ViewModel lifecycle pattern**: subclass `ViewModel`, implement `RegisterWith(Rml::Context*, const char*)`, and register in-header with `REGISTER_VIEWMODEL(...)`. `RmlUILayer` owns page + fragment view-models in one container and tears them down on page unload/reload. Register with `ServiceLocator` only when cross-system lookup is required.

### Game Plugins — Scenes

- **Scene lifecycle pattern**: `SDLLayer` owns one active `SceneObject` at a time. Derive from `SceneObject`, implement `Update()` + `Tick(float deltaTime)` (+ optional `HandleEvent`), and register in-header with `REGISTER_SCENE("scene-name", Type)`. Load via `SDL.LoadScene("scene-name")` or `SDLLayer::LoadScene()`. Previous scene teardown is automatic; if the scene implements `IScriptableObject`, Lua registration/unregistration is automatic too. Example reference: `source/GamePlugins/DemoGame/Platformer/PlatformerScene.h`.

### Game Plugins — Input & Events

- **Input event propagation**: `SDLLayer::Update()` polls SDL events and forwards each to `Application::dispatchEvent()` via the registered event handler. `dispatchEvent` walks `m_layers` in **reverse** push order, `dynamic_cast`s each layer to `IEventHandler*`, and calls `HandleEvent(SDL_Event&)`. Returning `true` consumes the event and stops propagation; returning `false` continues. `RmlUILayer::HandleEvent()` first dispatches to its ViewModels in reverse order (calling `ViewModel::HandleEvent()`), then forwards unconsumed events to `RmlSDL::InputEventHandler` and returns `!result` – `true` (stop propagation) when RmlUi consumed the event (e.g. a focused `<input>` captured a keypress), `false` (continue propagating) when RmlUi left the event unhandled. To opt a new layer in, inherit `IEventHandler` alongside `AppLayer`; to opt a ViewModel in, override `ViewModel::HandleEvent()` (default returns `false`).
- **Input ownership pattern**: keep domain-specific input handling with the owning scene or view-model (`HandleEvent`), while `LuaLayer::HandleEvent()` remains non-consuming.
- **`GameObjectBase` accessors**: both `Scene` and `ViewModel` inherit `GameObjectBase` (`<GameClasses/GameObjectBase.h>`). Inside any derived class use the protected helpers — `GetLoggingLayer()`, `GetPhysFSLayer()`, `GetDataLayer()`, `GetLuaLayer()`, `GetSDLLayer()`, `GetUILayer()` — instead of calling `ServiceLocator::TryGet<T>()` directly. All return raw non-owning pointers (null if the layer has not yet been registered); guard with a null-check when availability is uncertain. `Render3DLayer` is not a UGECore type and has no accessor here; fetch it via `ServiceLocator::TryGet<Render3DLayer>()` from code that links against `TeaPot3D`.

### Lua API Surfaces

- **Lua API surfaces**: `SDLLayer` exposes `SDL.SetBackground(virtualPath)`, `SDL.LoadScene(sceneName)`, and `SDL.ShowCollision()` (toggle collision debug overlay) to Lua. `RmlUILayer` exposes `UI.LoadFont(virtualPath)` and `UI.LoadDocument(virtualPath)`. `Render3DLayer` (TeaPot3D plugin) exposes the `Render3D` table (see `Render3DLayer` entry in Key Files). `UGEDataLayer` exposes the `Data` table: `Data.SetTransient(key, value)` (type-safe set; coerces to existing backing type or infers from Lua value for new keys), `Data.GetTransient(key)` ? Lua value or nil, `Data.ShowTransient()` (logs all transient store entries). `LuaLayer` exposes the `UGE` table: `UGE.AddTickFunction(fn [, name])` ? id, `UGE.RemoveTick(id)`, `UGE.RunScript(virtualPath)`, `UGE.ShowTicking()` (logs active tick entries), `UGE.GetTickId(name)` ? id or 0. All are registered by `LuaLayer` at startup (via the startup registration pass in `UGEApplication::Create()`).

### Build & Compilation

- **`RMLUI_STATIC_LIB`**: UGECore defines this as a `PUBLIC` compile definition. Any target that includes UGECore's public headers (e.g. `DemoGame.dll`) automatically receives it – do not redefine it manually.
- **MinGW symbol export**: UGECore is built with `-Wl,-u,_ZN3Rml10FamilyBase8GetNewIdEv` so that `Rml::FamilyBase::GetNewId` (in `librmlui.a:Traits.cpp.obj`) is forced into the UGECore link and therefore exported via MinGW's default export-all behaviour. Without this, templates in RmlUi headers (e.g. `Rml::Family<T>::Id()` ? `Rml::FamilyBase::GetNewId()`) produce `undefined reference` linker errors in DemoGame.dll even though the symbol physically lives inside UGECore.dll. If a new `Rml::Family<T>::Id()` instantiation causes a similar error for a different symbol in `librmlui.a`, add another `-Wl,-u,<mangled_name>` to `target_link_options(UGECore ...)` in `source/UGECore/CMakeLists.txt`.

## Key Files

### Application

| File | Purpose |
|---|---|
| `source/Application/main.cpp` | Entry point; reads `UGESettings.toml` via `LoadLaunchSettingsFromAppDirectory()`, loads plugin DLLs listed under `[Modules] Load` via `ModuleLoader::LoadModulesRelativeToExecutable()`, then calls `UGEApplication::Create(Argc, Argv, settings)`. No DLL names are hardcoded here. |
| `source/Application/ConfigLoader.h` / `.cpp` | `LoadLaunchSettingsFromAppDirectory()` — reads `UGESettings.toml` from the executable directory and returns a populated `LaunchSettings`. |
| `source/Application/CMakeLists.txt` | Executable entry-point CMake; links `PRIVATE UGECore`; no game or plugin logic here. |
| `UGESettings.toml` | Per-deployment runtime configuration (window title/size, init script, VFS mount points, plugin module names). Copied to `cmake-build-debug/` by the build. Edit to add/remove plugins without recompiling. |
| `assets/lua/initApp.lua` | Startup Lua script executed at the end of `UGEApplication::Create()`; loads fonts and the initial scene. |

### UGECore — Application & Layer Infrastructure

| File | Purpose |
|---|---|
| `source/UGECore/Public/UGEApplication.h` | `UGEApplication` class; `LaunchSettings Settings` static field; `Create(...)` factory; `Run()` frame loop; owns all layers via `m_layers`. |
| `source/UGECore/Private/UGEApplication.cpp` | Startup and runtime implementation — layers pushed via `LayerRegistry`, `InitScript` executed, Lua `IScriptableObject` registration pass. |
| `source/UGECore/Public/LaunchSettings.h` | Plain struct: `WindowTitle`, `WindowWidth`, `WindowHeight`, `ExecutablePath`, `InitScript`, `MountPoints`, `Modules`. |
| `source/UGECore/Public/Layers/AppLayer.h` | Base class for all layers (`Update()` / `Tick(float deltaTime)`); protected `Log(Msg)` helper. |
| `source/UGECore/Public/ServiceLocator.h` | Type-safe service registry. `Provide<T>()`, `Get<T>()`, `TryGet<T>()`, `Has<T>()`, `Remove<T>()`, `Clear()`. |
| `source/UGECore/Public/LayerRegistry.h` | Singleton layer factory registry + `REGISTER_LAYER(name, loadOrder, Type)` macro. |
| `source/UGECore/Public/SceneRegistry.h` | Singleton scene factory registry + `REGISTER_SCENE(name, Type)` macro. |
| `source/UGECore/Public/ViewModelRegistry.h` | Singleton ViewModel factory registry + `REGISTER_VIEWMODEL(name, Type)` macro. |
| `source/UGECore/CMakeLists.txt` | UGECore shared library build; defines `RMLUI_SDL_VERSION_MAJOR=3` and `RMLUI_STATIC_LIB` as PUBLIC. |

### UGECore — Core Layers

| File | Purpose |
|---|---|
| `source/UGECore/Public/Layers/LoggingLayer.h` | Application-wide log buffer; `MakeSink()` / `onLog` / `BindLogData()` / `Clear()`. |
| `source/UGECore/Public/Layers/PhysFSLayer.h` | Virtual filesystem wrapper; `Mount`, `ReadFile`, `Exists`, `ListFiles`, `OpenAsIOStream`. |
| `source/UGECore/Public/Layers/UGEDataLayer.h` | Key-value state bus + TOML dataset store; `State` / `Transient` / `Data` stores; `APPSTATE_BIND` macros; `AppStateBinding` RAII handle. |
| `source/UGECore/Public/Layers/SDLLayer.h` | SDL window, renderer, background, event loop; scene management (`LoadScene`, `ActiveScene`); collision overlay; `LoadTextureFromPhysFS()` free helper. |
| `source/UGECore/Public/Layers/LuaLayer.h` | Lua 5.4 VM (sol2); `Execute()` / `ExecuteFile()`; `Register` / `Unregister` for `IScriptableObject`; `AddTickFunction` / `RemoveTick`. |
| `source/UGECore/Public/Layers/RmlUILayer.h` | RmlUi lifecycle, document load, per-frame render; owns all ViewModel instances. |
| `source/UGECore/Public/PhysFSFileInterface.h` | Bridges PhysFS → RmlUi file I/O. |

### UGECore — Game Classes & Shared Components

| File | Purpose |
|---|---|
| `source/UGECore/Public/GameClasses/AppLayer.h` | Base layer; `Update()` / `Tick(deltaTime)` / protected `Log()`. |
| `source/UGECore/Public/GameClasses/SceneObject.h` | Base class for scenes (`Update`, `Tick`, optional `HandleEvent`); use with `SceneRegistry`. |
| `source/UGECore/Public/GameClasses/ViewModel.h` | Base class for RmlUi data-model ViewModels (`RegisterWith`); use with `ViewModelRegistry`. |
| `source/UGECore/Public/GameClasses/GameObjectBase.h` | Common base for `Scene` and `ViewModel`; protected layer accessors (`GetSDLLayer()`, `GetDataLayer()`, etc.). |
| `source/GamePlugins/UIWindows/WindowViewModel.h` | Base class for window-style ViewModels; position / visibility / z-order via transient AppState; helpers `BindCommonWindowVars`, `BindCloseEvent`, `SyncWindowModel`, `HandleWindowDragEvent`. Include as `#include <WindowViewModel.h>` (link against `UIWindows`). |
| `source/UGECore/Public/GameClasses/BoxCollision.h` | AABB collision; `Intersects()`, `OverlapX/Y()`, edge accessors; auto-registers on value construction. |
| `source/UGECore/Public/GameClasses/BoxCollisionRegistry.h` | Non-owning debug overlay registry owned by `SDLLayer`; `Register(Box*, Label)` → `CollisionHandle` RAII. |
| `source/UGECore/Public/GameClasses/BoxCollisionGrid.h` | Broad-phase spatial grid; build once, query with `Intersects(Target, OutHits)`. |
| `source/UGECore/Public/Sprite/Sprite.h` | Single-texture sprite; `Sprite::Load(...)` → `std::expected`; `Draw(Renderer, Dest, FlipMode)`. |
| `source/UGECore/Public/Sprite/SpriteSheet.h` | Uniform-grid sprite sheet; `Draw(Renderer, FrameIndex, Dest, FlipMode)`. |
| `source/UGECore/Public/Sprite/AnimatedSprite.h` | Stateful animation controller (non-owning `SpriteSheet*`); `Update(DeltaSeconds)` / `Draw(...)` / `Reset()`. |
| `source/UGECore/Public/IScriptableObject.h` | Mixin; implement `RegisterObject(sol::state&)` to expose a class to Lua; override `UnregisterObject` for scene-scoped objects. |
| `source/UGECore/Public/IEventHandler.h` | Opt-in mixin for event-handling layers and ViewModels; `HandleEvent()` returns bool. |
| `source/UGECore/Public/ScriptableObject.h` | Backward-compat shim — `using ScriptableObject = IScriptableObject`. Prefer `<IScriptableObject.h>` in new code. |
| `source/GamePlugins/TeaPot3D/Render3DLayer/Camera3D.h` | `Camera3D` struct: `Eye`, `Target`, `Up`, `FovDeg`, `NearZ`, `FarZ`. |
| `source/GamePlugins/TeaPot3D/Render3DLayer/Light3D.h` | `Light3D` struct: `Direction`, `Color`, `Ambient` (glm::vec3, [0,1] range). |
| `source/GamePlugins/TeaPot3D/Render3DLayer/Mesh3D.h` | `Mesh3D` owns `std::vector<Triangle3D>`; `Triangle3D` holds vertices, normals, UVs + `Material3D`. |
````
