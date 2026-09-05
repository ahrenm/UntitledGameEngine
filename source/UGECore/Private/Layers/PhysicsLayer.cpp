#include <Layers/PhysicsLayer.h>
#include <Layers/SDLLayer.h>
#include <Layers/LoggingLayer.h>
#include <Render/Renderer2D.h>
#include <ServiceLocator.h>
#include <UGEApplication.h>
#include <Camera2D.h>
#include <box2d/box2d.h>
#include <format>
#include <unordered_map>
#include <cstring>

// ── PIMPL implementation ──────────────────────────────────────────────────────
struct PhysicsLayer::Impl
{
    b2WorldId worldId = b2_nullWorldId;
    int       subSteps = 8;

    struct BodyEntry
    {
        b2BodyId  bodyId      = b2_nullBodyId;
        b2ShapeId mainShapeId = b2_nullShapeId;
        float     halfW       = 0.0f;  // metres — main body
        float     halfH       = 0.0f;  // metres — main body
        void*     userData    = nullptr;
    };

    uint32_t                                  nextId = 1;
    std::unordered_map<uint32_t, BodyEntry>   bodies;
};

// ── Factory ───────────────────────────────────────────────────────────────────
std::expected<std::unique_ptr<PhysicsLayer>, std::string> PhysicsLayer::Create()
{
    auto Layer = std::unique_ptr<PhysicsLayer>(new PhysicsLayer());
    Layer->m_impl = std::make_unique<Impl>();
    return Layer;
}

PhysicsLayer::~PhysicsLayer()
{
    ShutdownPhysics();
}

// ── RegisterWithServiceLocator ─────────────────────────────────────────────────
// Seeds physics.* transient tags from LaunchSettings and subscribes reactive
// bindings.  Called once during UGEApplication::Create() layer push loop.
void PhysicsLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);

    const auto& S = UGEApplication::Settings;

    // Seed defaults (only if not already set by an earlier layer).
    if (auto* data = ServiceLocator::TryGet<UGEDataLayer>())
    {
        auto& T = data->Store;
        if (!T.Has(TAG_PHYSICS_GRAVITY_X.data()))
            T.Set(TAG_PHYSICS_GRAVITY_X.data(), DataValue{S.PhysicsDefaultGravityX});
        if (!T.Has(TAG_PHYSICS_GRAVITY_Y.data()))
            T.Set(TAG_PHYSICS_GRAVITY_Y.data(), DataValue{S.PhysicsDefaultGravityY});
        if (!T.Has(TAG_PHYSICS_PPM.data()))
            T.Set(TAG_PHYSICS_PPM.data(),       DataValue{S.PhysicsPixelsPerMeter});
    }

    // Bind gravity.x — applies immediately to running world.
    m_gravXBinding = DATA_BIND(TAG_PHYSICS_GRAVITY_X.data(),
        S.PhysicsDefaultGravityX,
        [this](const Tag&, const DataValue& V) {
            if (const float* F = V.TryAs<float>()) { m_gravX = *F; applyGravityToWorld(); }
        }, Transient);

    // Bind gravity.y — applies immediately to running world.
    m_gravYBinding = DATA_BIND(TAG_PHYSICS_GRAVITY_Y.data(),
        S.PhysicsDefaultGravityY,
        [this](const Tag&, const DataValue& V) {
            if (const float* F = V.TryAs<float>()) { m_gravY = *F; applyGravityToWorld(); }
        }, Transient);

    // Bind pixels_per_meter — only effective on next InitPhysics().
    m_ppmBinding = DATA_BIND(TAG_PHYSICS_PPM.data(),
        S.PhysicsPixelsPerMeter,
        [this](const Tag&, const DataValue& V) {
            if (const float* F = V.TryAs<float>())
            {
                m_pixelsPerMeter = *F;
                if (HasPhysicsWorld())
                    Log("[Physics] pixels_per_meter changed while world active — effective on next InitPhysics()");
            }
        }, Transient);

    // Bind debug draw toggle — shared with SDL.ShowCollision() via the same tag.
    m_debugDrawBinding = DATA_BIND(TAG_DEBUG_SHOW_COLLISION.data(),
        0,
        [this](const Tag&, const DataValue& V) {
            if (const int* I = V.TryAs<int>()) m_debugDraw = (*I != 0);
        }, Transient);

    // Seed local floats from current (just-set) transient values.
    if (const auto* v = m_gravXBinding.GetValue())
        if (const float* f = v->TryAs<float>()) m_gravX = *f;
    if (const auto* v = m_gravYBinding.GetValue())
        if (const float* f = v->TryAs<float>()) m_gravY = *f;
    if (const auto* v = m_ppmBinding.GetValue())
        if (const float* f = v->TryAs<float>()) m_pixelsPerMeter = *f;
}

