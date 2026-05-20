#include "PlatformerCharacter.h"
#include <PlatformerStateTags.h>
#include <Layers/UGEDataLayer.h>
#include <Layers/PhysicsLayer.h>
#include <cmath>
#include <sol/sol.hpp>

// ── Constructor ───────────────────────────────────────────────────────────────
PlatformerCharacter::PlatformerCharacter(SDL_Renderer* Renderer, SDL_Window* Window,
                                         const char* DatasetKey)
    : SceneObject(Renderer, Window)
    , m_animController(Renderer)
{
    m_dataLayer = GetDataLayer();

    // ── Transient bindings ────────────────────────────────────────────────────
    DATA_BIND_LOCAL_STRING(m_characterNameBinding, "character.name", m_characterName);
    DATA_BIND_LOCAL_FLOAT(m_speedBinding,      TAG_SPEED,            m_readSpeed,      Transient);
    DATA_BIND_LOCAL_FLOAT(m_jumpHeightBinding, TAG_JUMP_HEIGHT,       m_readJumpHeight, Transient);
    DATA_BIND_LOCAL_FLOAT(m_gravYBinding,      TAG_PHYSICS_GRAVITY_Y, m_readGravityY,   Transient);
    DATA_BIND_LOCAL_FLOAT(m_ppmBinding,        TAG_PHYSICS_PPM,       m_readPPM,        Transient);

    // ── Create Box2D dynamic body ─────────────────────────────────────────────
    if (auto* physics = GetPhysicsLayer())
    {
        const float spawnX = m_playerX + COLLISION_X_OFFSET;
        m_body = physics->CreateBody({
            .X             = spawnX,
            .Y             = SPAWN_Y,
            .W             = COLLISION_W,
            .H             = PLAYER_H,
            .Type          = BodyType::Dynamic,
            .Density       = 1.0f,
            .Friction      = 0.0f,
            .Restitution   = 0.0f,
            .UserData      = this,
            .FixedRotation = true
        });
    }

    // ── Animation controller ──────────────────────────────────────────────────
    m_animController.LoadAnimations(DatasetKey);
}


// ── Update ────────────────────────────────────────────────────────────────────
void PlatformerCharacter::Update()
{
    if (!m_body.IsValid()) return;

    const SDL_FPoint curVel = m_body.GetLinearVelocity();  // px/s
    const float      mass   = m_body.GetMass();            // kg  (Box2D units)
    const float      ppm    = m_readPPM;                   // px / m

    // ── Horizontal movement via physics impulse ───────────────────────────────
    // Desired horizontal velocity based on held keys.
    float targetVxPxS = 0.0f;
    if (m_moveLeft)  { targetVxPxS = -m_readSpeed; m_facingLeft = true;  }
    if (m_moveRight) { targetVxPxS = +m_readSpeed; m_facingLeft = false; }

    // impulse (N·s) = mass (kg) × Δv (m/s)
    // Δv (m/s) = (targetVx − currentVx) / ppm
    // Applying the full delta produces a physically correct instantaneous velocity
    // change that goes through Box2D's dynamics solver (friction, contacts, etc.)
    // while remaining as responsive as direct velocity-setting.
    const float hImpulse = mass * (targetVxPxS - curVel.x) / ppm;
    m_body.ApplyImpulse(hImpulse, 0.0f);

    // ── Jump ──────────────────────────────────────────────────────────────────
    if (m_jumpPressed && IsGrounded())
    {
        // v = sqrt(2 * |g| * h)   — g in m/s², h in m, result in m/s → px/s
        const float gravAbs   = std::abs(m_readGravityY);
        const float jumpH_m   = m_readJumpHeight / ppm;
        const float vJump_pxs = std::sqrt(2.0f * gravAbs * jumpH_m) * ppm;

        // Upward impulse (N·s) to reach target jump velocity from current vy.
        const float vImpulse = mass * (vJump_pxs - curVel.y) / ppm;
        m_body.ApplyImpulse(0.0f, vImpulse);
        m_animController.ResetAnimation("jump");
        m_animController.ResetAnimation("fall");
    }
    m_jumpPressed = false;

    // ── Read back position from body ──────────────────────────────────────────
    const SDL_FPoint bodyPos = m_body.GetPosition(); // bottom-left of collision AABB
    m_playerX = bodyPos.x - COLLISION_X_OFFSET;      // sprite left edge
    m_playerY = bodyPos.y;                            // sprite bottom edge

    // X boundaries and fall containment are now handled by Box2D static bodies
    // (m_leftWall / m_rightWall / m_worldFloor created in PlatformerScene).
    // Only a last-resort teleport remains in case of extreme physics edge cases.
    if (m_playerY < -(PLAYER_H * 3.0f))
    {
        m_body.SetPosition(m_playerX + COLLISION_X_OFFSET, SPAWN_Y);
        m_body.SetLinearVelocity(0.0f, 0.0f);
    }

    // ── Nameplate anchor: screen-space top-centre of sprite ───────────────────
    if (!m_characterName.empty() && m_dataLayer)
    {
        const SDL_FPoint Anchor = WorldToScreen(m_playerX + PLAYER_W / 2.0f,
                                                m_playerY + PLAYER_H);
        m_dataLayer->Store.Set(TAG_NAMEPLATE_X.data(), DataValue{Anchor.x});
        m_dataLayer->Store.Set(TAG_NAMEPLATE_Y.data(), DataValue{Anchor.y});
    }
}

// ── Draw ──────────────────────────────────────────────────────────────────────
void PlatformerCharacter::Draw(float DeltaTime)
{
    // PhysicsLayer::Update() has already stepped the world this frame —
    // vy and IsGrounded() are fully current.
    const float vy = m_body.IsValid() ? m_body.GetLinearVelocity().y : 0.0f;

    m_animController.Tick(DeltaTime, {
        .PlayerX    = m_playerX,
        .PlayerY    = m_playerY,
        .SpriteW    = PLAYER_W,
        .SpriteH    = PLAYER_H,
        .FacingLeft = m_facingLeft,
        .IsGrounded = IsGrounded(),
        .VelocityY  = vy,
        .IsMoving   = m_moveLeft || m_moveRight
    });
}

// ── HandleEvent ───────────────────────────────────────────────────────────────
bool PlatformerCharacter::HandleEvent(SDL_Event& Event)
{
    if (Event.type == SDL_EVENT_KEY_DOWN || Event.type == SDL_EVENT_KEY_UP)
    {
        const bool Pressed = (Event.type == SDL_EVENT_KEY_DOWN);
        switch (Event.key.scancode)
        {
            case SDL_SCANCODE_LEFT:  m_moveLeft    = Pressed; return false;
            case SDL_SCANCODE_RIGHT: m_moveRight   = Pressed; return false;
            case SDL_SCANCODE_SPACE:
                if (Pressed) m_jumpPressed = true;
                return false;
            default: break;
        }
    }
    return false;
}

// ── Jump (Lua) ────────────────────────────────────────────────────────────────
void PlatformerCharacter::Jump()
{
    if (IsGrounded()) m_jumpPressed = true;
}

// ── IScriptableObject ─────────────────────────────────────────────────────────
void PlatformerCharacter::RegisterObject(sol::state& Lua)
{
    auto plat = Lua.create_named_table("plat2d");
    plat.set_function("Jump",        [this]()         { Jump(); });
    plat.set_function("IsGrounded",  [this]() -> bool { return IsGrounded(); });
}

void PlatformerCharacter::UnregisterObject(sol::state& Lua)
{
    Lua["plat2d"] = sol::nil;
}
