#pragma once
#include <string>

// ── AppLayer ──────────────────────────────────────────────────────────────────
// Base class for application layers. Layers are stored in Application and
// iterated each frame: Update() is called first (logic/state), then Draw()
// (rendering/presentation). Override either or both as needed.
class AppLayer
{
public:
    virtual ~AppLayer() = default;

    // Called once per frame before Draw(). Use for logic, input processing,
    // and state updates.
    virtual void Update() {}

    // Called once per frame after Update(). Use for rendering and presentation.
    // DeltaTime is elapsed seconds since the previous Draw() call.
    virtual void Draw(float /*deltaTime*/) {}

      // Called after the layer is pushed into the application's layer stack.
    // Override in derived classes to register the concrete type with ServiceLocator.
    // Base implementation does nothing (only base AppLayer type is registered).
    virtual void RegisterWithServiceLocator() {}

protected:
    // Route a message to LoggingLayer.  No-ops gracefully if LoggingLayer has
    // not yet been registered (e.g. during very early construction).
    void Log(std::string Msg) const;
};