// ── World lifecycle ────────────────────────────────────────────────────────────
void PhysicsLayer::InitPhysics(const PhysicsWorldCreateParams& Params)
{
    ShutdownPhysics();

    b2WorldDef worldDef    = b2DefaultWorldDef();
    worldDef.gravity       = { m_gravX, m_gravY };
    worldDef.enableSleep   = Params.EnableSleeping;

    m_impl->worldId  = b2CreateWorld(&worldDef);
    m_impl->subSteps = Params.SubSteps;

    Log(std::format("[Physics] World created — gravity=({:.2f},{:.2f}) m/s²  PPM={:.0f}  subSteps={}",
                    m_gravX, m_gravY, m_pixelsPerMeter, Params.SubSteps));
}

void PhysicsLayer::ShutdownPhysics()
{
    if (!b2World_IsValid(m_impl->worldId)) return;

    if (!m_impl->bodies.empty())
        Log(std::format("[Physics] ShutdownPhysics: {} body entries remaining (check scene cleanup)",
                        m_impl->bodies.size()));

    m_impl->bodies.clear();
    m_impl->nextId = 1;
    m_stateSyncEntries.clear();
    m_contactCallback = nullptr;

    b2DestroyWorld(m_impl->worldId);
    m_impl->worldId = b2_nullWorldId;

    Log("[Physics] World destroyed");
}

bool PhysicsLayer::HasPhysicsWorld() const
{
    return b2World_IsValid(m_impl->worldId);
}

// ── Body factory ───────────────────────────────────────────────────────────────
PhysicsBodyHandle PhysicsLayer::CreateBody(const BodyDef& Def)
{
    if (!b2World_IsValid(m_impl->worldId))
    {
        Log("[Physics] CreateBody: no active world — call InitPhysics() first");
        return {};
    }

    const float ppm   = m_pixelsPerMeter;
    const float halfW = (Def.W * 0.5f) / ppm;
    const float halfH = (Def.H * 0.5f) / ppm;
    const float cx    = (Def.X + Def.W * 0.5f) / ppm;
    const float cy    = (Def.Y + Def.H * 0.5f) / ppm;

    // ── Body ──────────────────────────────────────────────────────────────────
    b2BodyDef bodyDef     = b2DefaultBodyDef();
    bodyDef.position      = { cx, cy };
    bodyDef.fixedRotation = Def.FixedRotation;
    switch (Def.Type)
    {
        case BodyType::Dynamic:   bodyDef.type = b2_dynamicBody;   break;
        case BodyType::Kinematic: bodyDef.type = b2_kinematicBody; break;
        default:                  bodyDef.type = b2_staticBody;    break;
    }
    b2BodyId bodyId = b2CreateBody(m_impl->worldId, &bodyDef);

    // ── Main shape ────────────────────────────────────────────────────────────
    b2ShapeDef shapeDef            = b2DefaultShapeDef();
    shapeDef.density               = Def.Density;
    shapeDef.material.friction     = Def.Friction;
    shapeDef.material.restitution  = Def.Restitution;
    shapeDef.isSensor              = (Def.Type == BodyType::Sensor);
    shapeDef.enableSensorEvents    = (Def.Type == BodyType::Sensor);
    shapeDef.userData              = Def.UserData;
    b2Polygon   box        = b2MakeBox(halfW, halfH);
    b2ShapeId   mainShape  = b2CreatePolygonShape(bodyId, &shapeDef, &box);

    uint32_t id = m_impl->nextId++;
    m_impl->bodies[id] = { bodyId, mainShape, halfW, halfH, Def.UserData };

    PhysicsBodyHandle handle(id, this);
    handle.m_aliveToken = m_aliveToken;   // weak_ptr — expires when PhysicsLayer is destroyed
    return handle;
}

