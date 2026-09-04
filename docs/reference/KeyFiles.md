# Key Files

A map from subsystem to the header/source that owns it. Paths are relative to the repository
root.

Related: [architecture/Overview.md](../architecture/Overview.md) · [../README.md](../README.md)

---

## Application

| File | Purpose |
|---|---|
| `source/Application/main.cpp` | Entry point; reads `UGESettings.toml` via `LoadLaunchSettingsFromAppDirectory()`, loads plugin DLLs under `[Modules] Load` via `ModuleLoader::LoadModulesRelativeToExecutable()`, then calls `UGEApplication::Create(Argc, Argv, settings)`. No DLL names hardcoded. |
| `source/Application/ConfigLoader.h` / `.cpp` | `LoadLaunchSettingsFromAppDirectory()` — reads `UGESettings.toml` and returns a populated `LaunchSettings`. |
| `source/Application/CMakeLists.txt` | Executable entry-point CMake; links `PRIVATE UGECore`; no game or plugin logic. |
| `UGESettings.toml` | Per-deployment runtime configuration. Copied to `cmake-build-debug/` by the build. See [Configuration.md](Configuration.md). |
| `assets/lua/initApp.lua` | Startup Lua script executed at the end of `UGEApplication::Create()`; loads fonts and the initial scene. |

## UGECore — Application & Layer Infrastructure

| File | Purpose |
|---|---|
| `source/UGECore/Public/UGEApplication.h` | `UGEApplication` class; `LaunchSettings Settings` static field; `Create(...)` factory; `Run()` frame loop; owns all layers via `m_layers`. |
| `source/UGECore/Private/UGEApplication.cpp` | Startup/runtime impl — layers pushed via `LayerRegistry`, `InitScript` executed, Lua `IScriptableObject` registration pass. |
| `source/UGECore/Public/LaunchSettings.h` | Plain struct: `WindowTitle`, `WindowWidth`, `WindowHeight`, `ExecutablePath`, `InitScript`, `MountPoints`, `Modules`. |
| `source/UGECore/Public/Layers/AppLayer.h` | Base class for all layers (`Update()` / `Draw(float)`); protected `Log(Msg)` helper. |
| `source/UGECore/Public/ServiceLocator.h` | Type-safe service registry. `Provide<T>()`, `Get<T>()`, `TryGet<T>()`, `Has<T>()`, `Remove<T>()`, `Clear()`. |
| `source/UGECore/Public/LayerRegistry.h` | Singleton layer factory registry + `REGISTER_LAYER(name, loadOrder, Type)` macro. |
| `source/UGECore/Public/SceneRegistry.h` | Singleton scene factory registry + `REGISTER_SCENE(name, Type)` macro. |
| `source/UGECore/Public/ViewModelRegistry.h` | Singleton ViewModel factory registry + `REGISTER_VIEWMODEL(name, Type)` macro. |
| `source/UGECore/CMakeLists.txt` | UGECore shared library build; defines `RMLUI_SDL_VERSION_MAJOR=3` and `RMLUI_STATIC_LIB` as PUBLIC. |

## UGECore — Core Layers

| File | Purpose |
|---|---|
| `source/UGECore/Public/Layers/LoggingLayer.h` | Application-wide log buffer; `MakeSink()` / `onLog` / `BindLogData()` / `Clear()`. |
| `source/UGECore/Public/Layers/PhysFSLayer.h` | Virtual filesystem wrapper; `Mount`, `ReadFile`, `Exists`, `ListFiles`, `OpenAsIOStream`. |
| `source/UGECore/Public/Layers/UGEDataLayer.h` | Unified metadata-driven `DataStore` (`Store` member); `Save`/`Load`; `DataBinding` RAII; `DATA_BIND*` macros; Lua `Data.*` bridge. |
| `source/UGECore/Public/Layers/DataStores/DataStore.h` | Core `DataStore` API + `RowsView`/`Row` iteration. |
| `source/UGECore/Public/Layers/DataStores/DataValue.h` | Type-erased `DataValue`; `ValueConverter` + registry; `MakeDataValueFromToml`. |
| `source/UGECore/Public/Layers/DataStores/Vec2.h` | `Vec2` atomic two-component value; TOML + Lua converter. |
| `source/UGECore/Public/Layers/DataStores/DataMeta.h` | `DataMeta` (`Serialize`, `Source`, `SourcePath`) + `DataSource` enum. |
| `source/UGECore/Public/Layers/SDLLayer.h` | SDL window, renderer, background, event loop, 2-D `Camera2D`; `LoadTextureFromPhysFS()`. |
| `source/UGECore/Public/Layers/SceneManagerLayer.h` | Owns the active `SceneObject`; scene lifecycle; forwards events + Lua `Scene.Load`; load order 4.05. |
| `source/UGECore/Public/Layers/PhysicsLayer.h` | Box2D world (load order 4.1); `InitPhysics`/`CreateBody`/`SetContactCallback`/`EnableStateSync`; `Physics.*` Lua table. |
| `source/UGECore/Public/Physics/PhysicsTypes.h` | `BodyDef`, `BodyType`, `PhysicsWorldCreateParams`, `ContactEvent`; `physics.*` tag definitions. |
| `source/UGECore/Public/Physics/PhysicsBodyHandle.h` | RAII handle to a Box2D body; ownership contract; `ApplyImpulse`/`IsGrounded`/etc. |
| `source/UGECore/Public/Layers/LuaLayer.h` | Lua 5.4 VM (sol2); `Execute()` / `ExecuteFile()`; `Register`/`Unregister`; `AddTickFunction`/`RemoveTick`. |
| `source/UGECore/Public/Layers/RmlUILayer.h` | RmlUi lifecycle, document load, per-frame render; owns all ViewModel instances. |
| `source/UGECore/Public/PhysFSFileInterface.h` | Bridges PhysFS → RmlUi file I/O. |

