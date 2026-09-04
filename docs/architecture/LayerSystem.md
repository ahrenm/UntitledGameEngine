# Layer System

All engine subsystems derive from **`AppLayer`** and are owned by `UGEApplication`. Layers
are wired together at runtime through the non-owning **`ServiceLocator`** and discovered via
static **registries**. This document covers the mechanics shared by every layer.

Related: [Overview.md](Overview.md) · [FrameLoop.md](FrameLoop.md) ·
Key files: [`AppLayer.h`](../../source/UGECore/Public/Layers/AppLayer.h),
[`ServiceLocator.h`](../../source/UGECore/Public/ServiceLocator.h),
[`LayerRegistry.h`](../../source/UGECore/Public/LayerRegistry.h)

---

## AppLayer

Every subsystem derives from **`AppLayer`** (`Layers/AppLayer.h`) and is stored in
`UGEApplication::m_layers`. The base class defines the frame-loop hooks `Update()` and
`Draw(float deltaTime)` plus a protected `Log(std::string Msg)` helper that routes a message
to `LoggingLayer` via `ServiceLocator` (no-ops gracefully if logging is not yet registered).
Use `Log()` inside any layer implementation instead of touching `ServiceLocator` directly.

---

## pushLayer & Registration

`pushLayer<T>()` is the single idiom to add a layer. It:

1. takes ownership of the layer (stored in `m_layers`),
2. calls `Layer->RegisterWithServiceLocator()` (a virtual hook on `AppLayer`), and
3. returns a raw **observer** pointer.

Registration is **fully delegated** to `RegisterWithServiceLocator()`; `pushLayer` itself does
**not** call `ServiceLocator::Provide`. Every concrete layer's `RegisterWithServiceLocator()`
calls `ServiceLocator::Provide(this)` to register itself under its concrete type.

---

## ServiceLocator

A type-safe, **non-owning** service registry. Lifetime is managed by `UGEApplication`'s
`m_layers` vector — never store raw service pointers longer than the owning layer lives.

| Method | Purpose |
|---|---|
| `Provide<T>(T*)` | Register an instance (may register multiple; `Get<T>()` returns the most recent). |
| `Get<T>()` | Fetch a service that is guaranteed present. |
| `TryGet<T>()` | Fetch a service that may be absent (returns `nullptr`). |
| `Has<T>()` | Test presence, returns `bool`. |
| `Remove<T>()` | Single-type deregister. |
| `Clear()` | Deregister all instances. |

Inside `Scene` and `ViewModel` derivatives prefer the `GameObjectBase` accessors
(`GetSDLLayer()`, `GetDataLayer()`, …) over calling `ServiceLocator::TryGet<T>()` directly —
see [InputEvents.md](../systems/InputEvents.md#gameobjectbase-accessors).

---

## Static Registry Macros

Three singleton factory registries decouple construction from wiring:

| Macro | Registry | Placed in |
|---|---|---|
| `REGISTER_LAYER(name, loadOrder, Type)` | `LayerRegistry` | Layer class header (`public:`) |
| `REGISTER_SCENE(name, Type)` | `SceneRegistry` | Scene class header (`public:`) |
| `REGISTER_VIEWMODEL(name, Type)` | `ViewModelRegistry` | ViewModel class header (`public:`) |

Place the macro in a **`public:`** section of the class **header** so the static-inline
registration runs at DLL load. Factories are resolved through the registries — avoid manual
wiring in `main()` or `UGEApplication::Create()`.

Example references:
[`Render3DLayer.h`](../../source/GamePlugins/TeaPot3D/Render3DLayer/Render3DLayer.h),
[`PlatformerScene.h`](../../source/GamePlugins/DemoGame/Platformer/PlatformerScene.h),
[`LuaConsoleViewModel.h`](../../source/GamePlugins/DemoGame/LuaConsoleViewModel.h).

---

## Factory Pattern with `std::expected`

Subsystem constructors are private; construction goes through a static `::Create()` that
propagates errors with `return std::unexpected(...)`. All layers return
`std::expected<std::unique_ptr<ConcreteLayer>, std::string>` except `SDLLayer::Create()`
which returns `std::expected<std::unique_ptr<AppLayer>, std::string>`.

`RmlUILayer::Create()` takes no SDL parameters and fetches `SDLLayer` via `ServiceLocator`.
Both `SDLLayer` and `RmlUILayer` wire their log sinks inside `Create()` by fetching
`LoggingLayer` from `ServiceLocator`.

---

## Log Routing

`LoggingLayer` owns the log buffer. `SDLLayer` and `RmlUILayer` wire their log sinks
automatically in `Create()` via `ServiceLocator::TryGet<LoggingLayer>()`. Log wiring to the
UI is handled inside `LuaConsoleViewModel::RegisterWith()` — it fetches
`ServiceLocator::Get<LoggingLayer>()` and calls `BindLogData(Log.Lines())`, assigning the
result to `Log.onLog`.