// ── Contact callback ───────────────────────────────────────────────────────────
void PhysicsLayer::SetContactCallback(std::function<void(const ContactEvent&)> Cb)
{
    m_contactCallback = std::move(Cb);
}

// ── DataLayer state sync ───────────────────────────────────────────────────────
void PhysicsLayer::EnableStateSync(PhysicsBodyHandle& Handle, std::string_view KeyPrefix)
{
    if (!Handle.IsValid()) return;
    m_stateSyncEntries.push_back({ Handle.m_id, std::string(KeyPrefix) });
}

// ── Update ─────────────────────────────────────────────────────────────────────
// Steps the Box2D world, dispatches contact events, and syncs state-tracked bodies.
// Runs AFTER SDLLayer::Update() (load order 4.1 > 4.0) so the scene has already
// applied impulses for this frame before the integrator runs.
void PhysicsLayer::Update()
{
    if (!b2World_IsValid(m_impl->worldId)) return;

    // Use a fixed sub-step dt aligned to the engine's 30 FPS cap.
    constexpr float FIXED_DT = 1.0f / 30.0f;
    b2World_Step(m_impl->worldId, FIXED_DT, m_impl->subSteps);

    // ── Event dispatch ────────────────────────────────────────────────────────
    // Box2D 3.x splits events into two separate APIs:
    //
    //  b2World_GetSensorEvents()  — sensor shape overlaps (isSensor=true on either shape).
    //                               This covers foot-sensor↔floor and player↔coin contacts.
    //
    //  b2World_GetContactEvents() — solid-body touch events (enableContactEvents=true).
    //                               Not currently used but kept for future solid contacts.
    //
    // Both feed the same m_contactCallback so callers need no knowledge of the split.
    if (m_contactCallback)
    {
        auto fire = [&](b2ShapeId shA, b2ShapeId shB, ContactPhase phase)
        {
            void* udA = b2Shape_IsValid(shA) ? b2Shape_GetUserData(shA) : nullptr;
            void* udB = b2Shape_IsValid(shB) ? b2Shape_GetUserData(shB) : nullptr;
            m_contactCallback({ udA, udB, phase });
        };

        // Sensor events — foot sensor ↔ floor, player body ↔ coin sensors.
        b2SensorEvents sensorEvents = b2World_GetSensorEvents(m_impl->worldId);
        for (int i = 0; i < sensorEvents.beginCount; ++i)
            fire(sensorEvents.beginEvents[i].sensorShapeId,
                 sensorEvents.beginEvents[i].visitorShapeId, ContactPhase::Begin);
        for (int i = 0; i < sensorEvents.endCount; ++i)
            fire(sensorEvents.endEvents[i].sensorShapeId,
                 sensorEvents.endEvents[i].visitorShapeId, ContactPhase::End);

        // Solid contact events (requires enableContactEvents=true on the shape def).
        b2ContactEvents contactEvents = b2World_GetContactEvents(m_impl->worldId);
        for (int i = 0; i < contactEvents.beginCount; ++i)
            fire(contactEvents.beginEvents[i].shapeIdA,
                 contactEvents.beginEvents[i].shapeIdB, ContactPhase::Begin);
        for (int i = 0; i < contactEvents.endCount; ++i)
            fire(contactEvents.endEvents[i].shapeIdA,
                 contactEvents.endEvents[i].shapeIdB, ContactPhase::End);
    }

    // ── DataLayer state sync ──────────────────────────────────────────────────
    if (!m_stateSyncEntries.empty())
    {
        if (auto* data = ServiceLocator::TryGet<UGEDataLayer>())
        {
            const float ppm = m_pixelsPerMeter;
            for (const auto& entry : m_stateSyncEntries)
            {
                auto it = m_impl->bodies.find(entry.bodyId);
                if (it == m_impl->bodies.end()) continue;
                const auto& be = it->second;
                if (!b2Body_IsValid(be.bodyId)) continue;

                b2Vec2 pos = b2Body_GetPosition(be.bodyId);
                b2Vec2 vel = b2Body_GetLinearVelocity(be.bodyId);

                // Publish position and velocity as atomic Vec2 entries so each
                // update fires a single notification with both components
                // consistent (was four separate .x/.y/.vx/.vy sets).
                data->Store.Set(entry.prefix + ".pos",
                    DataValue{Vec2{(pos.x - be.halfW) * ppm, (pos.y - be.halfH) * ppm}});
                data->Store.Set(entry.prefix + ".vel",
                    DataValue{Vec2{vel.x * ppm, vel.y * ppm}});
            }
        }
    }
}

