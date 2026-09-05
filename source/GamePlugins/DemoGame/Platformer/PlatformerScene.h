#pragma once
#include <../../UGECore/Public/GameClasses/SceneObject.h>
#include <IScriptableObject.h>
#include <TileWorld.h>
#include <PlatformerHudViewModel.h>
#include <PlatformerCharacter.h>
#include <PlatformerStateTags.h>
#include <Physics/PhysicsBodyHandle.h>
#include <memory>

// ── PlatformerScene ───────────────────────────────────────────────────────────
// Owns the tile world, physics world (via PhysicsLayer), score binding, and
// contact-event dispatch.  InitPhysics() is called in the constructor before
// tiles and the character are created, so all bodies are registered in the
// same Box2D world.  ShutdownPhysics() is called automatically by
// SceneManagerLayer::UnloadScene() after the scene (and all its handles) are destroyed.
//
// Boundary bodies (m_leftWall, m_rightWall, m_worldFloor) handle all wall/floor
// containment: Box2D static geometry keeps the character within bounds and
// catches fall-through.
class PlatformerScene : public SceneObject, public IScriptableObject
{
public:
    REGISTER_SCENE("platformer", PlatformerScene)

    PlatformerScene(Renderer2D* Renderer, SDL_Window* Window);

    void Update()                      override;
    void Draw(float deltaTime)         override;
    bool HandleEvent(SDL_Event& Event) override;

    void RegisterObject(sol::state& Lua)   override;
    void UnregisterObject(sol::state& Lua) override;

    static constexpr const char* SCORE_KEY           = PlatformerHudViewModel::SCORE_KEY;
    static constexpr const char* PLATFORMER_DATA_KEY = "platformerData";

private:
    static constexpr float REF_W = 1600.0f;
    static constexpr float REF_H = 1200.0f;

    int             m_score        = 0;
    DataBinding m_scoreBinding;

    // ── Physics boundary bodies ────────────────────────────────────────────────
    // Created once in the constructor so Box2D enforces world edges without any
    // game-code clamping.  Destroyed automatically via RAII when the scene ends.
    PhysicsBodyHandle m_leftWall;
    PhysicsBodyHandle m_rightWall;
    PhysicsBodyHandle m_worldFloor;

    TileWorld m_world;
    std::unique_ptr<PlatformerCharacter> m_character;

    void checkCoinCollection();
};
