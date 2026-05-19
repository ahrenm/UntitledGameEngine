#pragma once
#include <GameClasses/BoxCollisionRegistry.h>
#include <algorithm>
#include <cstdint>
#include <string_view>

// ── BoxCollision ──────────────────────────────────────────────────────────────
// Axis-Aligned Bounding Box (AABB) for 2-D collision testing.
//
// All coordinates are in the caller's chosen reference space (e.g. the scene's
// 1600 × 1200 reference-resolution pixels).
//
// Layout:  (X, Y) is the top-left corner; W and H extend right and downward.
//
// Auto-registration:
//   BoxCollision objects constructed with the value constructor (X, Y, W, H [, Label])
//   are automatically registered with BoxCollisionRegistry::Active() when one is set.
//   SDLLayer::InitCollisionHooks() activates the registry; boxes constructed before
//   that call (or when no SDLLayer is alive) are simply not registered.
//
//   Copy construction does NOT register — copies are used for temporary collision
//   math and should not appear in the debug overlay.
//   Move construction transfers the existing registration to the new address.
//
// Example:
//   // Member variable — auto-registers, auto-deregisters on destruction:
//   BoxCollision m_box { playerX, playerY, PLAYER_W, PLAYER_H, "Player" };
//
//   // Temporary for collision test — not registered:
//   BoxCollision future = m_box;  future.X += vx; future.Y += vy;
//   if (future.Intersects(tile)) { ... }
struct BoxCollision
{
    float X       = 0.0f;   // left edge
    float Y       = 0.0f;   // top edge
    float W       = 0.0f;   // width  (must be >= 0)
    float H       = 0.0f;   // height (must be >= 0)
    bool  Enabled = true;   // when false, all intersection tests return false
    void* UserData = nullptr; // caller-owned; set to the owning object (e.g. this) so
                              // grid query results can be mapped back to their parent.
                              // Not propagated by copy/move — set explicitly by the instantiator.

    // Default constructor — does NOT register (for stack / temp use).
    BoxCollision() = default;

    // Value constructor — auto-registers with the active BoxCollisionRegistry.
    BoxCollision(float X, float Y, float W, float H, std::string_view Label = "")
        : X(X), Y(Y), W(W), H(H)
    {
        if (auto* Reg = BoxCollisionRegistry::Active())
            m_registryId = Reg->RegisterDirect(this, Label);
    }

    // Copy — does NOT register (result used for temporary math only).
    BoxCollision(const BoxCollision& Other) noexcept
        : X(Other.X), Y(Other.Y), W(Other.W), H(Other.H), Enabled(Other.Enabled) {}

    BoxCollision& operator=(const BoxCollision& Other) noexcept
    {
        X = Other.X; Y = Other.Y; W = Other.W; H = Other.H; Enabled = Other.Enabled;
        return *this;
    }

    // Move — transfers the existing registration to the new address.
    BoxCollision(BoxCollision&& Other) noexcept
        : X(Other.X), Y(Other.Y), W(Other.W), H(Other.H), Enabled(Other.Enabled)
        , m_registryId(Other.m_registryId)
    {
        Other.m_registryId = 0;
        if (m_registryId != 0)
            if (auto* Reg = BoxCollisionRegistry::Active())
                Reg->UpdatePointer(m_registryId, this);
    }

    BoxCollision& operator=(BoxCollision&& Other) noexcept
    {
        if (this != &Other)
        {
            // Deregister self first.
            if (m_registryId != 0)
                if (auto* Reg = BoxCollisionRegistry::Active())
                    Reg->Deregister(m_registryId);

            X = Other.X; Y = Other.Y; W = Other.W; H = Other.H;
            m_registryId       = Other.m_registryId;
            Enabled            = Other.Enabled;
            Other.m_registryId = 0;

            if (m_registryId != 0)
                if (auto* Reg = BoxCollisionRegistry::Active())
                    Reg->UpdatePointer(m_registryId, this);
        }
        return *this;
    }

    ~BoxCollision()
    {
        if (m_registryId != 0)
            if (auto* Reg = BoxCollisionRegistry::Active())
                Reg->Deregister(m_registryId);
    }

    [[nodiscard]] bool     IsRegistered() const { return m_registryId != 0; }
    [[nodiscard]] uint32_t RegistryId()   const { return m_registryId; }

    // ── Edge accessors ─────────────────────────────────────────────────────────
    [[nodiscard]] constexpr float Left()   const { return X; }
    [[nodiscard]] constexpr float Top()    const { return Y; }
    [[nodiscard]] constexpr float Right()  const { return X + W; }
    [[nodiscard]] constexpr float Bottom() const { return Y + H; }

    [[nodiscard]] constexpr float CenterX() const { return X + W * 0.5f; }
    [[nodiscard]] constexpr float CenterY() const { return Y + H * 0.5f; }

    // ── Intersection tests ─────────────────────────────────────────────────────

    [[nodiscard]] constexpr bool Intersects(const BoxCollision& Other) const
    {
        if (!Enabled || !Other.Enabled) return false;
        return Left()   < Other.Right()  &&
               Right()  > Other.Left()  &&
               Top()    < Other.Bottom() &&
               Bottom() > Other.Top();
    }

    [[nodiscard]] constexpr bool ContainsPoint(float Px, float Py) const
    {
        if (!Enabled) return false;
        return Px > Left() && Px < Right() &&
               Py > Top()  && Py < Bottom();
    }

    [[nodiscard]] constexpr bool ContainsBox(const BoxCollision& Other) const
    {
        if (!Enabled || !Other.Enabled) return false;
        return Other.Left()   >= Left()  &&
               Other.Right()  <= Right() &&
               Other.Top()    >= Top()   &&
               Other.Bottom() <= Bottom();
    }

    // ── Spatial queries ────────────────────────────────────────────────────────

    [[nodiscard]] constexpr float OverlapX(const BoxCollision& Other) const
    {
        const float PushRight = Other.Right()  - Left();
        const float PushLeft  = Other.Left()   - Right();
        if (PushRight <= 0.0f || PushLeft >= 0.0f) return 0.0f;
        return (std::abs(PushRight) < std::abs(PushLeft)) ? PushRight : PushLeft;
    }

    [[nodiscard]] constexpr float OverlapY(const BoxCollision& Other) const
    {
        const float PushDown = Other.Bottom() - Top();
        const float PushUp   = Other.Top()    - Bottom();
        if (PushDown <= 0.0f || PushUp >= 0.0f) return 0.0f;
        return (std::abs(PushDown) < std::abs(PushUp)) ? PushDown : PushUp;
    }

private:
    uint32_t m_registryId = 0;
};
