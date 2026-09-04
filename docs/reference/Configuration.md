# Configuration

Runtime configuration is driven entirely by **`UGESettings.toml`** in the executable directory
and the plain **`LaunchSettings`** struct populated from it.

Related: [AssetSystem.md](../systems/AssetSystem.md) · [BuildWorkflow.md](BuildWorkflow.md)
Key files:
[`LaunchSettings.h`](../../source/UGECore/Public/LaunchSettings.h),
[`ConfigLoader.h`](../../source/Application/ConfigLoader.h)

---

## LaunchSettings

`LaunchSettings` (see `LaunchSettings.h`) is a plain struct populated from `UGESettings.toml` at
startup by `ConfigLoader` and passed directly to `UGEApplication::Create()`.

Fields: `WindowTitle`, `WindowWidth`, `WindowHeight`, `ExecutablePath`, `InitScript`,
`MountPoints`, `Modules` (platform-independent plugin names loaded via `ModuleLoader`).

---

## UGESettings.toml

The single source of runtime configuration, copied to `cmake-build-debug/` by the build. Edit
to add/remove plugins or change mounts **without recompiling**.

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

---

## Startup Sequence

`main()`:

1. Calls `LoadLaunchSettingsFromAppDirectory()` (reads `UGESettings.toml`).
2. Calls `ModuleLoader::LoadModulesRelativeToExecutable(settings.Modules, error)` to load all
   plugin DLLs — firing `REGISTER_LAYER` / `REGISTER_VIEWMODEL` / `REGISTER_SCENE` static
   initialisers.
3. Calls `UGEApplication::Create(Argc, Argv, settings)`.

No DLL names are hardcoded in `main.cpp`; add/remove plugins by editing `UGESettings.toml`.

- `InitScript` defaults to `"assets/lua/initApp.lua"`; set to `""` to skip Lua initialisation.
- `PhysFSLayer::Create()` mounts every `MountPoints` entry; non-existent paths are skipped with
  a warning (see [AssetSystem.md](../systems/AssetSystem.md)).