// ── Tick ──────────────────────────────────────────────────────────────────────
// Debug draw only — no simulation here.
// Runs AFTER SDLLayer::Draw() (scene sprites) so physics AABBs composite on top
// of game graphics, and BEFORE RmlUILayer::Draw() (6.0) so UI composites last.
void PhysicsLayer::Draw(float /*DeltaTime*/)
{
    if (m_debugDraw) drawDebug();
}

// ── Private helpers ────────────────────────────────────────────────────────────
void PhysicsLayer::applyGravityToWorld()
{
    if (b2World_IsValid(m_impl->worldId))
        b2World_SetGravity(m_impl->worldId, { m_gravX, m_gravY });
}

void PhysicsLayer::drawDebug() const
{
    auto* sdl = ServiceLocator::TryGet<SDLLayer>();
    if (!sdl) return;
    Renderer2D* r2d = sdl->Get2DRenderer();
    if (!r2d) return;
    const Camera2D&  cam      = sdl->GetCamera();
    const float      refH     = static_cast<float>(sdl->RefHeight());
    const float      ppm      = m_pixelsPerMeter;

    // Filled quad + rectangle outline (4 thin quads) helpers using normalized colors.
    auto fill = [&](const SDL_FRect& rect, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
        r2d->DrawColoredQuad(rect, SDL_FColor{ R / 255.0f, G / 255.0f, B / 255.0f, A / 255.0f });
    };
    auto outline = [&](const SDL_FRect& rect, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
        const SDL_FColor c{ R / 255.0f, G / 255.0f, B / 255.0f, A / 255.0f };
        constexpr float t = 2.0f; // outline thickness in reference pixels
        r2d->DrawColoredQuad(SDL_FRect{ rect.x, rect.y, rect.w, t }, c);                    // top
        r2d->DrawColoredQuad(SDL_FRect{ rect.x, rect.y + rect.h - t, rect.w, t }, c);       // bottom
        r2d->DrawColoredQuad(SDL_FRect{ rect.x, rect.y, t, rect.h }, c);                    // left
        r2d->DrawColoredQuad(SDL_FRect{ rect.x + rect.w - t, rect.y, t, rect.h }, c);       // right
    };

    for (const auto& [id, be] : m_impl->bodies)
    {
        if (!b2Body_IsValid(be.bodyId)) continue;

        b2Vec2 pos = b2Body_GetPosition(be.bodyId);
        float  wx  = (pos.x - be.halfW) * ppm;
        float  wy  = (pos.y - be.halfH) * ppm;
        float  w   = be.halfW * 2.0f * ppm;
        float  h   = be.halfH * 2.0f * ppm;
        SDL_FRect rect = cam.WorldRect(wx, wy, w, h, refH);

        const bool isSensor = b2Shape_IsValid(be.mainShapeId)
                              && b2Shape_IsSensor(be.mainShapeId);
        const b2BodyType bt = b2Body_GetType(be.bodyId);

        if (isSensor)
        {
            // Sensor body (e.g. coins, trigger zones) — green outline, no fill.
            outline(rect, 0, 230, 100, 200);
        }
        else if (bt == b2_staticBody)
        {
            // Solid static body (terrain, walls) — blue tint fill + brighter outline.
            fill(rect, 80, 120, 255, 50);
            outline(rect, 80, 120, 255, 180);
        }
        else
        {
            // Dynamic / kinematic body (player, moving objects) — amber fill + outline.
            fill(rect, 255, 200, 50, 55);
            outline(rect, 255, 200, 50, 200);
        }
    }
}

