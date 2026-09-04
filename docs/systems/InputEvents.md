# Input & Events

SDL events flow from `SDLLayer` through the layer stack in **reverse push order**, with each
handler able to consume an event to stop propagation.

Related: [SceneSystem.md](SceneSystem.md) · [UISystem.md](UISystem.md) ·
[LayerSystem.md](../architecture/LayerSystem.md)
Key files:
[`IEventHandler.h`](../../source/UGECore/Public/IEventHandler.h),
[`GameObjectBase.h`](../../source/UGECore/Public/GameClasses/GameObjectBase.h)

---

## Propagation Model

`SDLLayer::Update()` polls SDL events and forwards each to `Application::dispatchEvent()` via
the registered event handler. `dispatchEvent` walks `m_layers` in **reverse** push order,
`dynamic_cast`s each layer to `IEventHandler*`, and calls `HandleEvent(SDL_Event&)`:

- Returning `true` **consumes** the event and stops propagation.
- Returning `false` **continues** propagation.

`RmlUILayer::HandleEvent()` first dispatches to its ViewModels in reverse order (calling
`ViewModel::HandleEvent()`), then forwards unconsumed events to `RmlSDL::InputEventHandler` and
returns `!result` — `true` (stop) when RmlUi consumed the event (e.g. a focused `<input>`
captured a keypress), `false` (continue) when RmlUi left it unhandled.

---

## Opting In

- **A new layer**: inherit `IEventHandler` alongside `AppLayer`.
- **A ViewModel**: override `ViewModel::HandleEvent()` (default returns `false`).
- **A scene**: inherit `IEventHandler`; events are dispatched via
  `SceneManagerLayer::HandleEvent()`.

---

## Ownership Pattern

Keep domain-specific input handling with the **owning** scene or view-model (`HandleEvent`),
while `LuaLayer::HandleEvent()` remains **non-consuming** (it observes input without stopping
propagation).

---

## GameObjectBase Accessors

Both `Scene` and `ViewModel` inherit `GameObjectBase` (`<GameClasses/GameObjectBase.h>`). Inside
any derived class use the protected helpers instead of calling `ServiceLocator::TryGet<T>()`
directly:

`GetLoggingLayer()`, `GetPhysFSLayer()`, `GetDataLayer()`, `GetLuaLayer()`, `GetSDLLayer()`,
`GetSceneManagerLayer()`, `GetUILayer()`.

All return raw **non-owning** pointers (null if the layer has not yet been registered) — guard
with a null-check when availability is uncertain.

`Render3DLayer` is not a UGECore type and has **no** accessor here; fetch it via
`ServiceLocator::TryGet<Render3DLayer>()` from code that links against `TeaPot3D` (see
[Rendering3D.md](Rendering3D.md)).