## UGECore — Game Classes & Shared Components

| File | Purpose |
|---|---|
| `source/UGECore/Public/GameClasses/SceneObject.h` | Base class for scenes (`Update`, `Draw`, optional `HandleEvent`); use with `SceneRegistry`. |
| `source/UGECore/Public/GameClasses/ViewModel.h` | Base class for RmlUi data-model ViewModels (`RegisterWith`); use with `ViewModelRegistry`. |
| `source/UGECore/Public/GameClasses/GameObjectBase.h` | Common base for `Scene` and `ViewModel`; protected layer accessors. |
| `source/GamePlugins/UIWindows/WindowViewModel.h` | Base class for window-style ViewModels; position/visibility/z-order via transient `DataStore`; helpers `BindCommonWindowVars`, `BindCloseEvent`, `SyncWindowModel`, `HandleWindowDragEvent`. Link against `UIWindows`. |
| `source/UGECore/Public/GameClasses/BoxCollision.h` | AABB collision; `Intersects()`, `OverlapX/Y()`, edge accessors; auto-registers on value construction. |
| `source/UGECore/Public/GameClasses/BoxCollisionRegistry.h` | Non-owning debug overlay registry owned by `SDLLayer`; `Register(Box*, Label)` → `CollisionHandle` RAII. |
| `source/UGECore/Public/GameClasses/BoxCollisionGrid.h` | Broad-phase spatial grid; build once, query with `Intersects(Target, OutHits)`. |
| `source/UGECore/Public/Sprite/Sprite.h` | Single-texture sprite; `Sprite::Load(...)` → `std::expected`; `Draw(...)`. |
| `source/UGECore/Public/Sprite/SpriteSheet.h` | Uniform-grid sprite sheet; `Draw(Renderer, FrameIndex, Dest, FlipMode)`. |
| `source/UGECore/Public/Sprite/AnimatedSprite.h` | Stateful animation controller (non-owning `SpriteSheet*`); `Update`/`Draw`/`Reset`. |
| `source/UGECore/Public/Animation/AnimationControllerBase.h` | TOML-driven named-clip animation controller; `GameObjectBase`-derived; `LoadAnimation`/`SetCurrentAnimation`/`Tick`/`Draw`. |
| `source/UGECore/Public/IScriptableObject.h` | Mixin; implement `RegisterObject(sol::state&)` to expose a class to Lua; override `UnregisterObject`. |
| `source/UGECore/Public/IEventHandler.h` | Opt-in mixin for event-handling layers/ViewModels; `HandleEvent()` returns bool. |
| `source/UGECore/Public/ScriptableObject.h` | Backward-compat shim — `using ScriptableObject = IScriptableObject`. Prefer `<IScriptableObject.h>`. |

## Game Plugins — TeaPot3D

| File | Purpose |
|---|---|
| `source/GamePlugins/TeaPot3D/Render3DLayer/Render3DLayer.h` | CPU-rasterised 3D viewport layer (load order 4.5); `Render3D.*` Lua table. |
| `source/GamePlugins/TeaPot3D/Render3DLayer/Camera3D.h` | `Camera3D` struct: `Eye`, `Target`, `Up`, `FovDeg`, `NearZ`, `FarZ`. |
| `source/GamePlugins/TeaPot3D/Render3DLayer/Light3D.h` | `Light3D` struct: `Direction`, `Color`, `Ambient` (glm::vec3, [0,1]). |
| `source/GamePlugins/TeaPot3D/Render3DLayer/Mesh3D.h` | `Mesh3D` owns `std::vector<Triangle3D>`; `Triangle3D` holds vertices, normals, UVs + `Material3D`. |

