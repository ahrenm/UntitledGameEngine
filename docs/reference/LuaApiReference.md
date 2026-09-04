# Lua API Reference

Consolidated listing of every Lua-exposed table. All tables are registered by their owning
layer at startup (via the registration pass in `UGEApplication::Create()`).

Related: [LuaIntegration.md](../systems/LuaIntegration.md) ·
[Physics.md](../systems/Physics.md) · [Rendering3D.md](../systems/Rendering3D.md)

---

## `SDL` (SDLLayer)

| Function | Purpose |
|---|---|
| `SDL.SetBackground(virtualPath)` | Set the background texture. |
| `SDL.SetCamera(worldX, worldY)` | Position the 2-D camera. |
| `SDL.MoveCamera(dx, dy)` | Pan the 2-D camera. |
| `SDL.ShowCollision()` | Toggle the collision debug overlay and log the state. |

## `Scene` (SceneManagerLayer)

| Function | Purpose |
|---|---|
| `Scene.Load(sceneName)` | Load a registered scene by name. |

## `UI` (RmlUILayer)

| Function | Purpose |
|---|---|
| `UI.LoadFont(virtualPath)` | Load a font from the VFS. |
| `UI.LoadDocument(virtualPath)` | Queue a UI document load. |

## `Data` (UGEDataLayer)

| Function | Purpose |
|---|---|
| `Data.Set(key, value)` | Type-safe set; coerces to the existing backing type or infers from the Lua value for new keys (new keys default to **transient** meta). |
| `Data.Get(key)` → value \| nil | Read via the value's `ToLua` converter. |
| `Data.Show()` | Log all store entries with their source label. |

## `UGE` (LuaLayer)

| Function | Purpose |
|---|---|
| `UGE.AddTickFunction(fn [, name])` → id | Register a per-frame tick function. |
| `UGE.RemoveTick(id)` | Unregister a tick by ID. |
| `UGE.RunScript(virtualPath)` | Execute a Lua file from the VFS. |
| `UGE.ShowTicking()` | Log active tick entries. |
| `UGE.GetTickId(name)` → id \| 0 | Look up a tick ID by name. |

## `Physics` (PhysicsLayer)

| Function | Purpose |
|---|---|
| `Physics.InitPhysics()` | Create the Box2D world (reads `physics.*` tags). |
| `Physics.ShutdownPhysics()` | Destroy the world. |
| `Physics.HasWorld()` → bool | Whether a world is live. |
| `Physics.GetBodyCount()` → int | Number of bodies. |
| `Physics.ShowCollision()` | Toggle the debug overlay. |
| `Physics.DebugDraw(on)` | Set the debug overlay explicitly. |
| `Physics.SetGravity(gx, gy)` | Sugar for writing `physics.gravity.*` (LIVE). |
| `Physics.SetPixelsPerMeter(ppm)` | Sugar for `physics.pixels_per_meter` (INIT-only; refuses while a world is live). |

See [Physics.md](../systems/Physics.md) for the `physics.*` tunable-parameter contract.

## `Render3D` (TeaPot3D plugin)

Documented in full in [Rendering3D.md](../systems/Rendering3D.md#lua-table-render3d).

