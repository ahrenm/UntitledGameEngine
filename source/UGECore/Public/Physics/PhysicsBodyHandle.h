#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <memory>

// Forward declaration — full type only needed in PhysicsLayer.cpp where all
// PhysicsBodyHandle method bodies are defined.
class PhysicsLayer;

// ── PhysicsBodyHandle ─────────────────────────────────────────────────────────
// RAII handle to a single Box2D body.  Destroys the body when it goes out of
// scope by calling PhysicsLayer::releaseBody().
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │ OWNERSHIP CONTRACT                                                      │
// │                                                                         │
// │  A PhysicsBodyHandle MUST be stored as a non-static member variable of  │
// │  the scene (or a scene-owned object such as PlatformerCharacter or      │
// │  Tile) that created it.  It MUST NOT be stored in:                      │
// │    • global or static variables                                         │
// │    • UGEDataLayer / AppState                                            │
// │    • any container with a lifetime longer than the active scene         │
// │                                                                         │
// │  SDLLayer::UnloadScene() destroys the scene (and all its handles) THEN  │
// │  calls PhysicsLayer::ShutdownPhysics().  Handles that outlive their     │
// │  scene call releaseBody() against an already-destroyed world.           │
// │                                                                         │
// │  Moving handles into std::vector<PhysicsBodyHandle> is safe — the move  │
// │  constructor transfers ownership correctly.                             │
// └─────────────────────────────────────────────────────────────────────────┘
class PhysicsBodyHandle
{
public:
    PhysicsBodyHandle() = default;
    ~PhysicsBodyHandle();

    // Non-copyable; move transfers ownership.
    PhysicsBodyHandle(const PhysicsBodyHandle&)            = delete;
    PhysicsBodyHandle& operator=(const PhysicsBodyHandle&) = delete;
    PhysicsBodyHandle(PhysicsBodyHandle&&) noexcept;
    PhysicsBodyHandle& operator=(PhysicsBodyHandle&&) noexcept;

    // Returns false if the handle was default-constructed, moved-from, or
    // belongs to a world that has been shut down.  All other methods are
    // guarded by this check and silently no-op on an invalid handle.
    [[nodiscard]] bool IsValid() const { return m_layer != nullptr && m_id != 0; }

    // ── Position (world pixels, Y-up, bottom-left origin) ─────────────────────
    // X, Y = bottom-left corner of the body's AABB.
    void       SetPosition(float Wx, float Wy);
    SDL_FPoint GetPosition() const;

    // ── Velocity (world pixels per second) ────────────────────────────────────
    void       SetLinearVelocity(float Vx, float Vy);
    SDL_FPoint GetLinearVelocity() const;

    // ── Mass (Box2D units: kg) ─────────────────────────────────────────────────
    // Use with ApplyImpulse to compute a mass-scaled velocity change:
    //   impulse (N·s) = GetMass() × Δv (m/s) = GetMass() × Δv_px_s / PPM
    [[nodiscard]] float GetMass() const;

    // Applies a linear impulse (N·s, Box2D units) at the body's centre.
    // To achieve an exact pixel-space velocity change ΔvPxS in one axis:
    //   ApplyImpulse( GetMass() * ΔvPxS / PPM,  0.0f )
    // Correct usage produces true physics-engine momentum (mass-scaled), unlike
    // SetLinearVelocity which bypasses dynamics entirely.
    void ApplyImpulse(float IxNs, float IyNs);

    void SetEnabled(bool Enabled);

    // ── Grounded query ────────────────────────────────────────────────────────
    // Returns true if the body currently has a contact whose normal is at least
    // MinNormalY away from horizontal (default 0.7 ≈ 45°).  Queries live contact
    // manifolds via b2Body_GetContactData — no event flags required.
    // Call in Tick() after PhysicsLayer::Update() has stepped the world.
    [[nodiscard]] bool IsGrounded(float MinNormalY = 0.7f) const;

private:
    friend class PhysicsLayer;
    explicit PhysicsBodyHandle(uint32_t Id, PhysicsLayer* Layer)
        : m_id(Id), m_layer(Layer) {}

    uint32_t             m_id         = 0;
    PhysicsLayer*        m_layer      = nullptr;
    // Weak reference to PhysicsLayer's alive-token.
    // Expires automatically when PhysicsLayer is destroyed, allowing the destructor
    // to skip releaseBody() safely rather than dereferencing a dangling pointer.
    std::weak_ptr<bool>  m_aliveToken;
};

