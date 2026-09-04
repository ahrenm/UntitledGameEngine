# Scene System

Scenes are gameplay containers owned exclusively by `SceneManagerLayer` (load order 4.05,
between `SDLLayer` 4.0 and `PhysicsLayer` 4.1). At most **one** `SceneObject` is live at a
time; loading a new scene automatically tears down the previous one.

Related: [FrameLoop.md](../architecture/FrameLoop.md) · [Physics.md](Physics.md) ·
[InputEvents.md](InputEvents.md) · [LuaIntegration.md](LuaIntegration.md)
Key files:
[`SceneManagerLayer.h`](../../source/UGECore/Public/Layers/SceneManagerLayer.h),
[`SceneObject.h`](../../source/UGECore/Public/GameClasses/SceneObject.h)

---

## Lifecycle

A scene's lifetime spans from its construction (inside `SceneManagerLayer::LoadScene()`, which
fetches the renderer/window from `SDLLayer` via `ServiceLocator`) until the next scene is loaded
or the application shuts down.

Within the frame loop, `SceneManagerLayer`:

- calls `Scene::Update()` during the **Update** pass (after `SDLLayer` polls input, before
  `PhysicsLayer` steps), and
- calls `Scene::Draw(float deltaTime)` during the **Draw** pass (after the `SDLLayer`
  background blit, before the `PhysicsLayer` debug overlay).

So scene geometry renders **beneath** any physics debug or RmlUi overlay.

**Scene teardown** (including physics-world shutdown ordering) is driven explicitly by
`UGEApplication`'s destructor while all layers are still alive. This guarantees a scene's
`PhysicsBodyHandle`s release their bodies before `PhysicsLayer::ShutdownPhysics()` — see the
ownership contract in [Physics.md](Physics.md).

---

## Camera

The 2-D `Camera2D` remains an `SDLLayer` concern (`SetCamera` / `GetCamera` / `MoveCamera`);
`SceneManagerLayer` resets it to default on each scene load.

---

## Optional Mixins

- **`IEventHandler`** — inherit to receive SDL events (dispatched via
  `SceneManagerLayer::HandleEvent()`). See [InputEvents.md](InputEvents.md).
- **`IScriptableObject`** — inherit to expose a Lua API that is automatically
  registered/unregistered alongside the scene's lifetime. See
  [LuaIntegration.md](LuaIntegration.md).

Access the currently active scene via `SceneManagerLayer::ActiveScene()` (returns
`SceneObject*`, or `nullptr`).

---

## Authoring a Scene

Derive from `SceneObject`, implement `Update()` + `Draw(float deltaTime)` (+ optional
`HandleEvent`), and register in-header with `REGISTER_SCENE("scene-name", Type)`. Load via
`Scene.Load("scene-name")` (Lua) or `SceneManagerLayer::LoadScene()` (C++). Previous scene
teardown is automatic; if the scene implements `IScriptableObject`, Lua
registration/unregistration is automatic too.

Example reference:
[`PlatformerScene.h`](../../source/GamePlugins/DemoGame/Platformer/PlatformerScene.h).

