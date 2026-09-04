# 3D Rendering (TeaPot3D)

`Render3DLayer` is an **optional, CPU-rasterised** 3D viewport. It lives in the **`TeaPot3D.dll`
plugin** (load order 4.5), **not** in UGECore. It is a no-op until `Activate()` and fetches its
`SDL_Renderer` via `ServiceLocator`.

Related: [Overview.md](../architecture/Overview.md) ·
[LuaApiReference.md](../reference/LuaApiReference.md)
Key files:
[`Render3DLayer.h`](../../source/GamePlugins/TeaPot3D/Render3DLayer/Render3DLayer.h),
[`Camera3D.h`](../../source/GamePlugins/TeaPot3D/Render3DLayer/Camera3D.h),
[`Light3D.h`](../../source/GamePlugins/TeaPot3D/Render3DLayer/Light3D.h),
[`Mesh3D.h`](../../source/GamePlugins/TeaPot3D/Render3DLayer/Mesh3D.h)

---

## Access from C++

`Render3DLayer` is not a UGECore type and has **no** `GameObjectBase` accessor. Fetch it via
`ServiceLocator::TryGet<Render3DLayer>()` from code that links against `TeaPot3D`.

Registered via `REGISTER_LAYER` in the plugin DLL; loaded when `TeaPot3D` is listed under
`[Modules] Load` in `UGESettings.toml`.

---

## Data Types

| Type | Fields |
|---|---|
| `Camera3D` | `Eye`, `Target`, `Up`, `FovDeg`, `NearZ`, `FarZ`. |
| `Light3D` | `Direction`, `Color`, `Ambient` (`glm::vec3`, `[0,1]` range). |
| `Mesh3D` | Owns `std::vector<Triangle3D>`; `Triangle3D` holds vertices, normals, UVs + `Material3D`. |

Models are loaded from `.obj`/`.mtl` via **tinyobjloader** (see `assets/3d/teapot.obj`).

---

## Lua Table (`Render3D`)

| Function | Purpose |
|---|---|
| `Render3D.Activate()` / `Render3D.Deactivate()` | Enable/disable the viewport. |
| `Render3D.IsActive()` → bool | Query state. |
| `Render3D.LoadModel(virtualPath)` | Load an `.obj` model from the VFS. |
| `Render3D.SetViewport(x, y, w, h)` | Screen-space viewport rect. |
| `Render3D.SetModelRotation(pitch, yaw, roll)` | Model orientation. |
| `Render3D.SetModelScale(scale)` | Uniform scale. |
| `Render3D.SetModelPosition(x, y, z)` | Model translation. |
| `Render3D.SetCamera(eyeX, eyeY, eyeZ, targetX, targetY, targetZ [, fovDeg])` | Camera placement. |
| `Render3D.SetLightDirection(x, y, z)` | Directional light vector. |
| `Render3D.SetLightColor(r, g, b)` | Light color, `[0, 1]`. |
| `Render3D.SetAmbientColor(r, g, b)` | Ambient color, `[0, 1]`. |
| `Render3D.SetClearColor(r, g, b, a)` | Clear color, `[0, 255]`; `a = 0` disables clear. |