// ── Body table mutation (called by PhysicsBodyHandle) ─────────────────────────
void PhysicsLayer::releaseBody(uint32_t Id)
{
    auto it = m_impl->bodies.find(Id);
    if (it == m_impl->bodies.end()) return;

    const auto& be = it->second;
    if (b2World_IsValid(m_impl->worldId) && b2Body_IsValid(be.bodyId))
        b2DestroyBody(be.bodyId);

    m_impl->bodies.erase(it);

    // Clean up state-sync entry if present.
    m_stateSyncEntries.erase(
        std::remove_if(m_stateSyncEntries.begin(), m_stateSyncEntries.end(),
                       [Id](const StateSyncEntry& e) { return e.bodyId == Id; }),
        m_stateSyncEntries.end());
}

void PhysicsLayer::pbhSetPosition(uint32_t Id, float Wx, float Wy)
{
    auto it = m_impl->bodies.find(Id);
    if (it == m_impl->bodies.end()) return;
    const auto& be = it->second;
    if (!b2Body_IsValid(be.bodyId)) return;
    b2Vec2 centre = { (Wx + be.halfW * m_pixelsPerMeter) / m_pixelsPerMeter,
                      (Wy + be.halfH * m_pixelsPerMeter) / m_pixelsPerMeter };
    b2Body_SetTransform(be.bodyId, centre, b2Body_GetRotation(be.bodyId));
}

SDL_FPoint PhysicsLayer::pbhGetPosition(uint32_t Id) const
{
    auto it = m_impl->bodies.find(Id);
    if (it == m_impl->bodies.end()) return {};
    const auto& be = it->second;
    if (!b2Body_IsValid(be.bodyId)) return {};
    b2Vec2 pos = b2Body_GetPosition(be.bodyId);
    return { (pos.x - be.halfW) * m_pixelsPerMeter,
             (pos.y - be.halfH) * m_pixelsPerMeter };
}

void PhysicsLayer::pbhSetLinearVelocity(uint32_t Id, float Vx, float Vy)
{
    auto it = m_impl->bodies.find(Id);
    if (it == m_impl->bodies.end()) return;
    if (!b2Body_IsValid(it->second.bodyId)) return;
    b2Body_SetLinearVelocity(it->second.bodyId,
        { Vx / m_pixelsPerMeter, Vy / m_pixelsPerMeter });
}

SDL_FPoint PhysicsLayer::pbhGetLinearVelocity(uint32_t Id) const
{
    auto it = m_impl->bodies.find(Id);
    if (it == m_impl->bodies.end()) return {};
    if (!b2Body_IsValid(it->second.bodyId)) return {};
    b2Vec2 v = b2Body_GetLinearVelocity(it->second.bodyId);
    return { v.x * m_pixelsPerMeter, v.y * m_pixelsPerMeter };
}

