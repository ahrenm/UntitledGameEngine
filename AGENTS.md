# AGENTS.md � Codebase Guide for AI Coding Agents

## Project Overview
**Untitled Game Engine** � C++23 desktop application using **SDL3 + RmlUi** for rendering and UI, with **PhysicsFS** as a virtual filesystem for asset loading. All dependencies are fetched at configure time via CMake `FetchContent` (SDL3, SDL3_image, RmlUi, PhysicsFS, FreeType, Lua 5.4, sol2, **glm**, **tinyobjloader**).

## Architecture

```
main.cpp
  +-- [Plugin Loading]           ? Explicitly load UIWindows.dll and DemoGame.dll before Create()
  +-- UGEApplication::Create()   ? factory returning std::expected; owns all subsystems; reads from UGEApplication::Settings
        +-- [Core Layers (Load Order 1–7)]
        +-- LoggingLayer (1)      ? owns the log-line buffer; must be pushed first
        +-- PhysFSLayer (2)       ? virtual filesystem (assets.pak + OVERRIDE/assets/); reads ExecutablePath + MountPoints from UGEApplication::Settings and mounts during Create(); non-existent paths are skipped with a warning (not fatal)
        +-- UDEDataLayer (3)      ? key-value state bus; pushed by default after PhysFSLayer
        +-- SDLLayer (4)          ? window, renderer, background texture, event loop; reads window config from UGEApplication::Settings; InitCollisionHooks() called automatically in Create()
          +-- Scene              ? The active scene; only one at a time; owned by SDLLayer; scene teardown is automatic on load
        +-- Render3DLayer (4.5)   ? CPU-rasterised 3D viewport; loads immediately after SDLLayer; no-op until Activate(); fetches SDL_Renderer via ServiceLocator
        +-- LuaLayer (5)          ? sol::state VM + ScriptableObject registry; implements IEventHandler (currently non-consuming input handler)
        +-- RmlUILayer (6)        ? RmlUi context, document loading, per-frame render; Create() fetches SDL window/renderer from SDLLayer via ServiceLocator
          +-- Document           ? The active UI document; only one at a time; document teardown is automatic on load
            +-- Fragment(n)      ? Optional fragments injected via data-fragment; owned by RmlUILayer; instantiated automatically based on data-fragment attributes in the markup
          +-- ViewModel(n)       ? Owned by RmlUILayer; instantiated automatically based on data-model attributes in the markup; e.g. LuaConsoleViewModel for the console panel
        +-- [Plugin Layers (Load Order 10.0+)] ? Registered via LayerRegistry and REGISTER_LAYER; push order determined by LayerRegistry::Names() sorted by LoadOrder
```

