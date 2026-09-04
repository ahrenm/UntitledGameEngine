# UI System

The UI stack is built on **RmlUi**, driven by `RmlUILayer` (load order 6). Documents
(`.rml`/`.rcss`) live under `assets/ui/` and are backed by C++ **ViewModels** that expose
data-models. A reusable **window framework** (`UIWindows.dll`) layers draggable desktop-style
windows on top.

Related: [InputEvents.md](InputEvents.md) · [DataStore.md](DataStore.md) ·
[AssetSystem.md](AssetSystem.md)
Key files:
[`RmlUILayer.h`](../../source/UGECore/Public/Layers/RmlUILayer.h),
[`ViewModel.h`](../../source/UGECore/Public/GameClasses/ViewModel.h),
[`WindowViewModel.h`](../../source/GamePlugins/UIWindows/WindowViewModel.h)

---

## Documents & ViewModels

`RmlUILayer::LoadDocument()` queues a path that is processed on the next `Update()` (deferred);
any previously loaded page and its ViewModels are torn down first. Only **one page** may be
live at a time. The loader automatically scans the file for `data-model="..."` attributes,
instantiates the registered `ViewModel` via `ViewModelRegistry`, and calls
`RegisterWith(context, modelName)` **before** RmlUi parses the markup. No manual
`RegisterWith()` call is needed in `UGEApplication.cpp`.

`ViewModel` implementations are **not** layers. They self-register with
`REGISTER_VIEWMODEL("model-name", Type)` in the header (`public:` section), are discovered from
`data-model="..."` during `RmlUILayer::LoadDocument()`, and are created/destroyed with the page
lifecycle. Use `ServiceLocator::Provide(this)` inside `RegisterWith()` only when global lookup
is intentionally needed.

Example references:
[`LuaConsoleViewModel.h`](../../source/GamePlugins/DemoGame/LuaConsoleViewModel.h)
(window-style VM, input + log binding) and
[`PlatformerHudViewModel.h`](../../source/GamePlugins/DemoGame/Platformer/PlatformerHudViewModel.h)
(HUD score binding).

### ViewModel Lifecycle Pattern

Subclass `ViewModel`, implement `RegisterWith(Rml::Context*, const char*)`, and register
in-header with `REGISTER_VIEWMODEL(...)`. `RmlUILayer` owns page + fragment view-models in one
container and tears them down on page unload/reload. Register with `ServiceLocator` only when
cross-system lookup is required.

---

## Fragments

Elements with `data-fragment="assets/ui/frag_foo.rml"` are found after load; the fragment file
is read from PhysFS, its `data-model` attributes are wired to ViewModels, and the markup is
injected via `SetInnerRML`. Use this to split large documents into reusable fragments (e.g.
`assets/ui/frag_luaConsole.rml`).

---

## Window Framework (UIWindows.dll)

A reusable framework for building draggable, resizable UI windows. The architecture separates
**runtime state** (position, visibility, z-order) from **widget markup** — state changes are
stored transient-only and updates are driven reactively via RmlUi data-binding.

### WindowViewModel

`WindowViewModel` (`source/GamePlugins/UIWindows/WindowViewModel.h`; include root
`source/GamePlugins/UIWindows/`) is the base class for all window data-models. It derives from
`ViewModel` and manages position, visibility, and z-order, all stored under
`uiRuntime.<activeDoc>.<windowTag>.*` transient keys.

**Public API**: `Show()` / `Hide()` / `IsVisible()`, `SetPosition(Left, Top)` / `SetLeft()` /
`SetTop()`, `SetZOrder()` / `RaiseWindowToFront()`, `LoadRuntimeState()` / `SaveRuntimeState()`,
`ClampToViewport()`. Transient-backed bindings are created during initialization for `posX`,
`posY`, `z`, and `visible`; those callbacks update local members and call
`OnRuntimeStateChanged()`.

**Shared protected helpers** for derived windows:

| Helper | Purpose |
|---|---|
| `BindCommonWindowVars(Ctor)` | Binds `panel_left` / `panel_top` / `panel_z` / `panel_visible`. |
| `BindCloseEvent(Ctor [, EventName])` | Binds a close callback that calls `Hide()`. |
| `SyncWindowModel(Model [, ExtraDirtyVars])` | Stores model handle and dirties common + extra vars. |
| `HandleWindowDragEvent(Event, DragHandleId)` | Reusable drag-to-move behavior. |
| `setTransient(Suffix, DataValue)` | Write runtime (transient-meta) keys under `m_runtimePrefix + Suffix`. |

Include as `#include <WindowViewModel.h>` (requires linking against `UIWindows`).

### Runtime State Contract

`RmlUILayer` writes `uiRuntime.activeDoc` to the current page slug (normalized from file path,
e.g. `"platformer"` from `assets/ui/platformer.rml`). `WindowViewModel` loads/stores per-window
state under:

| Key | Meaning |
|---|---|
| `uiRuntime.<activeDoc>.<windowTag>.posX` / `.posY` | Position in reference pixels. |
| `uiRuntime.<activeDoc>.<windowTag>.z` | Z-order (stacking) index. |
| `uiRuntime.<activeDoc>.<windowTag>.visible` | Visibility flag (`1` = visible, `0` = hidden). |
| `uiRuntime.<activeDoc>.currentWinZMax` | Shared per-page z-order counter. |

### Derived Constructor Pattern

Keep constructors minimal and pass a static tag to base. Apply optional window-specific
defaults via transient writes only.

```cpp
MyWindowViewModel::MyWindowViewModel()
    : WindowViewModel("my-window")
{
    // Optional default runtime values:
    // setTransient(".visible", DataValue{1});
}
```

### Derived RegisterWith Pattern

Bind common panel fields through base helpers, bind window-specific fields locally, then
synchronize via `SyncWindowModel`.

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

### Constructor Restore Pattern

The base constructor calls `LoadRuntimeState()`, seeds transient via `SaveRuntimeState()`, then
establishes bindings. This restores position/z/visibility and keeps runtime state synchronized
for all derived windows.

**Example reference**:
[`LuaConsoleViewModel.h`](../../source/GamePlugins/DemoGame/LuaConsoleViewModel.h) shows a
complete derived implementation with extra data bindings, submit/close callbacks, and drag
handling.

