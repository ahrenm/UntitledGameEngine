#pragma once
#include <../../UGECore/Public/GameClasses/SceneObject.h>
#include <Physics/PhysicsBodyHandle.h>
#include <IScriptableObject.h>
#include "CharacterAnimationController.h"

class UGEDataLayer;

// ── PlatformerCharacter ───────────────────────────────────────────────────────
// Owns the player's Box2D dynamic body, animation, and input handling.
// Physics is fully driven by Box2D — no manual velocity integration.
// Grounded state is maintained via AddGroundContact()/RemoveGroundContact()
// called from PlatformerScene's contact callback.
class PlatformerCharacter : public SceneObject, public IScriptableObject
{
public:
    static constexpr float PLAYER_W           = 100.0f;
    static constexpr float PLAYER_H           = 100.0f;
    static constexpr float COLLISION_W        = PLAYER_W * 0.85f;
    static constexpr float COLLISION_X_OFFSET = (PLAYER_W - COLLISION_W) / 2.0f;

    // DatasetKey = TOML dataset for sprite / animation config
    PlatformerCharacter(Renderer2D* Renderer, SDL_Window* Window,
                        const char* DatasetKey);

    void Update()                      override;
    void Draw(float DeltaTime)         override;
    bool HandleEvent(SDL_Event& Event) override;

    void Jump();
    void RegisterObject(sol::state& Lua)   override;
    void UnregisterObject(sol::state& Lua) override;

    // ── Ground state (polled from Box2D contact manifolds each frame) ─────────
    [[nodiscard]] bool       IsGrounded()      const { return m_body.IsGrounded(); }
    // World-pixel bottom-left corner of the collision AABB (same coord as Box2D body origin).
    [[nodiscard]] SDL_FPoint GetBodyPosition() const { return m_body.GetPosition(); }

private:
    static constexpr float REF_W         = 1600.0f;
    static constexpr float GROUND_TILE_H = 160.0f;   // top edge of the ground tile layer (px)
    // Foot sensor is 10 px tall (0.05 m half-height × 2 × 100 PPM), centred flush with
    // the body bottom.  Spawn 15 px above the tile top so the sensor clears the tile
    // interior on the first frame and avoids a spurious grounded contact event.
    static constexpr float SPAWN_Y       = GROUND_TILE_H + 50.0f;

    UGEDataLayer* m_dataLayer = nullptr;

    // ── Character identity ────────────────────────────────────────────────────
    std::string     m_characterName;
    DataBinding     m_characterNameBinding;

    // ── Physics tunables (bound to transient) ─────────────────────────────────
    float m_readSpeed       = 240.0f;  // px/s  (TAG_SPEED)
    float m_readJumpHeight  = 280.0f;  // px    (TAG_JUMP_HEIGHT)
    float m_readGravityY    = -9.81f;  // m/s²  (TAG_PHYSICS_GRAVITY_Y)
    float m_readPPM         = 100.0f;  // px/m  (TAG_PHYSICS_PPM)

    DataBinding m_speedBinding;
    DataBinding m_jumpHeightBinding;
    DataBinding m_gravYBinding;
    DataBinding m_ppmBinding;

    // ── Box2D body ────────────────────────────────────────────────────────────
    PhysicsBodyHandle m_body;          // owns the dynamic body (destroyed with scene)

    // ── Render position (read from body each tick) ────────────────────────────
    float m_playerX = (REF_W - PLAYER_W) / 2.0f;  // sprite left edge
    float m_playerY = SPAWN_Y;                      // sprite bottom edge (Y-up)

    // ── Animation controller ──────────────────────────────────────────────────
    CharacterAnimationController m_animController;

    bool m_facingLeft = false;

    // ── Input flags ───────────────────────────────────────────────────────────
    bool m_moveLeft    = false;
    bool m_moveRight   = false;
    bool m_jumpPressed = false;
};