`LuaConsoleViewModel` is **not** pushed as a layer. It lives in `DemoGame.dll` and self-registers via `REGISTER_VIEWMODEL("lua-console", LuaConsoleViewModel)` (static initialiser) and is instantiated automatically by `RmlUILayer::LoadDocument()` when `data-model="lua-console"` is found in the markup. It calls `ServiceLocator::Provide(this)` from within `RegisterWith()` so it is thereafter accessible via `ServiceLocator::Get<LuaConsoleViewModel>()`. Include `<LuaConsoleViewModel.h>` (Game's `source/GamePlugins/DemoGame/` is the include root). Derives `WindowViewModel` with tag `"lua-console"`, inheriting position/visibility/z-order management; visibility state stored under `uiRuntime.<activeDoc>.lua-console.visible` transient key. Public API: `Show()` / `Hide()` / `IsVisible()` (inherited), `SetPosition()` / `RaiseWindowToFront()` (inherited), `InputText()` (console-specific); data-model variables: `panel_left`, `panel_top`, `panel_z`, `panel_visible` (all inherited window state), `log_lines`, `input_text` (console-specific); event callbacks: `onSubmit`, `onClose` (bound via base helper `BindCloseEvent`); event handling includes backtick visibility toggle, Enter submit (when input has focus), and titlebar drag-to-move (via base helper `HandleWindowDragEvent`).

`PlatformerHudViewModel` similarly lives in `DemoGame.dll`, self-registers via `REGISTER_VIEWMODEL("platformer-hud", PlatformerHudViewModel)`, and is instantiated when `data-model="platformer-hud"` is found (currently in `assets/ui/platformer.rml`). Include `<PlatformerHudViewModel.h>` (`source/GamePlugins/DemoGame/Platformer/` is the include root). Public API: `SetScore(int)`, `onNext` (assign a `std::function<void()>` callback for the Next button); data-model variable: `score`. The score is also mirrored through the transient `UDEDataLayer` key `"Platformer2d.Score"` (defined as `PlatformerHudViewModel::SCORE_KEY`), so `PlatformerScene` can update it via `layer.Transient.Set(SCORE_KEY, score)` and the HUD updates automatically via `AppStateBinding`.

All subsystems derive from **`AppLayer`** (`Layers/AppLayer.h`) and are stored in `UGEApplication::m_layers` via `pushLayer<T>()`, which takes ownership, calls `RegisterWithServiceLocator()` (virtual hook on `AppLayer`) where each concrete layer registers itself with `ServiceLocator::Provide(this)`. Use `ServiceLocator::Get<T>()` to access services anywhere; use `TryGet<T>()` when the service may not be present.

**Frame loop** (`UGEApplication::Run()`): runs a **30 FPS-capped loop** using `SDL_GetTicksNS()` / `SDL_DelayNS()`. Each tick: `Update()` is called on each layer in push order (input ? logic), then `m_sdlLayer->BeginFrame()` clears the renderer, `Tick()` is called on each layer in **push order** (SDLLayer blits background + scene first, RmlUILayer composites UI on top, `m_sdlLayer->EndFrame()` presents last). If execution falls more than one full frame behind, `NextFrame` is reset to avoid catch-up bursts.

`UDEDataLayer` is a central key-value state bus and TOML dataset store pushed **by default** in `UGEApplication::Create()` (immediately after `PhysFSLayer`, before `LuaLayer`). It exposes three public store objects:
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

**`LaunchSettings`** is a plain struct that holds startup configuration (window title, width, height, executable path, init script path, and VFS mount points) populated before `UGEApplication::Create()` is called. Configure in `main()` before calling `Create()`:
```cpp
UGEApplication::Settings.WindowTitle = "My Game";
UGEApplication::Settings.WindowWidth = 1600;
UGEApplication::Settings.WindowHeight = 1200;
UGEApplication::Settings.ExecutablePath = argv[0];  // Required by PhysFSLayer
UGEApplication::Settings.InitScript = "assets/lua/initApp.lua"; // Set "" to skip
UGEApplication::Settings.MountPoints = {
    {"assets.pak", "/", true},
    {"OVERRIDE/assets", "/assets", true},
};
auto app = UGEApplication::Create(argc, argv);
```
All values have sensible defaults (see `LaunchSettings.h`). If `MountPoints` is empty, `UGEApplication::Create()` seeds the defaults shown above. `PhysFSLayer::Create()` mounts every configured entry; paths that do not exist are skipped with a warning. `ExecutablePath` is populated from `argv[0]` automatically if not pre-set. `InitScript` defaults to `"assets/lua/initApp.lua"`; set to `""` to skip Lua initialisation entirely.

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
- **Factory pattern with `std::expected`**: subsystem constructors are private; use `::Create()` and propagate errors with `return std::unexpected(...)`. All layers return `std::expected<std::unique_ptr<ConcreteLayer>, std::string>` except `SDLLayer::Create()` which returns `std::expected<std::unique_ptr<AppLayer>, std::string>`. `RmlUILayer::Create()` takes no SDL parameters and fetches `SDLLayer` via ServiceLocator. Both `SDLLayer` and `RmlUILayer` wire their log sinks inside `Create()` by fetching `LoggingLayer` from `ServiceLocator`.
- **`pushLayer<T>()`**: the single idiom to add a layer – takes ownership, calls `Layer->RegisterWithServiceLocator()` (virtual hook), and returns a raw observer pointer. Registration is **fully delegated** to `RegisterWithServiceLocator()`; `pushLayer` itself does not call `ServiceLocator::Provide`. Every concrete layer's `RegisterWithServiceLocator()` calls `ServiceLocator::Provide(this)` to register under its concrete type.
- **ServiceLocator is non-owning**: lifetime is managed by `UGEApplication`'s `m_layers` vector. Never store raw pointers longer than the owning layer lives. Additional methods: `Has<T>()` (returns bool), `Provide<T>(T*)` (can register multiple instances; `Get<T>()` returns the most recently provided), `Remove<T>()` (single-type deregister), `Clear()` (deregister all instances).
- **LayerRegistry and REGISTER_LAYER**: all core and plugin layers self-register via the `REGISTER_LAYER("name", loadOrder, Type)` macro placed in a `public:` section of the class body header. The macro injects a static inline member whose constructor fires at DLL load time, registering the layer with `LayerRegistry::Instance()`. `UGEApplication::Create()` loops over `LayerRegistry::Instance().Names()` (sorted by load order) to push every registered layer automatically — no explicit per-layer creation code needed. Core layer load orders: `LoggingLayer` (1.0), `PhysFSLayer` (2.0), `UDEDataLayer` (3.0), `SDLLayer` (4.0), `Render3DLayer` (4.5), `LuaLayer` (5.0), `RmlUILayer` (6.0). Plugin layers use load orders ≥ 10.0.
- **Log routing**: `LoggingLayer` owns the log buffer. `SDLLayer` and `RmlUILayer` wire their log sinks automatically in `Create()` via `ServiceLocator::TryGet<LoggingLayer>()`. Log wiring to the UI is handled automatically inside `LuaConsoleViewModel::RegisterWith()` — it fetches `ServiceLocator::Get<LoggingLayer>()` and calls `BindLogData(Log.Lines())`, assigning the result to `Log.onLog`.
- **UI documents**: `.rml` / `.rcss` files live under `assets/ui/`. `RmlUILayer::LoadDocument()` queues a path that is processed on the next `Update()` (deferred); any previously loaded page and its ViewModels are torn down first. Only **one page** may be live at a time. The loader automatically scans the file for `data-model="..."` attributes, instantiates the registered `ViewModel` via `ViewModelRegistry`, and calls `RegisterWith(context, modelName)` **before** RmlUi parses the markup. No manual `RegisterWith()` call is needed in `UGEApplication.cpp`.
- **data-fragment support**: Elements with `data-fragment="assets/ui/frag_foo.rml"` are found after load, the fragment file is read from PhysFS, its `data-model` attributes are wired to ViewModels, and the markup is injected via `SetInnerRML`. Use this to split large documents into reusable fragments (e.g. `assets/ui/frag_luaConsole.rml`).
- **ViewModel / ViewModelRegistry**: To create a new data-binding model, subclass `ViewModel` (see `source/UGECore/Public/GameClasses/ViewModel.h`), implement `RegisterWith(Rml::Context*, const char*)`, place `REGISTER_VIEWMODEL("model_name", MyViewModel)` inside a `public:` section of the **class body** (in the header). The macro injects a `static inline` member whose constructor fires at DLL load time. **Do not place it in a `.cpp` file** � it must be in the header so the compiler's implicit instantiation rules see the `static inline` member. `RmlUILayer` owns all `ViewModel` instances (page-level and fragment-level) together in `m_pageViewModels`; all instances are destroyed when `UnloadPage()` is called or a new page is queued. To make a ViewModel accessible globally, call `ServiceLocator::Provide(this)` inside `RegisterWith()`.
- **Input event propagation**: `SDLLayer::Update()` polls SDL events and forwards each to `Application::dispatchEvent()` via the registered event handler. `dispatchEvent` walks `m_layers` in **reverse** push order, `dynamic_cast`s each layer to `IEventHandler*`, and calls `HandleEvent(SDL_Event&)`. Returning `true` consumes the event and stops propagation; returning `false` continues. `RmlUILayer::HandleEvent()` first dispatches to its ViewModels in reverse order (calling `ViewModel::HandleEvent()`), then forwards unconsumed events to `RmlSDL::InputEventHandler` and returns `!result` � `true` (stop propagation) when RmlUi consumed the event (e.g. a focused `<input>` captured a keypress), `false` (continue propagating) when RmlUi left the event unhandled. To opt a new layer in, inherit `IEventHandler` alongside `AppLayer`; to opt a ViewModel in, override `ViewModel::HandleEvent()` (default returns `false`).
- **`GameObjectBase` accessors**: both `Scene` and `ViewModel` inherit `GameObjectBase` (`<GameClasses/GameObjectBase.h>`). Inside any derived class use the protected helpers � `GetLoggingLayer()`, `GetPhysFSLayer()`, `GetDataLayer()`, `GetLuaLayer()`, `GetSDLLayer()`, `GetRender3DLayer()`, `GetUILayer()` � instead of calling `ServiceLocator::TryGet<T>()` directly. All return raw non-owning pointers (null if the layer has not yet been registered); guard with a null-check when availability is uncertain.
- **`BoxCollision`**: AABB helper at `<GameClasses/BoxCollision.h>` used for tile/player collision. `Intersects(Other)` for overlap detection; `OverlapX(Other)` / `OverlapY(Other)` return the MTV depth for axis-aligned push-back. Edge accessors: `Left()`, `Top()`, `Right()`, `Bottom()`, `CenterX()`, `CenterY()`. Query registration state: `IsRegistered()`, `RegistryId()`. **`Enabled` flag**: when `false`, all intersection tests return `false` � toggle freely at runtime (e.g. to deactivate a collected coin). **`UserData` convention**: every instantiator of a `BoxCollision` member **must** set `Bounds.UserData = this` (or equivalent) immediately after construction, so that `BoxCollisionGrid::Intersects()` results can be cast back to the owning type at the call site (e.g. `static_cast<Tile*>(hit->UserData)`). `UserData` is intentionally **not** propagated by copy or move � it is the identity of the owning object and must always be stamped by that object. Classes that call `UpdateCollision()` should set `UserData` inside that helper (see `Tile::UpdateCollision()`). **Auto-registration**: the value constructor `BoxCollision(X, Y, W, H [, Label])` registers with `BoxCollisionRegistry::Active()` when one is set (i.e. after `SDLLayer::InitCollisionHooks()` � called automatically in `SDLLayer::Create()`). Copy construction does **not** register (copies are for temporary collision math only). Move construction transfers the existing registration to the new address. Destruction automatically deregisters. Default construction does not register � use default + assignment for temporary boxes.
- **`BoxCollisionRegistry`**: Non-owning debug overlay registry at `<GameClasses/BoxCollisionRegistry.h>`. Owned by `SDLLayer` (as `m_collisionRegistry`). `SDLLayer::ShowCollisionBoxes(bool)` / `IsShowingCollisionBoxes()` toggle the overlay rendered in `Tick()`. Explicit opt-in (for boxes that should not auto-register): `SDLLayer::RegisterCollision(Box*, Label)` ? `CollisionHandle` (RAII, deregisters on destruction). Access the registry directly via `SDLLayer::CollisionRegistry()`. Lua: `SDL.ShowCollision()` toggles the overlay and logs the state.
- **`ScriptableObject` mixin**: any class that should expose itself to Lua inherits `ScriptableObject` (include `<ScriptableObject.h>`) and implements `RegisterObject(sol::state& Lua)`. Register the C++ API using `Lua.new_usertype<T>(...)` and expose the instance via `Lua["Name"] = this`. For objects that are created and destroyed at runtime (e.g. scene-lifetime services), also override `UnregisterObject(sol::state& Lua)` to nil out the globals: `Lua["Name"] = sol::nil`. Examples: `SDLLayer`, `RmlUILayer`.
- **`LuaLayer` script registration pattern**: `LuaLayer` owns `ScriptableObject` registration (previously `ScriptAPILayer`, which no longer exists). Two registration paths:
  - **Startup**: `UGEApplication::Create()` iterates all pushed layers via `dynamic_cast<ScriptableObject*>` and calls `luaLayer->Register(Scriptable)` on each. No manual wiring needed for layers pushed before the init script runs.
  - **Runtime (dynamic)**: call `ServiceLocator::Get<LuaLayer>().Register(obj)` to immediately invoke `RegisterObject()` and begin tracking; call `Unregister(obj)` to invoke `UnregisterObject()` and stop tracking. Use this for `ScriptableObject`s whose lifetime is shorter than the application (e.g. scene objects). `SDLLayer` calls these automatically when loading/unloading `ScriptableObject` scenes.
- **`LuaLayer` tickable functions**: `AddTickFunction(sol::protected_function Fn, const char* Name)` ? `uint32_t` registers a Lua function to be called every `Update()` (returns a tick ID); `RemoveTick(uint32_t Id)` unregisters by ID. Duplicate name registrations are silently ignored (returns existing ID). Exposed to Lua via the `UGE` table � see Lua API surfaces below.
- **`LuaLayer` backtick console toggle**: `LuaLayer::HandleEvent()` is currently non-consuming and does not toggle console visibility. The backtick toggle lives in `LuaConsoleViewModel::HandleEvent()`, which writes to the window's transient key `uiRuntime.<activeDoc>.lua-console.visible`.
- **`PhysFSLayer` helpers**: beyond `Mount()` / `Unmount()`, use `ReadFile(path)` ? `std::vector<std::byte>`, `OpenAsIOStream(path)` ? `SDL_IOStream*`, `Exists(path)` ? `bool`, `ListFiles(dir)` ? `std::vector<std::string>`, `LogFiles(dir = "")` ? logs all VFS files to `LoggingLayer` (called in `UGEApplication::Create()` after mounting). Free helper `LoadTextureFromPhysFS(Renderer, Physfs, VirtualPath)` ? `std::expected<UniqueTexture, std::string>` (declared in `Layers/SDLLayer.h`) loads an image from the VFS into an SDL texture.
- **`UDEDataLayer` state bus and dataset store**: always available (pushed by default in `UGEApplication::Create()`). Three public store objects accessed directly on the layer:
  - **`layer.State`** (`PersistentStore`) � `Set(key, val)`, `Get(key)` ? `const Value*`, `Has(key)`, `Subscribe(key, cb)`, `Unsubscribe(token)`, `Save(FilePath)`, `Load(FilePath)` (real OS paths). Typed helpers return `std::optional`: use `std::get<int/float/std::string>(*layer.State.Get(key))` or bind reactively.
  - **`layer.Transient`** (`TransientStore`) � same as State plus `Remove(key)`; never serialised.
  - **`layer.Data`** (`DatasetStore`) � read-only TOML datasets. `HasDataset(name)`, `GetInt(dataset, path)`, `GetFloat(dataset, path)`, `GetString(dataset, path)` return `std::optional`; `GetRows(dataset, arrayKey)` ? `const DataTable*` (each `DataRow` is `unordered_map<string, AppStateValue>`); `LoadFile(vfsPath)` loads a single TOML file. All `*.toml` files under `assets/DATA/` are loaded at layer creation time.
  Keys use dot-notation by convention (`"ui.console.visible"`); storage is flat. Use the `APPSTATE_BIND("key", defaultValue, callback)` macro to inject a default into `layer.State`, subscribe, and obtain an `AppStateBinding` RAII handle; use `APPSTATE_BIND_TRANSIENT` for `layer.Transient`; use `APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT(Binding_, Key_, Member_)` for the common pattern of binding a transient float key directly to a `float` member variable (subscribes, then seeds `Member_` from the current stored value immediately). Always store `AppStateBinding` as a member variable � it auto-unsubscribes on destruction; call `binding.Release()` to unsubscribe early, or test liveness with `binding.IsActive()`. `binding.SetValue(AppStateValue{...})` writes a new value to the bound key and fires all subscribers; `binding.GetValue()` returns `const AppStateValue*` to read the current stored value (returns `nullptr` if the binding is inactive or the key is absent). Prefix subscriptions (`SubscribePrefix("ui.", cb)`) fire for all keys starting with the prefix across both `State` and `Transient`.
  **Dataset store example**: `layer.Data.GetFloat("platformerData", "physics.gravity")`, `layer.Data.GetString("platformerData", "assets.background")`, `layer.Data.GetRows("platformerMap", "tiles")`.
- **`AppLayer::Log()`**: the protected `Log(std::string Msg)` method on `AppLayer` routes a message to `LoggingLayer` via `ServiceLocator`; it no-ops gracefully if `LoggingLayer` is not yet registered. Use it inside any layer's implementation instead of accessing `ServiceLocator` directly for logging.
- **`LuaLayer` `print()` redirection**: `LuaLayer` overrides Lua's `print()` to forward output to `LoggingLayer` with a `[Lua] ` prefix. Errors from `Execute()` / `ExecuteFile()` are logged and returned as `std::unexpected` so callers can decide whether to abort.
- **Lua API surfaces**: `SDLLayer` exposes `SDL.SetBackground(virtualPath)`, `SDL.LoadScene(sceneName)`, and `SDL.ShowCollision()` (toggle collision debug overlay) to Lua. `RmlUILayer` exposes `UI.LoadFont(virtualPath)` and `UI.LoadDocument(virtualPath)`. `Render3DLayer` exposes the `Render3D` table (see `Render3DLayer` entry in Key Files). `UDEDataLayer` exposes the `Data` table: `Data.SetTransient(key, value)` (type-safe set; coerces to existing backing type or infers from Lua value for new keys), `Data.GetTransient(key)` ? Lua value or nil, `Data.ShowTransient()` (logs all transient store entries). `LuaLayer` exposes the `UGE` table: `UGE.AddTickFunction(fn [, name])` ? id, `UGE.RemoveTick(id)`, `UGE.RunScript(virtualPath)`, `UGE.ShowTicking()` (logs active tick entries), `UGE.GetTickId(name)` ? id or 0. All are registered by `LuaLayer` at startup (via the startup registration pass in `UGEApplication::Create()`).
- **Scene system**: `SDLLayer` owns exactly one active `SceneObject` at a time. Derive from `SceneObject` (include `<GameClasses/SceneObject.h>`), implement `Update()`, `Tick(float deltaTime)`, and optionally `HandleEvent(SDL_Event&)`. `deltaTime` is elapsed seconds since the previous tick (matches `AppLayer::Tick`). Register with `REGISTER_SCENE("scene-name", MyScene)` placed inside a `public:` section of the class body header (same static-inline rule as `REGISTER_VIEWMODEL`). Load at runtime via `SDL.LoadScene("scene-name")` from Lua or `SDLLayer::LoadScene(const char*)` from C++; teardown of the previous scene is automatic. `SDLLayer::HandleEvent()` dispatches to the active scene _before_ the event enters the application dispatch chain (IEventHandler layers / RmlUILayer); return `true` to consume and stop propagation, `false` to continue. If a scene also inherits `ScriptableObject`, `SDLLayer` auto-calls `LuaLayer::Register()` on load and `LuaLayer::Unregister()` on unload. Observer access: `SDLLayer::ActiveScene()` returns the raw `SceneObject*` (or `nullptr`). Examples: `PlatformerScene` (`source/GamePlugins/DemoGame/Platformer/PlatformerScene.h`, full scene with tile map, score binding, and a `PlatformerCharacter` sub-object that handles physics/input/animation); `LuaTestsScene` (`source/GamePlugins/DemoGame/LuaTests/LuaTestsScene.h`, loads `assets/ui/lua_tests.rml`; activate via `SDL.LoadScene("lua-tests")`).
- **Sprite system**: Three reusable rendering helpers live under `source/UGECore/Public/Sprite/` (include root `Sprite/`):
  - **`Sprite`** (`<Sprite/Sprite.h>`) � owns a single SDL texture loaded from the VFS. `Sprite::Load(Renderer, Physfs, "assets/�", PaddingH, PaddingV)` ? `std::expected<Sprite, std::string>`. `Draw(Renderer, SDL_FRect, FlipMode)`. Optional `PaddingH`/`PaddingV` shrink the destination rect at draw time while sampling the full source texture. Query dimensions via `Width()`, `Height()`, `RenderedW()`, `RenderedH()`.
  - **`SpriteSheet`** (`<Sprite/SpriteSheet.h>`) � owns a single texture divided into a uniform grid of frames (left-to-right, top-to-bottom, 0-indexed). `SpriteSheet::Load(Renderer, Physfs, "assets/�", FrameW, FrameH, PaddingH, PaddingV)`. `Draw(Renderer, FrameIndex, Dest, FlipMode)`. Query via `FrameCount()`, `FrameW()`, `FrameH()`, `RenderedW()`, `RenderedH()`.
  - **`AnimatedSprite`** (`<Sprite/AnimatedSprite.h>`) � stateful animation controller that holds a **non-owning** `const SpriteSheet*` plus a frame sub-range `[StartFrame, StartFrame+FrameCount)`. Call `Update(float DeltaSeconds)` once per frame in `Scene::Update()`; call `Draw(Renderer, Dest, FlipMode)` in `Scene::Tick()`. `Reset()` rewinds to the first frame. The owning scene must ensure the `SpriteSheet` outlives the `AnimatedSprite`.
- **Window system (desktop UI windows)**: A reusable framework for building draggable, resizable UI windows. The architecture separates **runtime state** (position, visibility, z-order) from **widget markup** � state changes are stored transient-only and updates are driven reactively via RmlUi data-binding. **Core class**:
  - **`WindowViewModel`** (`<GameClasses/WindowViewModel.h>`, UGECore) � base class for all window data-models. Derives from `ViewModel`. Manages position, visibility, and z-order, all stored under `uiRuntime.<activeDoc>.<windowTag>.*` transient keys. Public API: `Show()` / `Hide()` / `IsVisible()`, `SetPosition(Left, Top)` / `SetLeft()` / `SetTop()`, `SetZOrder()` / `RaiseWindowToFront()`, `LoadRuntimeState()` / `SaveRuntimeState()`, `ClampToViewport()`. Transient-backed bindings are created during initialization for `posX`, `posY`, `z`, and `visible`; those callbacks update local members and call `OnRuntimeStateChanged()`. Shared protected helpers for derived windows:
    - `BindCommonWindowVars(Ctor)` � binds `panel_left` / `panel_top` / `panel_z` / `panel_visible`
    - `BindCloseEvent(Ctor [, EventName])` � binds a close callback that calls `Hide()`
    - `SyncWindowModel(Model [, ExtraDirtyVars])` � stores model handle and dirties common + extra variables
    - `HandleWindowDragEvent(Event, DragHandleId)` � reusable drag-to-move behavior
    - `setTransient(Suffix, Value)` � write runtime keys directly
    Include as `#include <GameClasses/WindowViewModel.h>`.
  - **Runtime state contract**: `RmlUILayer` writes `uiRuntime.activeDoc` to the current page slug (normalized from file path, e.g., `"platformer"` from `assets/ui/platformer.rml`). `WindowViewModel` loads/stores per-window state under:
    - `uiRuntime.<activeDoc>.<windowTag>.posX` / `.posY` � position in reference pixels
    - `uiRuntime.<activeDoc>.<windowTag>.z` � z-order (stacking) index
    - `uiRuntime.<activeDoc>.<windowTag>.visible` � visibility flag (`1` = visible, `0` = hidden)
    - `uiRuntime.<activeDoc>.currentWinZMax` � shared per-page z-order counter
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
  - **Concrete example**: `LuaConsoleViewModel` in `DemoGame.dll` derives `WindowViewModel` and adds Lua-specific data (`log_lines`, `input_text`). `RegisterWith()` uses `BindCommonWindowVars`, `BindCloseEvent`, and `SyncWindowModel`; `HandleEvent()` handles backtick toggle, Enter submit, and uses base drag handler.
  - **Constructor restore pattern**: base constructor calls `LoadRuntimeState()`, seeds transient via `SaveRuntimeState()`, then establishes bindings. This restores position/z/visibility and keeps runtime state synchronized for all derived windows.

- **`RMLUI_STATIC_LIB`**: UGECore defines this as a `PUBLIC` compile definition. Any target that includes UGECore's public headers (e.g. `DemoGame.dll`) automatically receives it � do not redefine it manually.
- **MinGW symbol export**: UGECore is built with `-Wl,-u,_ZN3Rml10FamilyBase8GetNewIdEv` so that `Rml::FamilyBase::GetNewId` (in `librmlui.a:Traits.cpp.obj`) is forced into the UGECore link and therefore exported via MinGW's default export-all behaviour. Without this, templates in RmlUi headers (e.g. `Rml::Family<T>::Id()` ? `Rml::FamilyBase::GetNewId()`) produce `undefined reference` linker errors in DemoGame.dll even though the symbol physically lives inside UGECore.dll. If a new `Rml::Family<T>::Id()` instantiation causes a similar error for a different symbol in `librmlui.a`, add another `-Wl,-u,<mangled_name>` to `target_link_options(UGECore ...)` in `source/UGECore/CMakeLists.txt`.


## RmlUi Layout Conventions
- **Top-level body children are widgets**: every direct child of `<body>` must be a self-contained widget. Give it `position: absolute` with explicit `top`/`left`/`width`/`height` so its placement is fully independent of sibling elements. Flow-based layout (normal block/inline flow, flexbox) is **not** used for page-level composition � it produces unpredictable results in RmlUi at 1600�1200 reference resolution.
- **No flow between top-level widgets**: sibling widgets must never depend on each other's flow or size. Each widget occupies its own fixed region of the canvas; use coordinates to avoid overlap.
- **Sub-elements are absolutely positioned within their widget**: child elements inside a widget use `position: absolute` with pixel offsets that are measured relative to the parent widget container � not the viewport � because CSS/RmlUi resolves absolute positioning against the nearest positioned ancestor, and the parent widget already carries `position: absolute`. Do **not** use `position: relative` for sub-element placement. Example: `#score-label` and `#score-value` inside `#score-block` in `platformer.rml` both use `position: absolute` with offsets from the score-block's top-left corner.
- **Table layout for structured multi-row/column content**: when a widget contains rows and columns (e.g. a command reference list, a tick demo panel), use `display: table` / `display: table-row` / `display: table-cell` with explicit `width` on cells. Set `position: absolute` on the table itself to anchor it on the canvas (see `table.notes-table` and `table.tick-table` in `lua_tests.rml`). Avoid `<table>` HTML element - use `<div>` / `<span>` with `display: table*` styles instead.
- **Fragment containers**: the fragment injection `<div>` (e.g. `<div id="lua-console" data-fragment="assets/ui/frag_luaConsole.rml"/>`) is a top-level absolute widget whose position is defined inside the fragment's own stylesheet � do not add layout styles to the injection host element.
- **Reference resolution**: all coordinates are expressed in the scene's 1600 � 1200 reference-resolution pixel space. The RmlUi context is sized to the SDL window; scale consistently with this grid. **Do not use `right:` or `bottom:` for absolute positioning** � RmlUi does not reliably resolve these against the canvas width/height. Instead calculate `left:` explicitly: e.g. a 150px-wide widget with a 20px right margin sits at `left: 1430px` (1600 - 150 - 20).
- **Inline styles for one-off tweaks only**: shared, repeated styles belong in `UGECore.rcss`, page-level `<style>` blocks, or dedicated `.rcss` files (e.g. `navButton.rcss`). Inline `style=""` attributes are acceptable only for truly one-off positional overrides on a single element.

## Key Files
| File | Purpose |
|---|---|
| `source/Application/main.cpp` | Entry point; explicitly loads UIWindows.dll and DemoGame.dll plugins via `ModuleLoader` before calling `UGEApplication::Create()`; both DLLs are loaded relative to the executable directory so REGISTER_LAYER/REGISTER_VIEWMODEL/REGISTER_SCENE static initialisers fire before the application initializes. Configures startup via `UGEApplication::Settings` before calling `Create()` |
| `source/UGECore/Public/UGEApplication.h` | `UGEApplication` class; `LaunchSettings Settings` static field (configure before calling `Create()`); `Create(int argc, char* argv[])` factory returning `std::expected`; `Run()` frame loop; owns all layers via `m_layers`; observer pointers to core layers (`m_logLayer`, `m_physFSLayer`, `m_sdlLayer`, `m_luaLayer`, `m_render3dLayer`, `m_rmlLayer`) |
| `source/UGECore/Private/UGEApplication.cpp` | Startup and runtime implementation – all core layers are pushed via a single loop over `LayerRegistry::Instance().Names()` (sorted by load order); typed observer pointers are populated from `ServiceLocator` after the loop. Read this to understand default `Settings.MountPoints` seeding, the `InitScript` execution, and the Lua ScriptableObject registration pass. |
| `source/UGECore/Public/LaunchSettings.h` | Plain struct holding startup configuration: window title, width, height, executable path, `InitScript` (default `"assets/lua/initApp.lua"`; set `""` to skip), and `MountPoints` (`std::vector<MountPoint>`). Configure `UGEApplication::Settings` in `main()` before calling `Create()`. Include as `#include <LaunchSettings.h>` |
| `source/UGECore/Public/LayerRegistry.h` | Singleton mapping layer name strings to `AppLayer` factory functions. Plugins use `REGISTER_LAYER("name", loadOrder, Type)` macro (in `public:` section) to auto-register; static initialiser calls `LayerRegistry::Instance().RegisterFactory()` at DLL load time. `LayerRegistry::Names()` returns registered names sorted by load order. `LayerRegistry::Create(name)` instantiates a layer. Include as `#include <LayerRegistry.h>` |
| `source/UGECore/Public/ServiceLocator.h` | Type-safe service registry and singleton container. Methods: `Provide<T>(T*)` (register an instance), `Get<T>()` (fetch most recent; throws if absent), `TryGet<T>()` (fetch most recent; returns nullptr if absent), `Has<T>()` (returns bool), `Remove<T>()` (deregister), `Clear()` (deregister all). Lifetime managed by layer ownership; never store raw pointers longer than the owning layer lives. Include as `#include <ServiceLocator.h>` |
| `source/UGECore/Public/Layers/AppLayer.h` | Base class for all layers (`Update()` / `Tick(float deltaTime)`); protected `Log(Msg)` helper routes to `LoggingLayer` |
| `source/UGECore/Public/Layers/LoggingLayer.h` | Application-wide log buffer; `MakeSink()` / `onLog` / `BindLogData()` / `Clear()` |
| `source/UGECore/Public/Layers/PhysFSLayer.h` | Virtual filesystem wrapper; `Mount`, `ReadFile`, `Exists`, `ListFiles` |
| `source/UGECore/Public/Layers/UDEDataLayer.h` | Key-value state bus + TOML dataset store; three public stores: `State` (persistent), `Transient` (runtime), `Data` (TOML datasets); `APPSTATE_BIND` / `APPSTATE_BIND_TRANSIENT` / `APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT` macros, `AppStateBinding` RAII handle; implements `ScriptableObject` � exposes `Data.SetTransient/GetTransient/ShowTransient` to Lua; store headers in `Layers/DataStores/` |
| `source/UGECore/Public/Layers/LuaLayer.h` | Lua 5.4 VM (sol2); `Execute()` / `ExecuteFile()` / `State()` for binding; `Register(ScriptableObject*)` / `Unregister(ScriptableObject*)` for Lua API management; `AddTickFunction(fn, name)` ? id / `RemoveTick(id)` for per-frame Lua callbacks; `HandleEvent()` is currently non-consuming (console toggle is handled by `LuaConsoleViewModel`) |
| `source/UGECore/Public/Layers/SDLLayer.h` | SDL window, renderer, background texture, event loop; scene management (`LoadScene`, `UnloadScene`, `ActiveScene`); collision debug overlay (`RegisterCollision`, `ShowCollisionBoxes`, `IsShowingCollisionBoxes`); auto-calls `LuaLayer::Register/Unregister` for `ScriptableObject` scenes |
| `source/UGECore/Public/Layers/Render3DLayer.h` | CPU-rasterised 3D viewport layer; loads immediately after `SDLLayer` and renders before `RmlUILayer`; no-op until `Activate()`. Lifecycle: `Activate()` / `Deactivate()` / `IsActive()`. Model: `LoadModel(VirtualPath)` (deferred to next Update; tinyobjloader parses .obj+.mtl from PhysFS), `UnloadModel()`, `HasModel()`. Viewport: `SetViewport(SDL_FRect)` � arbitrary sub-rect of the SDL window; auto-fills to full renderer size on `Activate()` if not set. Transform: `SetModelRotation(PitchDeg, YawDeg, RollDeg)`, `SetModelScale(float)`, `SetModelPosition(X,Y,Z)`. Camera: `Camera()` ? `Camera3D&` (mutate `Eye`, `Target`, `Up`, `FovDeg`, `NearZ`, `FarZ`) or `SetCamera(eyeX,eyeY,eyeZ, targetX,targetY,targetZ [,fovDeg])`. Lighting: `Light()` ? `Light3D&` (mutate `Direction`, `Color`, `Ambient`) or `SetLightDirection/Color/AmbientColor(...)`. Clear: `SetViewportClearColor(SDL_Color)` / `ClearViewportClear()`. Rendering uses `SDL_RenderGeometry()` + painter's algorithm (back-to-front face sort by view-space centroid Z). Implements `IScriptableObject` � exposes `Render3D` Lua table: `Activate`, `Deactivate`, `IsActive`, `LoadModel`, `SetViewport`, `SetModelRotation`, `SetModelScale`, `SetModelPosition`, `SetCamera`, `SetLightDirection`, `SetLightColor`, `SetAmbientColor`, `SetClearColor`. Includes `Render3D/Camera3D.h`, `Render3D/Light3D.h`, `Render3D/Mesh3D.h` (all use glm types). Access in game code via `GetRender3DLayer()` from `GameObjectBase`. |
| `source/UGECore/Public/Render3D/Camera3D.h` | `Camera3D` struct: `Eye`, `Target`, `Up` (glm::vec3), `FovDeg`, `NearZ`, `FarZ` (float). Mutate directly via `Render3DLayer::Camera()`. |
| `source/UGECore/Public/Render3D/Light3D.h` | `Light3D` struct: `Direction`, `Color`, `Ambient` (glm::vec3, [0,1] range). Mutate directly via `Render3DLayer::Light()`. |
| `source/UGECore/Public/Render3D/Mesh3D.h` | `Mesh3D` struct (owns `std::vector<Triangle3D>`); `Triangle3D` holds `V[3]`/`N[3]`/`UV[3]` (glm types) + `Material3D` (`Ambient`, `Diffuse`, `Specular`, `Shininess`). Owned by `Render3DLayer`. |
| `source/UGECore/Public/GameClasses/SceneObject.h` | Abstract base class for game scenes; `Update()`, `Tick()`, `HandleEvent()`; owns `m_renderer`/`m_window`; inherits `GameObjectBase` |
| `source/UGECore/Public/SceneRegistry.h` | Singleton scene factory registry + `REGISTER_SCENE(SceneName, Type)` macro |
| `source/UGECore/Public/Layers/RmlUILayer.h` | RmlUi lifecycle, document load, per-frame render; owns ViewModel instances |
| `source/UGECore/Public/PhysFSFileInterface.h` | Bridges PhysFS ? RmlUi file I/O |
| `source/UGECore/Public/ScriptableObject.h` | Mixin interface; implement `RegisterObject(sol::state&)` to expose a class to Lua |
| `source/UGECore/Public/IEventHandler.h` | Opt-in mixin for event-handling layers and ViewModels; `HandleEvent()` returns bool |
| `source/UGECore/Public/GameClasses/GameObjectBase.h` | Common base for `Scene` and `ViewModel`; provides protected accessors `GetLoggingLayer()`, `GetPhysFSLayer()`, `GetDataLayer()`, `GetLuaLayer()`, `GetSDLLayer()`, `GetUILayer()` via `ServiceLocator::TryGet<T>()` |
| `source/UGECore/Public/GameClasses/BoxCollision.h` | AABB collision helper; `Intersects()`, `ContainsPoint()`, `ContainsBox()`, `OverlapX()`, `OverlapY()`; edge accessors `Left/Top/Right/Bottom/CenterX/CenterY()`; auto-registers with `BoxCollisionRegistry::Active()` on value construction; include root is `GameClasses/` � use `#include <GameClasses/BoxCollision.h>` |
| `source/UGECore/Public/GameClasses/BoxCollisionRegistry.h` | Non-owning debug overlay registry; owned by `SDLLayer`; `Register(Box*, Label)` ? `CollisionHandle` (RAII); `SetActive()`/`Active()` singleton; `BoxCollision` value constructors call `RegisterDirect()` automatically |
| `source/UGECore/Public/GameClasses/ViewModel.h` | Abstract base for all RmlUi data-model ViewModels; defines `RegisterWith()`; inherits `GameObjectBase` |
| `source/UGECore/Public/ViewModelRegistry.h` | Singleton factory registry + `REGISTER_VIEWMODEL(ModelName, Type)` macro |
| `source/UGECore/Public/GameClasses/WindowViewModel.h` | Base class for reusable RmlUi window-like ViewModels; manages position, visibility, and z-order via transient state (`uiRuntime.<activeDoc>.<windowTag>.*` keys); transient bindings (`posX/posY/z/visible`) keep local members synchronized and trigger base `OnRuntimeStateChanged()`; public API: `Show()`, `Hide()`, `IsVisible()`, `SetPosition()`, `SetLeft()`, `SetTop()`, `SetZOrder()`, `RaiseWindowToFront()`, `LoadRuntimeState()`, `SaveRuntimeState()`, `ClampToViewport()`; reusable protected helpers: `BindCommonWindowVars()`, `BindCloseEvent()`, `SyncWindowModel()`, `HandleWindowDragEvent()`, `setTransient()` |
| `source/GamePlugins/UIWindows/CMakeLists.txt` | `UIWindows` shared library; links `PUBLIC UGECore`; loaded first at startup (before DemoGame) via `ModuleLoader.hpp` so `REGISTER_VIEWMODEL` static initialisers register with `ViewModelRegistry` before `DemoGame` is loaded. Provides `WindowViewModel` base class and test window implementations available to all plugins. |
| `source/GamePlugins/UIWindows/WindowViewModel.h` / `.cpp` | Full implementation of `WindowViewModel` base class (public headers in `source/UGECore/Public/GameClasses/WindowViewModel.h`); owns position, visibility, z-order management via transient AppState keys. Implements derived window implementation patterns for plugins. |
| `source/GamePlugins/UIWindows/TestWindow/TestWindowViewModel.h` / `.cpp` | Example derived window ViewModel (`REGISTER_VIEWMODEL("test-window", TestWindowViewModel)`); demonstrates load order, position binding, and close event handling. Can be loaded via `UI.LoadDocument("assets/ui/frag_testWindow.rml")` as a test. |
| `source/GamePlugins/DemoGame/LuaConsoleViewModel.h` | RmlUi data-binding model for the Lua console UI panel; lives in `DemoGame.dll`; derives `WindowViewModel` with window tag `"lua-console"`; self-registers via `REGISTER_VIEWMODEL`; `RegisterWith()` follows base helper pattern (`BindCommonWindowVars`, `BindCloseEvent`, `SyncWindowModel`) while binding console-specific fields (`log_lines`, `input_text`); `HandleEvent()` handles backtick toggle, Enter submit, and uses base drag handler |
| `source/GamePlugins/DemoGame/GameModule.h` | Legacy no-op compatibility export; startup DLL loading is now handled explicitly by `ModuleLoader.hpp` in `source/Application/` |
| `source/GamePlugins/DemoGame/CMakeLists.txt` | `DemoGame` shared library; links `PUBLIC UGECore` and `PUBLIC UIWindows` (depends on UIWindows for WindowViewModel base class); loaded second at startup via `ModuleLoader.hpp` (after UIWindows) so `REGISTER_VIEWMODEL`/`REGISTER_SCENE` static initialisers fire before `UGEApplication::Create()` calls `LoadDocument()`. |
| `source/UGECore/CMakeLists.txt` | UGECore shared library build; include paths, deps, export header; defines `RMLUI_SDL_VERSION_MAJOR=3` and `RMLUI_STATIC_LIB` as PUBLIC |
| `source/Application/CMakeLists.txt` | Executable entry-point CMake; `target_link_libraries(untitled PRIVATE UGECore Game)` � the actual place Game is linked into the executable |
| `pack_assets.cmake` | Asset zipping script (invoked as CMake `-P` script) |
| `assets/lua/initApp.lua` | Startup Lua script executed at the end of `UGEApplication::Create()`; loads fonts and the platformer scene via `UI.LoadFont` and `SDL.LoadScene("platformer")`; `SDL.LoadScene("lua-tests")` is available as a commented-out alternative; `UI.LoadDocument` is **not** called here � `PlatformerScene` loads the document itself via the `document` key in its config file |
| `assets/ui/main.rml` | Main UI document (not currently loaded at startup � superseded by `platformer.rml` in `initApp.lua`) |
| `assets/ui/platformer.rml` | Platformer HUD document; uses `data-model="platformer-hud"`; displays score (top-left) and Next button (top-right) |
| `assets/ui/frag_luaConsole.rml` | Lua console panel fragment injected via `data-fragment` |
| `assets/ui/lua_tests.rml` | Lua control demos document; uses `data-model="lua-tests"`; demonstrates tick functions, coin animation, physics tweaking commands, and `SDL.ShowCollision()` |
| `assetsDevOverride/` | Source-tracked asset overrides; copied to `cmake-build-debug/OVERRIDE/assets/` on each build |
| `cmake-build-debug/OVERRIDE/assets/` | Drop files here at runtime to override assets without repacking |
| `source/UGECore/Public/Sprite/Sprite.h` | Single-texture sprite; `Sprite::Load(Renderer, Physfs, VirtualPath, PaddingH, PaddingV)` ? `std::expected<Sprite, std::string>`; `Draw(Renderer, Dest, FlipMode)` |
| `source/UGECore/Public/Sprite/SpriteSheet.h` | Uniform-grid sprite sheet; `SpriteSheet::Load(�, FrameW, FrameH, PaddingH, PaddingV)`; `Draw(Renderer, FrameIndex, Dest, FlipMode)` |
| `source/UGECore/Public/Sprite/AnimatedSprite.h` | Stateful animation controller (non-owning `SpriteSheet*`); `Update(DeltaSeconds)` / `Draw(Renderer, Dest, FlipMode)` / `Reset()` |
| `source/GamePlugins/DemoGame/Platformer/Tile.h` | `Tile` struct: reference-space position, color fallback, optional non-owning `Sprite*`, `IsCoin` flag (collectible; passable and removed on player overlap); shared `SIZE = 160.0f` reference-px constant. Include root is `source/GamePlugins/DemoGame/Platformer/` � use `#include <Tile.h>` |
| `source/GamePlugins/DemoGame/Platformer/PlatformerScene.h` | Full platformer scene (`"platformer"`); owns tile world and score binding; delegates physics/input/animation to `PlatformerCharacter`; loads config from `UDEDataLayer::Data` (`PLATFORMER_DATA_KEY = "platformerData"`); exposes `plat2d.Jump()` to Lua via `PlatformerCharacter::RegisterObject`. Reference resolution `1600 � 1200`. Include root is `source/GamePlugins/DemoGame/Platformer/` |
| `source/GamePlugins/DemoGame/Platformer/PlatformerCharacter.h` | Owns player physics, input, animation (`SpriteSheet` / `AnimatedSprite`), and tile collision (using `BoxCollision`); exposes `plat2d.Jump()` to Lua via `ScriptableObject`; `DrainCollectedCoins()` returns coins collected since last call. Constructor: `PlatformerCharacter(SDL_Renderer*, SDL_Window*, std::vector<Tile>& World, const char* DatasetKey)` � assets and physics tunables loaded internally from the dataset and bound reactively to transient AppState keys defined in `PlatformerStateTags.h`. Include as `#include <PlatformerCharacter.h>` |
| `source/GamePlugins/DemoGame/Platformer/PlatformerStateTags.h` | `constexpr std::string_view` constants for transient physics tunables: `TAG_GRAVITY = "plat2d.physics.gravity"`, `TAG_GROUND_EPSILON`, `TAG_SPEED`, `TAG_JUMP_HEIGHT`. Used by `PlatformerCharacter` (and bound in `PlatformerScene`) to expose live-tunable physics over the `UDEDataLayer::Transient` store. Include as `#include <PlatformerStateTags.h>` |
| `source/GamePlugins/DemoGame/LuaTests/LuaTestsScene.h` | Scene `"lua-tests"` (`REGISTER_SCENE`); loads `assets/ui/lua_tests.rml` on construction. Include root is `source/GamePlugins/DemoGame/LuaTests/`. Activate via `SDL.LoadScene("lua-tests")` |
| `source/GamePlugins/DemoGame/LuaTests/LuaTestsViewModel.h` | ViewModel `"lua-tests"` (`REGISTER_VIEWMODEL`); drives coin animation demo; binds `KEY_COIN_X`, `KEY_COIN_Y`, `KEY_COIN_DIRECTION`, `KEY_STATE_STOP` transient keys; exposes `onNext` callback; `SetCoinPosition(Left, Top)` |
| `source/GamePlugins/DemoGame/RmlArrayRegistry.h` | Declares `EnsureGameArrayTypesRegistered(Rml::DataModelConstructor&)` � call from any ViewModel in `DemoGame.dll` that needs `std::vector<std::string>` registered with the RmlUi context; guards against "already declared" errors when multiple ViewModels share the same context |
| `assets/Maps/platformerMap.txt` | ~~Moved to `assets/DATA/platformerMap.toml`~~ |
| `assets/DATA/platformerData.txt` | ~~Legacy key-value text config; superseded by `platformerData.toml`. `PlatformerScene` now reads config via `UDEDataLayer::Data` using `PLATFORMER_DATA_KEY = "platformerData"`.~~ |
| `assets/DATA/platformerMap.txt` | ~~Legacy tile map text format; superseded by `platformerMap.toml`.~~ |
| `assets/DATA/platformerData.toml` | Runtime config for `PlatformerScene` in TOML format; sections: `[physics]` (`gravity`, `ground_epsilon`, `speed`, `jump_height`), `[assets]` (`background`, `document`, `tile_map`), `[tiles]` (sprite paths keyed by tile type), `[character]` (walk/jump/fall sprite paths). Loaded by `UDEDataLayer` as dataset `"platformerData"`. |
| `assets/DATA/platformerMap.toml` | Tile map for `PlatformerScene` in TOML format; contains `[[tiles]]` array-of-tables with fields `x`, `y`, `sprite`. Loaded as dataset `"platformerMap"`; access via `GetDataRows("platformerMap", "tiles")`. |
