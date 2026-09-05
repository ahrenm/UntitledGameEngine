# Frame Loop

The game loop lives in `UGEApplication::Run()`. It runs a **30 FPS-capped loop** using
`SDL_GetTicksNS()` / `SDL_DelayNS()`.

Related: [LayerSystem.md](LayerSystem.md) · [Physics.md](../systems/Physics.md) ·
[SceneSystem.md](../systems/SceneSystem.md)

---

## Two-Phase Tick

Each game tick has two phases:

1. **Update phase** — `Update()` is called on each layer in **push order** (input → logic).
2. **Draw phase** — `m_sdlLayer->BeginFrame()` acquires the frame's `SDL_GPU` command buffer +
   swapchain texture and clears it to black, then `Draw(float deltaTime)` is called on each layer
   in **push order** (each records a render pass into the shared command buffer), then
   `m_sdlLayer->EndFrame()` submits the command buffer last.

If execution falls more than one full frame behind, `NextFrame` is reset to avoid catch-up
bursts.

---

## Per-Layer Ordering

Because layers tick in push (load) order, the following composition emerges each frame:

| Load order | Layer | Update phase | Draw phase |
|---|---|---|---|
| 4.0 | `SDLLayer` | polls SDL input, dispatches events | clears swapchain + submits (Begin/End) |
| 4.05 | `SceneManagerLayer` | `Scene::Update()` | `Scene::Draw(dt)` (beneath overlays) |
| 4.1 | `PhysicsLayer` | `b2World_Step` + contacts + state sync | debug AABB overlay only |
| 4.5 | `Render3DObjectLayer` (plugin) | — | 3D viewport composite |
| 5 | `LuaLayer` | runs tick functions | — |
| 6 | `RmlUILayer` | — | composites UI on top |

Net result: scene geometry renders **beneath** any physics debug overlay, which renders
beneath the RmlUi layer. Physics integrates the impulses set by scene logic *this* frame
before anything is drawn.

See [SceneSystem.md](../systems/SceneSystem.md) for how `SceneManagerLayer` drives the scene
within these phases, and [Physics.md](../systems/Physics.md) for the physics Update/Draw split.

