#pragma once
#include <../../UGECore/Public/GameClasses/SceneObject.h>
#include <GameClasses/BoxCollision.h>
#include <IScriptableObject.h>
#include <Sprite/AnimatedSprite.h>
#include <Sprite/SpriteSheet.h>
#include <TileWorld.h>
#include <optional>
#include <vector>

class UGEDataLayer; // forward declaration — cached for per-frame transient writes

// ── PlatformerCharacter ───────────────────────────────────────────────────────
//
// Owns all player-character state: input, physics, animation, rendering, and
// tile collision.  PlatformerScene passes a non-owning reference to m_world at
// construction; coin collection is handled by PlatformerScene via the grid.
//
class PlatformerCharacter : public SceneObject, public IScriptableObject
{
public:
    // ── Collision geometry (public so PlatformerScene can size tiles consistently)
    static constexpr float PLAYER_W           = 100.0f;
    static constexpr float PLAYER_H           = 100.0f;
    static constexpr float COLLISION_W        = PLAYER_W * 0.85f;
    static constexpr float COLLISION_X_OFFSET = (PLAYER_W - COLLISION_W) / 2.0f;

    PlatformerCharacter(SDL_Renderer* Renderer, SDL_Window* Window,
                        TileWorld& World, const char* DatasetKey);

    void Update()                      override;
    void Tick(float DeltaTime)         override;
    bool HandleEvent(SDL_Event& Event) override;

    // ── ScriptableObject — exposes plat2d.Jump() to Lua ──────────────────────
    void Jump();
    void RegisterObject(sol::state& Lua)   override;
    void UnregisterObject(sol::state& Lua) override;

    // Exposes the player's AABB so PlatformerScene can run grid queries against it.
    [[nodiscard]] BoxCollision& CollisionBox() { return m_collisionBox; }

    // ── Configuration — called by PlatformerScene after loading DataLayer ─────
private:

    // ── Reference resolution ──────────────────────────────────────────────────
    static constexpr float REF_W    = 1600.0f;
    static constexpr float REF_H    = 1200.0f;
    static constexpr float GROUND_Y = REF_H - 160;
    static constexpr float FRAME_DT = 1.0f / 30.0f;

    TileWorld& m_world;   // non-owning ref; owned by PlatformerScene
    UGEDataLayer*      m_dataLayer = nullptr; // cached non-owning; for per-frame transient writes

    // ── Character identity — bound to persistent AppState ────────────────────
    std::string     m_characterName;
    AppStateBinding m_characterNameBinding;

    // ── Physics tunables — bound to transient AppState in constructor ─────────
    float m_readGravity       = 0.0f;
    float m_readSpeed         = 0.0f;
    float m_readJumpHeight    = 0.0f;
    float m_readGroundEpsilon = 0.0f;

    AppStateBinding m_gravityBinding;
    AppStateBinding m_speedBinding;
    AppStateBinding m_jumpHeightBinding;
    AppStateBinding m_groundEpsilonBinding;

    // ── Sprites / animation ───────────────────────────────────────────────────
    SpriteSheet    m_walkSheet;
    AnimatedSprite m_walkAnim;
    SpriteSheet    m_jumpSheet;
    AnimatedSprite m_jumpAnim;
    SpriteSheet    m_fallSheet;
    AnimatedSprite m_fallAnim;

    bool  m_facingLeft    = false;
    float m_playerRenderW = PLAYER_W;
    float m_playerRenderH = PLAYER_H;

    // ── Physics / position state ──────────────────────────────────────────────
    float m_playerX   = (REF_W - PLAYER_W) / 2.0f;
    float m_playerY   = GROUND_Y - PLAYER_H;
    float m_velocityY = 0.0f;
    bool  m_isJumping = false;

    // ── Collision box ─────────────────────────────────────────────────────────
    BoxCollision m_collisionBox;

    // ── Input flags ───────────────────────────────────────────────────────────
    bool m_moveLeft    = false;
    bool m_moveRight   = false;
    bool m_jumpPressed = false;

    // ── Tile collision helpers ────────────────────────────────────────────────
    [[nodiscard]] std::optional<float> snapVertical(float ColL, float ColR,
                                                    float CandidateY,
                                                    bool FallingDown) const;

    void buildAnimatedSprite(const char*         DatasetKey,
                              const std::string&  SectionPrefix,
                              SpriteSheet&        OutSheet,
                              AnimatedSprite&     OutAnim);

};