void PhysicsLayer::pbhApplyImpulse(uint32_t Id, float IxNs, float IyNs)
{
    auto it = m_impl->bodies.find(Id);
    if (it == m_impl->bodies.end()) return;
    if (!b2Body_IsValid(it->second.bodyId)) return;
    // IxNs / IyNs are in N·s (kg·m/s) — Box2D's native impulse units.
    // This changes velocity by impulse/mass (m/s), correctly respecting body mass.
    b2Body_ApplyLinearImpulseToCenter(it->second.bodyId, { IxNs, IyNs }, true);
}

float PhysicsLayer::pbhGetMass(uint32_t Id) const
{
    auto it = m_impl->bodies.find(Id);
    if (it == m_impl->bodies.end()) return 0.0f;
    if (!b2Body_IsValid(it->second.bodyId)) return 0.0f;
    return b2Body_GetMass(it->second.bodyId);
}

void PhysicsLayer::pbhSetEnabled(uint32_t Id, bool Enabled)
{
    auto it = m_impl->bodies.find(Id);
    if (it == m_impl->bodies.end()) return;
    if (!b2Body_IsValid(it->second.bodyId)) return;
    if (Enabled) b2Body_Enable(it->second.bodyId);
    else         b2Body_Disable(it->second.bodyId);
}

bool PhysicsLayer::pbhIsGrounded(uint32_t Id, float MinNormalY) const
{
    auto it = m_impl->bodies.find(Id);
    if (it == m_impl->bodies.end()) return false;
    const auto& be = it->second;
    if (!b2Body_IsValid(be.bodyId)) return false;

    const int capacity = b2Body_GetContactCapacity(be.bodyId);
    if (capacity == 0) return false;

    // Stack buffer — avoids heap alloc for typical contact counts.
    b2ContactData contacts[16];
    const int count = b2Body_GetContactData(be.bodyId, contacts,
                                             std::min(capacity, 16));

    for (int i = 0; i < count; ++i)
    {
        // b2Manifold.normal points from shape A toward shape B.
        // For a floor-below-character contact the normal is roughly vertical.
        // Using std::abs covers both A=character and B=character orderings.
        if (std::abs(contacts[i].manifold.normal.y) >= MinNormalY)
            return true;
    }
    return false;
}

// ── IScriptableObject ─────────────────────────────────────────────────────────
void PhysicsLayer::RegisterObject(sol::state& Lua)
{
    auto P = Lua.create_named_table("Physics");

    P.set_function("InitPhysics",     [this]()           { InitPhysics(); });
    P.set_function("ShutdownPhysics", [this]()           { ShutdownPhysics(); });
    P.set_function("HasWorld",        [this]() -> bool   { return HasPhysicsWorld(); });
    P.set_function("GetBodyCount",    [this]() -> int    { return static_cast<int>(m_impl->bodies.size()); });

    // ShowCollision() — toggles the physics body debug overlay on/off.
    // Writes to the shared "debug.show_collision" transient tag; the DataBinding
    // in RegisterWithServiceLocator() reacts and updates m_debugDraw.
    P.set_function("ShowCollision", [this]() {
        if (auto* d = ServiceLocator::TryGet<UGEDataLayer>()) {
            const DataValue* cur = d->Store.Get(TAG_DEBUG_SHOW_COLLISION.data());
            const int*  curI = cur ? cur->TryAs<int>() : nullptr;
            const bool  next = !(curI && *curI != 0);
            d->Store.Set(TAG_DEBUG_SHOW_COLLISION.data(), DataValue{next ? 1 : 0});
            Log(std::format("[Physics] Debug draw {}", next ? "ON" : "OFF"));
        }
    });

    // DebugDraw(bool) — explicit on/off; ShowCollision() is the toggle convenience.
    P.set_function("DebugDraw", [this](bool On) {
        if (auto* d = ServiceLocator::TryGet<UGEDataLayer>())
            d->Store.Set(TAG_DEBUG_SHOW_COLLISION.data(), DataValue{On ? 1 : 0});
        Log(std::format("[Physics] Debug draw {}", On ? "ON" : "OFF"));
    });

    P.set_function("SetGravity", [this](float Gx, float Gy) {
        if (auto* d = ServiceLocator::TryGet<UGEDataLayer>()) {
            d->Store.Set(TAG_PHYSICS_GRAVITY_X.data(), DataValue{Gx});
            d->Store.Set(TAG_PHYSICS_GRAVITY_Y.data(), DataValue{Gy});
        }
    });

    P.set_function("SetPixelsPerMeter", [this](float Ppm) {
        if (HasPhysicsWorld()) {
            Log("[Physics] Cannot change pixels_per_meter while world is active. Call ShutdownPhysics() first.");
            return;
        }
        if (auto* d = ServiceLocator::TryGet<UGEDataLayer>())
            d->Store.Set(TAG_PHYSICS_PPM.data(), DataValue{Ppm});
    });
}

