#include "PlatformerScene.h"
#include <sol/sol.hpp>

// SCENE CONSTRUCTOR
PlatformerScene::PlatformerScene(SDL_Renderer* Renderer, SDL_Window* Window)
    : SceneObject(Renderer, Window)
{

    auto* dataLayer = GetDataLayer();

    // Pulls from the data set stored in /assets/DATA
    // .toml files in this location are pre-loaded by the data layer at startup and referenced by file name/key
    // Data elements in the file are referenced by . separated paths

    // ── Assets ────────────────────────────────────────────────────────────────
    const auto BgPath  = dataLayer->Data.GetString(PLATFORMER_DATA_KEY, "assets.background");
    GetSDLLayer()->SetBackground(BgPath ? BgPath->c_str() : nullptr);

    const auto DocPath = dataLayer->Data.GetString(PLATFORMER_DATA_KEY, "assets.document");
    GetUILayer()->LoadDocument(DocPath ? DocPath->c_str() : nullptr);

    // ── Physics — load from .toml and publish to transient AppState ───────────
    // Read each value from the dataset and push it straight into the transient
    // store so PlatformerCharacter (and the Lua console) can read/write them.
    dataLayer->Transient.Set(TAG_GRAVITY.data(),        AppStateValue{ dataLayer->Data.GetFloat(PLATFORMER_DATA_KEY, "physics.gravity"       ).value_or(0.0f) });
    dataLayer->Transient.Set(TAG_GROUND_EPSILON.data(), AppStateValue{ dataLayer->Data.GetFloat(PLATFORMER_DATA_KEY, "physics.ground_epsilon" ).value_or(0.0f) });
    dataLayer->Transient.Set(TAG_SPEED.data(),          AppStateValue{ dataLayer->Data.GetFloat(PLATFORMER_DATA_KEY, "physics.speed"          ).value_or(0.0f) });
    dataLayer->Transient.Set(TAG_JUMP_HEIGHT.data(),    AppStateValue{ dataLayer->Data.GetFloat(PLATFORMER_DATA_KEY, "physics.jump_height"    ).value_or(0.0f) });

    // ── Tile world (must exist before character is created) ───────────────────
    m_world.Build(PLATFORMER_DATA_KEY);

    // ── Character ─────────────────────────────────────────────────────────────
    m_character = std::make_unique<PlatformerCharacter>(Renderer, Window, m_world, PLATFORMER_DATA_KEY);

    // ── Score — transient AppState binding ────────────────────────────────────
    m_scoreBinding = APPSTATE_BIND_TRANSIENT(SCORE_KEY, 0,
        [this](const std::string&, const AppStateValue& V)
        {
            if (const auto* I = std::get_if<int>(&V)) m_score = *I;
        });

    m_scoreBinding.SetValue(AppStateValue{m_score});
}

// ── ScriptableObject — delegates to the character ────────────────────────────
void PlatformerScene::RegisterObject(sol::state& Lua)
{
    if (m_character) m_character->RegisterObject(Lua);
}

void PlatformerScene::UnregisterObject(sol::state& Lua)
{
    if (m_character) m_character->UnregisterObject(Lua);
}

// ── HandleEvent ───────────────────────────────────────────────────────────────
bool PlatformerScene::HandleEvent(SDL_Event& Event)
{
    return m_character && m_character->HandleEvent(Event);
}

// ── Update ────────────────────────────────────────────────────────────────────
void PlatformerScene::Update()
{
    if (m_character) m_character->Update();

    // ── Coin collection ───────────────────────────────────────────────────────
    // After the character's collision box is updated, query the coin grid to
    // find overlapping coins.  Each hit's UserData points back to the owning
    // Tile; we hide it (disables collision + rendering) and award score directly.
    if (m_character)
    {
        std::vector<BoxCollision*> hits;
        if (m_world.Coins.CollisionGrid.Intersects(&m_character->CollisionBox(), hits))
        {
            for (BoxCollision* hit : hits)
            {
                if (auto* tile = static_cast<Tile*>(hit->UserData))
                {
                    tile->Hide();
                    m_score += 20;
                }
            }
            m_scoreBinding.SetValue(AppStateValue{m_score});
        }
    }
}

// ── Tick ──────────────────────────────────────────────────────────────────────
void PlatformerScene::Tick(float DeltaTime)
{
    int W = 0, H = 0;
    SDL_GetWindowSize(m_window, &W, &H);

    const float SX = static_cast<float>(W) / REF_W;
    const float SY = static_cast<float>(H) / REF_H;

    auto drawTile = [&](const Tile& T)
    {
        if (!T.IsVisible) return;

        const SDL_FRect Rect{ T.Bounds.Left() * SX, T.Bounds.Top() * SY,
                              T.Bounds.W      * SX, T.Bounds.H     * SY };
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

    if (m_character) m_character->Tick(DeltaTime);
}
