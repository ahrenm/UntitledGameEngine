#pragma once
#include "AppLayer.h"
#include "../IScriptableObject.h"
#include "../LayerRegistry.h"
#include "../Physics/PhysicsTypes.h"
#include "../Physics/PhysicsBodyHandle.h"
#include "../Layers/UGEDataLayer.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>

// ── PhysicsLayer ──────────────────────────────────────────────────────────────
// Load order: 4.1 — runs just after SDLLayer (4.0).
//
// Frame role split:
//   Update() — b2World_Step + contact-event dispatch + DataLayer state sync.
//              Runs after SDLLayer::Update() (scene logic / impulse-setting) so
//              physics integrates the impulses set this frame before any rendering.
//   Draw()   — drawDebug() only.
//              Runs after SDLLayer::Draw() (scene sprites), so physics AABBs are
//              composited on top of game graphics and beneath RmlUI.
//
// ...existing code...
class PhysicsLayer : public AppLayer, public IScriptableObject
{
public:
    REGISTER_LAYER("physics", 4.1f, PhysicsLayer)

    [[nodiscard]] static std::expected<std::unique_ptr<PhysicsLayer>, std::string> Create();

    ~PhysicsLayer() override;
    PhysicsLayer(const PhysicsLayer&)            = delete;
    PhysicsLayer& operator=(const PhysicsLayer&) = delete;

    // ── World lifecycle ───────────────────────────────────────────────────────
    void InitPhysics(const PhysicsWorldCreateParams& Params = {});
    void ShutdownPhysics();

    [[nodiscard]] bool  HasPhysicsWorld() const;
    [[nodiscard]] float PixelsPerMeter()  const { return m_pixelsPerMeter; }

    // ── Body factory ──────────────────────────────────────────────────────────
    [[nodiscard]] PhysicsBodyHandle CreateBody(const BodyDef& Def);

    // ── Contact callback ──────────────────────────────────────────────────────
    void SetContactCallback(std::function<void(const ContactEvent&)> Cb);

    // ── DataLayer state sync ──────────────────────────────────────────────────
    void EnableStateSync(PhysicsBodyHandle& Handle, std::string_view KeyPrefix);

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void Update() override;   // b2World_Step + contacts + state-sync
    void Draw(float DeltaTime) override;  // debug draw only

    // ── IScriptableObject ─────────────────────────────────────────────────────
    void RegisterObject(sol::state& Lua) override;
    void RegisterWithServiceLocator()    override;

private:
    PhysicsLayer() = default;

    // All Box2D types confined here (defined in PhysicsLayer.cpp).
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    // Current params kept in sync with physics.* transient tags.
    float m_gravX          =  0.0f;
    float m_gravY          = -9.81f;
    float m_pixelsPerMeter =  100.0f;
    bool  m_debugDraw      =  false;

    DataBinding m_gravXBinding;
    DataBinding m_gravYBinding;
    DataBinding m_ppmBinding;
    DataBinding m_debugDrawBinding;  // bound to TAG_DEBUG_SHOW_COLLISION

    struct StateSyncEntry { uint32_t bodyId; std::string prefix; };
    std::vector<StateSyncEntry> m_stateSyncEntries;

    std::function<void(const ContactEvent&)> m_contactCallback;

    // Alive-token: shared with all PhysicsBodyHandle instances via weak_ptr.
    // When PhysicsLayer is destroyed this shared_ptr is released; all handles
    // whose weak_ptr has expired will skip releaseBody() in their destructor.
    std::shared_ptr<bool> m_aliveToken = std::make_shared<bool>(true);

    void applyGravityToWorld();
    void drawDebug() const;

    friend class PhysicsBodyHandle;
    void       releaseBody(uint32_t Id);
    void       pbhSetPosition(uint32_t Id, float Wx, float Wy);
    SDL_FPoint pbhGetPosition(uint32_t Id) const;
    void       pbhSetLinearVelocity(uint32_t Id, float Vx, float Vy);
    SDL_FPoint pbhGetLinearVelocity(uint32_t Id) const;
    float      pbhGetMass(uint32_t Id) const;
    void       pbhApplyImpulse(uint32_t Id, float IxNs, float IyNs);
    void       pbhSetEnabled(uint32_t Id, bool Enabled);
    bool       pbhIsGrounded(uint32_t Id, float MinNormalY) const;
};

