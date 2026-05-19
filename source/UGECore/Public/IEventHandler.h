#pragma once
#include <SDL3/SDL.h>

// ── IEventHandler ─────────────────────────────────────────────────────────────
// Opt-in mixin for layers and ViewModels that want to participate in event
// propagation.  Inherit alongside AppLayer (or ViewModel) to opt in:
//
//   class MyLayer : public AppLayer, public IEventHandler { ... };
//
// Application::DispatchEvent() iterates m_layers in REVERSE order and calls
// HandleEvent() on every layer that implements this interface; the first layer
// to return true consumes the event and stops propagation.
//
// RmlUILayer::HandleEvent() does the same for ViewModels owned by each open
// document before forwarding unconsumed events to RmlUi itself.
class IEventHandler
{
public:
    virtual ~IEventHandler() = default;

    // Return true  → event is consumed; propagation stops.
    // Return false → event continues to the next handler.
    virtual bool HandleEvent(SDL_Event& Event) = 0;
};

