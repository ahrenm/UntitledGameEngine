#pragma once
#include <GameClasses/GameObjectBase.h>
#include "../SceneRegistry.h"
#include <SDL3/SDL.h>

// ── Scene ─────────────────────────────────────────────────────────────────────
// Abstract base class for game scenes.
// Derive in Game.dll, self-register with REGISTER_SCENE, and load at runtime
// via SDL.LoadScene("scene-name") from Lua or SDLLayer::LoadScene() from C++.
//
// SDLLayer owns exactly one active scene at a time.  Loading a new scene
// automatically tears down the previous one.
//
// Event dispatch: SDLLayer calls HandleEvent() for every SDL event during its
// PollEvents pass, before the event is forwarded into the application dispatch
// chain (IEventHandler layers / RmlUILayer).  Return true to consume the event
// and prevent further propagation; return false to let it continue.
//
// If a derived scene also inherits ScriptableObject, SDLLayer will automatically
// call ScriptAPILayer::Register() on load and ScriptAPILayer::Unregister() on
// unload, exactly like the dynamic ScriptableObject pattern.
class SceneObject : public GameObjectBase
{
public:
    SceneObject(SDL_Renderer* Renderer, SDL_Window* Window)
        : m_renderer(Renderer), m_window(Window) {}

    virtual ~SceneObject() = default;

    // Called once per frame before Tick() — use for logic and state updates.
    virtual void Update() {}

    // Called once per frame during the render pass — use for SDL draw calls.
    // DeltaTime is elapsed seconds since the previous Tick() call.
    virtual void Tick(float /*deltaTime*/) {}

    // Called for every SDL event before the event enters the application dispatch
    // chain.  Return true to consume the event; return false to forward it.
    virtual bool HandleEvent(SDL_Event& /*Event*/) { return false; }

protected:
    SDL_Renderer* m_renderer = nullptr;  // non-owning; lifetime is SDLLayer's
    SDL_Window*   m_window   = nullptr;  // non-owning; lifetime is SDLLayer's
};

