#pragma once
#include <../../UGECore/Public/GameClasses/SceneObject.h>
#include <IScriptableObject.h>
#include <TileWorld.h>
#include <PlatformerHudViewModel.h>
#include <PlatformerCharacter.h>
#include <PlatformerStateTags.h>
#include <memory>

// ── PlatformerScene ───────────────────────────────────────────────────────────
//
//  Reference resolution : 1600 × 1200.  All positions, sizes, and speeds are
//  expressed in reference pixels.  Tick() scales them to the actual window size
//  at render time so the scene looks identical at any resolution.
//
//  Owns the tile world, score binding, and coin collection.  After the character
//  updates its collision box each frame, PlatformerScene::Update() queries the
//  coin BoxCollisionGrid, hides any hit tiles, and updates the score directly.
//
class PlatformerScene : public SceneObject, public IScriptableObject
{
public:
    REGISTER_SCENE("platformer", PlatformerScene)

    PlatformerScene(SDL_Renderer* Renderer, SDL_Window* Window);

    void Update()                      override;
    void Tick(float deltaTime)         override;
    bool HandleEvent(SDL_Event& Event) override;

    // ── IScriptableObject — delegates to PlatformerCharacter ─────────────────
    void RegisterObject(sol::state& Lua)   override;
    void UnregisterObject(sol::state& Lua) override;

    // ── State keys ────────────────────────────────────────────────────────────
    static constexpr const char* SCORE_KEY           = PlatformerHudViewModel::SCORE_KEY;
    static constexpr const char* PLATFORMER_DATA_KEY = "platformerData";

private:
    static constexpr float REF_W = 1600.0f;
    static constexpr float REF_H = 1200.0f;

    // ── Score ─────────────────────────────────────────────────────────────────
    int             m_score        = 0;
    AppStateBinding m_scoreBinding;

    // ── Tile world ────────────────────────────────────────────────────────────
    TileWorld m_world;


    // ── Player character ──────────────────────────────────────────────────────
    std::unique_ptr<PlatformerCharacter> m_character;
};
