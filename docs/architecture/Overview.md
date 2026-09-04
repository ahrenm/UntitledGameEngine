# Architecture Overview

**Untitled Game Engine (UGE)** is a C++23 desktop application using **SDL3 + RmlUi** for
rendering and UI, with **PhysicsFS** as a virtual filesystem for asset loading. All
dependencies are fetched at configure time via CMake `FetchContent` (SDL3, SDL3_image,
RmlUi, PhysicsFS, FreeType, Lua 5.4, sol2, **glm**, **tinyobjloader**, **Box2D**).

Related: [LayerSystem.md](LayerSystem.md) · [FrameLoop.md](FrameLoop.md) ·
[Key Files](../reference/KeyFiles.md)

---

## Subsystems

The project is organized into three broad subsystems:

1. **Application** (`source/Application/`) — Platform-specific entry point and launching
   code. Reads `UGESettings.toml`, loads plugin DLLs via `ModuleLoader`, and calls
   `UGEApplication::Create()` to bootstrap the engine. This is where the executable
   (`untitled.exe`) is built; no engine logic lives here.

2. **UGECore** (`source/UGECore/`) — The core engine library (shared DLL `libUGECore.dll`).
   Contains:
   - Base classes (`AppLayer`, `SceneObject`, `ViewModel`, `GameObjectBase`)
   - Core layers (`LoggingLayer`, `PhysFSLayer`, `UGEDataLayer`, `SDLLayer`,
     `SceneManagerLayer`, `PhysicsLayer`, `LuaLayer`, `RmlUILayer`)
   - Static registries (`LayerRegistry`, `SceneRegistry`, `ViewModelRegistry`)
   - Shared components (sprite system, animation controller, box collision,
     window view-model framework)
   - Lua scripting infrastructure and Lua API surfaces

3. **Game Plugins** (`source/GamePlugins/`) — Loadable DLL modules that extend the engine.
   Each plugin can contain:
   - **Game Scenes & ViewModels** + supporting code (e.g. `DemoGame/Platformer/` —
     platformer scene, character physics, HUD)
   - **Shared/Reusable Components** (e.g. `UIWindows/` — base window model, derived windows)
   - **Custom Layers** (e.g. `TeaPot3D/Render3DLayer` — alternative rendering backend)
   - All registered via static `REGISTER_LAYER` / `REGISTER_SCENE` / `REGISTER_VIEWMODEL`
     macros; discovered automatically at DLL load.

**Underpinning all**: The **Assets & Data** systems provide binary resources (textures,
animations, 3D models) and arbitrary TOML data tables (game config, tile maps, entity
properties). Both are loaded through the virtual filesystem (`PhysFSLayer`) at runtime —
see [AssetSystem.md](../systems/AssetSystem.md) and [DataStore.md](../systems/DataStore.md).

---

## Module Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          untitled.exe (Application)                      │
│                     Reads UGESettings.toml, loads plugins                │
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

---

## Layer Hierarchy & Lifecycle

```
main.cpp
  +-- [Plugin Loading]           → Loads DLLs listed in UGESettings.toml [Modules] via ModuleLoader
                                   (Static REGISTER_LAYER/REGISTER_SCENE initialisers fire at DLL load)
  +-- UGEApplication::Create()   → factory returning std::expected; owns all subsystems
        +-- [Core Layers (auto-instantiated via LayerRegistry, by load order)]
        +-- LoggingLayer (1)      → owns the log-line buffer; must be pushed first
        +-- PhysFSLayer (2)       → virtual filesystem (assets.pak + OVERRIDE/assets/)
        +-- UGEDataLayer (3)      → unified metadata-driven DataStore; pushed by default
        +-- SDLLayer (4)          → window, renderer, background texture, event loop, 2-D camera
        +-- SceneManagerLayer (4.05) → owns the active Scene; only one at a time
          +-- Scene              → active scene; owned by SceneManagerLayer
        +-- PhysicsLayer (4.1)    → Box2D world; b2World_Step + contacts + debug draw
        +-- [Render3DLayer (4.5)] → CPU-rasterised 3D viewport; lives in TeaPot3D.dll (plugin)
        +-- LuaLayer (5)          → sol::state VM + ScriptableObject registry; IEventHandler
        +-- RmlUILayer (6)        → RmlUi context, document loading, per-frame render
          +-- Document           → active UI document; only one at a time
            +-- Fragment(n)      → optional fragments injected via data-fragment
          +-- ViewModel(n)       → owned by RmlUILayer; instantiated from data-model attributes
        +-- [Plugin Layers (Load Order 10.0+, auto-instantiated via LayerRegistry)]
```

Load order values are declared in each layer's `REGISTER_LAYER(...)` macro; `Create()` pushes
layers in ascending order. See [LayerSystem.md](LayerSystem.md) for the registration mechanics
and [FrameLoop.md](FrameLoop.md) for per-frame ordering.

**Scene teardown** is driven explicitly by `UGEApplication`'s destructor while all layers are
still alive — this guarantees that a scene's `PhysicsBodyHandle`s release their Box2D bodies
*before* `PhysicsLayer::ShutdownPhysics()` runs. See [Physics.md](../systems/Physics.md) and
[SceneSystem.md](../systems/SceneSystem.md).

