#pragma once
#include <GameClasses/GameObjectBase.h>
#include "../SceneRegistry.h"
#include "../Camera2D.h"
#include <SDL3/SDL.h>
#include <optional>

class Renderer2D;

// ── Scene ─────────────────────────────────────────────────────────────────────
// Abstract base class for game scenes.
// Derive in Game.dll, self-register with REGISTER_SCENE, and load at runtime
// via Scene.Load("scene-name") from Lua or SceneManagerLayer::LoadScene() from C++.
//
// SceneManagerLayer owns exactly one active scene at a time.  Loading a new scene
// automatically tears down the previous one.
//
// Coordinate system
// -----------------
// All game-object positions are expressed in Y-up world pixels.
// (0, 0) = bottom-left of the reference area; X right, Y up.
// Use the protected WorldRect() / WorldToScreen() / ScreenToWorld() helpers
// to convert to screen-space rects before issuing Renderer2D draw calls.
// The default camera (FlipY=true, zero offset, zoom=1) makes this transparent —
// if you never set m_cameraOverride the scene behaves exactly as if it were
// rendering in reference-pixel screen space, but Y is flipped.
//
// Camera override
// ---------------
// Set m_cameraOverride in the derived constructor (or each Update() for
// per-frame scrolling) to provide a scene-local camera instead of the global
// SDLLayer camera.  Clear it with m_cameraOverride.reset() to restore the
// SDLLayer default.
//
// Event dispatch: SceneManagerLayer calls HandleEvent() for every SDL event
// during the application dispatch chain, before the event reaches lower layers.
// Return true to consume the event and prevent further propagation;
// return false to let it continue.
//
// If a derived scene also inherits IScriptableObject, SceneManagerLayer will
// automatically call LuaLayer::Register() on load and Unregister() on unload.
class SceneObject : public GameObjectBase
{
public:
    SceneObject(Renderer2D* Renderer, SDL_Window* Window)
        : m_renderer2D(Renderer), m_window(Window) {}

    virtual ~SceneObject() = default;

    // Called once per frame before Draw() — use for logic and state updates.
    virtual void Update() {}

    // Called once per frame during the render pass — use for Renderer2D draw calls.
    // DeltaTime is elapsed seconds since the previous Draw() call.
    virtual void Draw(float /*deltaTime*/) {}

    // Called for every SDL event before the event enters the application dispatch
    // chain.  Return true to consume the event; return false to forward it.
    virtual bool HandleEvent(SDL_Event& /*Event*/) { return false; }

protected:
    Renderer2D*   m_renderer2D = nullptr;  // non-owning; lifetime is SDLLayer's
    SDL_Window*   m_window   = nullptr;  // non-owning; lifetime is SDLLayer's

    // Optional per-scene camera override.  When set, WorldRect() and friends use
    // this camera instead of SDLLayer's global camera.  Reset by SDLLayer on each
    // new scene load.  Modify per-frame in Update() for smooth scrolling.
    std::optional<Camera2D> m_cameraOverride;

    // ── Coordinate helpers ────────────────────────────────────────────────────
    // These helpers apply the effective camera transform (override if set,
    // else SDLLayer's global camera) to convert Y-up world positions to the
    // screen-space SDL_FRect / SDL_FPoint expected by Renderer2D draw calls.

    // Convert a Y-up world AABB (bottom-left origin) to a screen-space SDL_FRect.
    // wx, wy = bottom-left corner in world space; w, h = size.
    [[nodiscard]] SDL_FRect WorldRect(float Wx, float Wy, float W, float H) const
    {
        const Camera2D cam = getEffectiveCamera();
        if (auto* sdl = GetSDLLayer())
            return cam.WorldRect(Wx, Wy, W, H, static_cast<float>(sdl->RefHeight()));
        return { Wx, Wy, W, H }; // fallback: no SDLLayer
    }

    // Convert a Y-up world point to a screen-space SDL_FPoint.
    [[nodiscard]] SDL_FPoint WorldToScreen(float Wx, float Wy) const
    {
        const Camera2D cam = getEffectiveCamera();
        if (auto* sdl = GetSDLLayer())
            return cam.WorldToScreen(Wx, Wy, static_cast<float>(sdl->RefHeight()));
        return { Wx, Wy };
    }

    // Convert a screen-space point back to a Y-up world position.
    [[nodiscard]] SDL_FPoint ScreenToWorld(float Sx, float Sy) const
    {
        const Camera2D cam = getEffectiveCamera();
        if (auto* sdl = GetSDLLayer())
            return cam.ScreenToWorld(Sx, Sy, static_cast<float>(sdl->RefHeight()));
        return { Sx, Sy };
    }

private:
    [[nodiscard]] Camera2D getEffectiveCamera() const
    {
        if (m_cameraOverride.has_value()) return *m_cameraOverride;
        if (auto* sdl = GetSDLLayer())    return sdl->GetCamera();
        return Camera2D{};
    }
};

