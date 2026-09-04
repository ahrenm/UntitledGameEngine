#include "PlatformerScene.h"
#include <Layers/PhysicsLayer.h>
#include <sol/sol.hpp>

// ── Constructor ───────────────────────────────────────────────────────────────
PlatformerScene::PlatformerScene(SDL_Renderer* Renderer, SDL_Window* Window)
    : SceneObject(Renderer, Window)
{
    auto* dataLayer = GetDataLayer();
    auto* physics   = GetPhysicsLayer();

    auto& Store = dataLayer->Store;
    auto dsStr = [&](const char* Path) -> std::optional<std::string> {
        if (const DataValue* V = Store.Get(std::string(PLATFORMER_DATA_KEY) + "." + Path))
            if (const std::string* S = V->TryAs<std::string>()) return *S;
        return std::nullopt;
    };
    auto dsFloat = [&](const char* Path) -> std::optional<float> {
        if (const DataValue* V = Store.Get(std::string(PLATFORMER_DATA_KEY) + "." + Path))
            if (const float* F = V->TryAs<float>()) return *F;
        return std::nullopt;
    };

    // ── Assets ────────────────────────────────────────────────────────────────
    const auto BgPath  = dsStr("assets.background");
    GetSDLLayer()->SetBackground(BgPath ? BgPath->c_str() : nullptr);

    const auto DocPath = dsStr("assets.document");
    GetUILayer()->LoadDocument(DocPath ? DocPath->c_str() : nullptr);

    // ── Physics override from TOML (fires PhysicsLayer bindings before InitPhysics) ──
    // This updates m_gravX/m_gravY inside PhysicsLayer so InitPhysics() picks them up.
    if (auto gY = dsFloat("physics.gravity_y"))
        Store.Set(TAG_PHYSICS_GRAVITY_Y.data(), DataValue{*gY});
    if (auto gX = dsFloat("physics.gravity_x"))
        Store.Set(TAG_PHYSICS_GRAVITY_X.data(), DataValue{*gX});

    // ── Platformer-specific transient state ───────────────────────────────────
    Store.Set(TAG_SPEED.data(),       DataValue{dsFloat("physics.speed"      ).value_or(240.0f)});
    Store.Set(TAG_JUMP_HEIGHT.data(), DataValue{dsFloat("physics.jump_height" ).value_or(280.0f)});

    // ── Initialise physics world ──────────────────────────────────────────────
    // Must happen BEFORE tile world builds bodies and before character is created.
    if (physics) physics->InitPhysics();

    // ── Boundary static bodies ────────────────────────────────────────────────
    // Replace game-code X-clamping and fall-through recovery with Box2D geometry.
    // Walls are tall enough to contain any in-play height; floor is a deep slab
    // below the ground tiles as a safety net for edge-case physics tunnelling.
    if (physics)
    {
        // Collision-box dimensions (shared with PlatformerCharacter).
        constexpr float cw   = PlatformerCharacter::COLLISION_W;          // 85 px
        constexpr float cOff = PlatformerCharacter::COLLISION_X_OFFSET;   // 7.5 px
        constexpr float pw   = PlatformerCharacter::PLAYER_W;             // 100 px
        constexpr float wall = 200.0f;   // wall thickness (px)
        constexpr float tall = REF_H + 1000.0f;
        constexpr float deep = 600.0f;   // floor depth below world floor

        // Left wall — right edge sits at x = cOff so the sprite's left pixel
        // aligns with x = 0 when pressed against it.
        m_leftWall = physics->CreateBody({
            .X = -(wall - cOff), .Y = -deep,
            .W = wall,           .H = tall,
            .Type = BodyType::Static, .Friction = 0.0f
        });

        // Right wall — left edge at x = REF_W - pw + cOff + cw so the sprite's
        // right pixel aligns with x = REF_W when pressed against it.
        const float rightEdge = REF_W - pw + cOff + cw;   // = 1592.5 px
        m_rightWall = physics->CreateBody({
            .X = rightEdge, .Y = -deep,
            .W = wall,      .H = tall,
            .Type = BodyType::Static, .Friction = 0.0f
        });

        // World floor — wide slab below y = 0 (the bottom of the ground tiles).
        m_worldFloor = physics->CreateBody({
            .X = -(wall + cOff), .Y = -(deep + 200.0f),
            .W = REF_W + 2.0f * wall, .H = deep,
            .Type = BodyType::Static
        });
    }

    // ── Tile world (builds Box2D static/sensor bodies into the active world) ──
    const auto mapKey = dsStr("assets.tile_map_key").value_or("");
    m_world.Build(PLATFORMER_DATA_KEY, mapKey.c_str());

    // ── Character ─────────────────────────────────────────────────────────────
    m_character = std::make_unique<PlatformerCharacter>(Renderer, Window, PLATFORMER_DATA_KEY);

    // ── Score ─────────────────────────────────────────────────────────────────
    m_scoreBinding = DATA_BIND(SCORE_KEY, 0,
        [this](const Tag&, const DataValue& V) {
            if (const auto* I = V.TryAs<int>()) m_score = *I;
        }, Transient);
    m_scoreBinding.SetValue(DataValue{m_score});
}