// ── PhysicsBodyHandle method bodies ───────────────────────────────────────────
// Defined here (after PhysicsLayer is fully defined) so the full type is visible.

PhysicsBodyHandle::~PhysicsBodyHandle()
{
    // Guard: skip if PhysicsLayer has already been destroyed (weak_ptr expired)
    // or if this handle was never initialised / already moved-from.
    if (!m_aliveToken.expired() && IsValid())
        m_layer->releaseBody(m_id);
}

PhysicsBodyHandle::PhysicsBodyHandle(PhysicsBodyHandle&& Other) noexcept
    : m_id(std::exchange(Other.m_id, 0))
    , m_layer(std::exchange(Other.m_layer, nullptr))
    , m_aliveToken(std::move(Other.m_aliveToken))
{}

PhysicsBodyHandle& PhysicsBodyHandle::operator=(PhysicsBodyHandle&& Other) noexcept
{
    if (this != &Other)
    {
        if (!m_aliveToken.expired() && IsValid()) m_layer->releaseBody(m_id);
        m_id         = std::exchange(Other.m_id,    0);
        m_layer      = std::exchange(Other.m_layer, nullptr);
        m_aliveToken = std::move(Other.m_aliveToken);
    }
    return *this;
}

void       PhysicsBodyHandle::SetPosition(float Wx, float Wy)    { if (IsValid()) m_layer->pbhSetPosition(m_id, Wx, Wy); }
SDL_FPoint PhysicsBodyHandle::GetPosition() const                  { return IsValid() ? m_layer->pbhGetPosition(m_id) : SDL_FPoint{}; }
void       PhysicsBodyHandle::SetLinearVelocity(float Vx, float Vy) { if (IsValid()) m_layer->pbhSetLinearVelocity(m_id, Vx, Vy); }
SDL_FPoint PhysicsBodyHandle::GetLinearVelocity() const            { return IsValid() ? m_layer->pbhGetLinearVelocity(m_id) : SDL_FPoint{}; }
float      PhysicsBodyHandle::GetMass() const                      { return IsValid() ? m_layer->pbhGetMass(m_id) : 0.0f; }
void       PhysicsBodyHandle::ApplyImpulse(float IxNs, float IyNs){ if (IsValid()) m_layer->pbhApplyImpulse(m_id, IxNs, IyNs); }
void       PhysicsBodyHandle::SetEnabled(bool Enabled)             { if (IsValid()) m_layer->pbhSetEnabled(m_id, Enabled); }
bool       PhysicsBodyHandle::IsGrounded(float MinNormalY) const   { return IsValid() ? m_layer->pbhIsGrounded(m_id, MinNormalY) : false; }