// ── ScriptableObject ──────────────────────────────────────────────────────────
void PlatformerScene::RegisterObject(sol::state& Lua)   { if (m_character) m_character->RegisterObject(Lua);   }
void PlatformerScene::UnregisterObject(sol::state& Lua) { if (m_character) m_character->UnregisterObject(Lua); }

// ── HandleEvent ───────────────────────────────────────────────────────────────
bool PlatformerScene::HandleEvent(SDL_Event& Event)
{
    return m_character && m_character->HandleEvent(Event);
}

// ── Update ────────────────────────────────────────────────────────────────────
void PlatformerScene::Update()
{
    if (m_character) m_character->Update();
    checkCoinCollection();
}

// ── checkCoinCollection ───────────────────────────────────────────────────────
// Direct AABB overlap test between the player's collision box and each visible
// coin tile.  Runs after character Update() so m_body position is current.
// Does not rely on Box2D sensor events (static sensor bodies do not generate
// b2SensorBeginTouchEvent in Box2D 3.x — only dynamic sensor bodies do).
void PlatformerScene::checkCoinCollection()
{
    if (!m_character) return;

    const SDL_FPoint bp = m_character->GetBodyPosition();
    const float bx = bp.x;
    const float by = bp.y;
    const float bw = PlatformerCharacter::COLLISION_W;
    const float bh = PlatformerCharacter::PLAYER_H;

    for (auto& T : m_world.Coins.Tiles)
    {
        if (!T.IsVisible) continue;
        // AABB overlap test (Y-up: Y = bottom edge, Y+H = top edge)
        if (bx < T.X + T.SIZE_W && bx + bw > T.X &&
            by < T.Y + T.SIZE_H && by + bh > T.Y)
        {
            T.Hide();
            m_score += 20;
            m_scoreBinding.SetValue(DataValue{m_score});
        }
    }
}

// ── Draw ──────────────────────────────────────────────────────────────────────
void PlatformerScene::Draw(float DeltaTime)
{
    // T.X = left edge, T.Y = bottom edge (Y-up world space).
    auto drawTile = [&](const Tile& T)
    {
        if (!T.IsVisible) return;
        const SDL_FRect Rect = WorldRect(T.X, T.Y, T.SIZE_W, T.SIZE_H);
        if (T.Spr)
            T.Spr->Draw(m_renderer, Rect);
        else
        {
            SDL_SetRenderDrawColor(m_renderer, T.Color.r, T.Color.g, T.Color.b, T.Color.a);
            SDL_RenderFillRect(m_renderer, &Rect);
        }
    };

    for (const auto& T : m_world.Walls.Tiles) drawTile(T);
    for (const auto& T : m_world.Coins.Tiles) drawTile(T);

    if (m_character) m_character->Draw(DeltaTime);
}
