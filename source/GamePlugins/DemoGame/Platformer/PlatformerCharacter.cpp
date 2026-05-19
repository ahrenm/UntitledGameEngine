#include "PlatformerCharacter.h"
#include <PlatformerStateTags.h>
#include <Layers/UDEDataLayer.h>
#include <algorithm>
#include <cmath>
#include <sol/sol.hpp>

// ─────────────────────────────────────────────────────────────────────────────

PlatformerCharacter::PlatformerCharacter(SDL_Renderer* Renderer, SDL_Window* Window,
                                         TileWorld& World, const char* DatasetKey)
    : SceneObject(Renderer, Window), m_world(World)
{
    m_collisionBox = BoxCollision(m_playerX + COLLISION_X_OFFSET, m_playerY,
                                   COLLISION_W, PLAYER_H, "Player");

    // Cache the data layer pointer once — used every frame in Update() for nameplate writes.
    m_dataLayer = GetDataLayer();

    // ── Character identity — bound to persistent AppState ────────────────────
    APPSTATE_BIND_LOCAL_STRING(m_characterNameBinding, "character.name", m_characterName);

    // ── Physics — bind to transient AppState (published by PlatformerScene) ───
    // Cache in local variables for quick access during Update() & Tick()
    APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT(m_gravityBinding,       TAG_GRAVITY,        m_readGravity);
    APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT(m_speedBinding,         TAG_SPEED,          m_readSpeed);
    APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT(m_jumpHeightBinding,    TAG_JUMP_HEIGHT,    m_readJumpHeight);
    APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT(m_groundEpsilonBinding, TAG_GROUND_EPSILON, m_readGroundEpsilon);

    // ── Sprite sheets ─────────────────────────────────────────────────────────
    buildAnimatedSprite(DatasetKey, "character.walk", m_walkSheet, m_walkAnim);
    buildAnimatedSprite(DatasetKey, "character.jump", m_jumpSheet, m_jumpAnim);
    buildAnimatedSprite(DatasetKey, "character.fall", m_fallSheet, m_fallAnim);

    if (m_walkSheet.IsValid())
    {
        const float Aspect = static_cast<float>(m_walkSheet.RenderedW())
                           / static_cast<float>(m_walkSheet.RenderedH());
        m_playerRenderH = PLAYER_H;
        m_playerRenderW = m_playerRenderH * Aspect;
    }
}

// ── Configuration ─────────────────────────────────────────────────────────────

// ── buildAnimatedSprite (file-local helper) ─────────────────────────────────────
// Reads all sprite-sheet and animation parameters for one animation from the
// TOML dataset under SectionPrefix (e.g. "character.walk"), loads the sheet,
// and wires it to OutAnim.  OutSheet must outlive OutAnim.
// Keys read (relative to SectionPrefix):
//   sprite, frame_w, frame_h, padding_h, start_frame, frame_count, frame_duration
void PlatformerCharacter::buildAnimatedSprite(const char*         DatasetKey,
                              const std::string&  SectionPrefix,
                              SpriteSheet&        OutSheet,
                              AnimatedSprite&     OutAnim)
{
    auto& tomlDataStore = m_dataLayer->Data;

    const auto get = [&](const char* Leaf) { return SectionPrefix + "." + Leaf; };

    const auto SpritePath    = tomlDataStore.GetString(DatasetKey, get("sprite")).value_or("");
    const int  FrameW        = tomlDataStore.GetInt   (DatasetKey, get("frame_w")       ).value_or(64);
    const int  FrameH        = tomlDataStore.GetInt   (DatasetKey, get("frame_h")       ).value_or(64);
    const int  PaddingH      = tomlDataStore.GetInt   (DatasetKey, get("padding_h")     ).value_or(0);
    const int  StartFrame    = tomlDataStore.GetInt   (DatasetKey, get("start_frame")   ).value_or(0);
    const int  FrameCount    = tomlDataStore.GetInt   (DatasetKey, get("frame_count")   ).value_or(1);
    const float FrameDuration = tomlDataStore.GetFloat(DatasetKey, get("frame_duration")).value_or(0.1f);

    if (SpritePath.empty()) return;

    auto Result = SpriteSheet::Load(SpritePath.c_str(), FrameW, FrameH, PaddingH);
    if (!Result) return;

    OutSheet              = std::move(*Result);
    OutAnim.Sheet         = &OutSheet;
    OutAnim.StartFrame    = StartFrame;
    OutAnim.FrameCount    = FrameCount;
    OutAnim.FrameDuration = FrameDuration;
}

// ── Jump (public) ─────────────────────────────────────────────────────────────
// Programmatically triggers a jump when the character is grounded.
void PlatformerCharacter::Jump()
{
    if (!m_isJumping)
    {
        m_isJumping = true;
        m_velocityY = -std::sqrt(2.0f * m_readGravity * m_readJumpHeight);
    }
}

// ── ScriptableObject ──────────────────────────────────────────────────────────
void PlatformerCharacter::RegisterObject(sol::state& Lua)
{
    auto Table = Lua.create_named_table("plat2d");
    Table.set_function("Jump", [this]() { Jump(); });
}

void PlatformerCharacter::UnregisterObject(sol::state& Lua)
{
    Lua["plat2d"] = sol::nil;
}


//END INIT CODE
//BEGIN RUN CODE



// ── HandleEvent ───────────────────────────────────────────────────────────────
// Stores key-down / key-up flags for Update() to consume.
bool PlatformerCharacter::HandleEvent(SDL_Event& Event)
{
    if (Event.type == SDL_EVENT_KEY_DOWN || Event.type == SDL_EVENT_KEY_UP)
    {
        const bool Pressed = (Event.type == SDL_EVENT_KEY_DOWN);
        switch (Event.key.scancode)
        {
            case SDL_SCANCODE_LEFT:  m_moveLeft    = Pressed; return true;
            case SDL_SCANCODE_RIGHT: m_moveRight   = Pressed; return true;
            case SDL_SCANCODE_SPACE:
                if (Pressed) m_jumpPressed = true;  // one-shot; cleared in Update
                return true;
            default: break;
        }
    }
    return false;
}

// ── Tile collision helpers ────────────────────────────────────────────────────

std::optional<float> PlatformerCharacter::snapVertical(float ColL, float ColR,
                                                        float CandidateY,
                                                        bool  FallingDown) const
{
    // Build a probe box matching the candidate position and query the broad-phase grid.
    // Default-constructed to avoid auto-registration with the collision registry.
    BoxCollision probe;
    probe.X = ColL;
    probe.Y = CandidateY;
    probe.W = ColR - ColL;
    probe.H = PLAYER_H;

    std::vector<BoxCollision*> hits;
    if (!m_world.Walls.CollisionGrid.Intersects(&probe, hits))
        return std::nullopt;

    std::optional<float> result;
    for (const BoxCollision* Hit : hits)
    {
        if (FallingDown)
        {
            const float Snap = Hit->Top() - PLAYER_H;
            if (!result || Snap < *result) result = Snap;
        }
        else
        {
            const float Snap = Hit->Bottom();
            if (!result || Snap > *result) result = Snap;
        }
    }
    return result;
}


// ── Update ────────────────────────────────────────────────────────────────────
void PlatformerCharacter::Update()
{
    const float CandL        = m_playerX - m_readSpeed;
    const float CandR        = m_playerX + m_readSpeed;
    const float PlayerBottom = m_playerY + PLAYER_H;
    const float CL           = m_playerX + COLLISION_X_OFFSET;

    bool overlapLeft  = false;
    bool overlapRight = false;
    bool grounded     = false;

    // ── Horizontal and ground collision via broad-phase grid ─────────────────
    // Probe boxes are default-constructed to avoid auto-registration.
    std::vector<BoxCollision*> hits;

    if (m_moveLeft)
    {
        BoxCollision boxL;
        boxL.X = CandL + COLLISION_X_OFFSET; boxL.Y = m_playerY;
        boxL.W = COLLISION_W;                boxL.H = PLAYER_H;
        overlapLeft = m_world.Walls.CollisionGrid.Intersects(&boxL, hits);
        hits.clear();
    }
    if (m_moveRight)
    {
        BoxCollision boxR;
        boxR.X = CandR + COLLISION_X_OFFSET; boxR.Y = m_playerY;
        boxR.W = COLLISION_W;                boxR.H = PLAYER_H;
        overlapRight = m_world.Walls.CollisionGrid.Intersects(&boxR, hits);
        hits.clear();
    }
    if (!m_isJumping)
    {
        // Thin horizontal strip centred on the player's feet within ground epsilon.
        BoxCollision groundProbe;
        groundProbe.X = CL;
        groundProbe.Y = PlayerBottom - m_readGroundEpsilon;
        groundProbe.W = COLLISION_W;
        groundProbe.H = m_readGroundEpsilon * 2.0f;
        grounded = m_world.Walls.CollisionGrid.Intersects(&groundProbe, hits);
    }

    // ── Apply horizontal movement ─────────────────────────────────────────────
    if (m_moveLeft  && !overlapLeft)  { m_playerX -= m_readSpeed; m_facingLeft = true;  }
    if (m_moveRight && !overlapRight) { m_playerX += m_readSpeed; m_facingLeft = false; }

    // ── Edge-walk detection ───────────────────────────────────────────────────
    if (!m_isJumping && !grounded)
        m_isJumping = true;

    // ── Jump trigger ──────────────────────────────────────────────────────────
    if (m_jumpPressed && !m_isJumping)
    {
        m_isJumping = true;
        m_velocityY = -std::sqrt(2.0f * m_readGravity * m_readJumpHeight);
        m_jumpAnim.Reset();
        m_fallAnim.Reset();
    }
    m_jumpPressed = false;

    // ── Animation advance ─────────────────────────────────────────────────────
    if (!m_isJumping && (m_moveLeft || m_moveRight))
        m_walkAnim.Update(FRAME_DT);
    if (m_isJumping && m_velocityY < 0.0f)
        m_jumpAnim.Update(FRAME_DT);
    if (m_isJumping && m_velocityY >= 0.0f)
        m_fallAnim.Update(FRAME_DT);

    // ── Vertical physics ──────────────────────────────────────────────────────
    m_velocityY += m_readGravity;
    const float CandidateY = m_playerY + m_velocityY;
    const float ColL = m_playerX + COLLISION_X_OFFSET;
    const float ColR = ColL + COLLISION_W;

    if (m_velocityY > 0.0f)
    {
        if (const auto Snap = snapVertical(ColL, ColR, CandidateY, /*FallingDown=*/true))
        {
            m_playerY   = *Snap;
            m_velocityY = 0.0f;
            m_isJumping = false;
        }
        else { m_playerY = CandidateY; }
    }
    else
    {
        if (const auto Snap = snapVertical(ColL, ColR, CandidateY, /*FallingDown=*/false))
        {
            m_playerY   = *Snap;
            m_velocityY = 0.0f;
        }
        else { m_playerY = CandidateY; }
    }

    // ── Clamp to reference-resolution bounds ──────────────────────────────────
    if (m_playerX < 0.0f)             m_playerX = 0.0f;
    if (m_playerX > REF_W - PLAYER_W) m_playerX = REF_W - PLAYER_W;

    if (m_playerY > REF_H - PLAYER_H)
    {
        m_playerY   = GROUND_Y - PLAYER_H;
        m_velocityY = 0.0f;
        m_isJumping = false;
    }

    // ── Sync collision box to final position  ──
    m_collisionBox.X = m_playerX + COLLISION_X_OFFSET;
    m_collisionBox.Y = m_playerY;

    // ── Gated transient writes: character nameplate anchor (if any) ───────────
    // Only written when a name is set — avoids per-frame ServiceLocator cost
    // and unnecessary ViewModel subscription firings when no name is active.
    if (!m_characterName.empty() && m_dataLayer)
    {
        // Anchor point: top-centre of the character sprite in reference space.
        const float AnchorX = m_playerX + PLAYER_W / 2.0f;
        const float AnchorY = m_playerY;
        m_dataLayer->Transient.Set(TAG_NAMEPLATE_X.data(), AppStateValue{ AnchorX });
        m_dataLayer->Transient.Set(TAG_NAMEPLATE_Y.data(), AppStateValue{ AnchorY });
    }
}

// ── Tick ──────────────────────────────────────────────────────────────────────
void PlatformerCharacter::Tick(float /*DeltaTime*/)
{
    int W = 0, H = 0;
    SDL_GetWindowSize(m_window, &W, &H);

    const float SX = static_cast<float>(W) / REF_W;
    const float SY = static_cast<float>(H) / REF_H;

    const SDL_FlipMode Flip  = m_facingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    const float        OffX  = (PLAYER_W - m_playerRenderW) / 2.0f;
    const SDL_FRect    Rect  =
    {
        (m_playerX + OffX) * SX,
        m_playerY          * SY,
        m_playerRenderW    * SX,
        m_playerRenderH    * SY
    };

    if (m_isJumping && m_velocityY < 0.0f && m_jumpSheet.IsValid())
    {
        m_jumpAnim.Draw(m_renderer, Rect, Flip);
    }
    else if (m_isJumping && m_fallSheet.IsValid())
    {
        m_fallAnim.Draw(m_renderer, Rect, Flip);
    }
    else if (!m_isJumping && m_walkSheet.IsValid())
    {
        m_walkAnim.Draw(m_renderer, Rect, Flip);
    }
    else
    {
        // Fallback colour rect when sprite sheets are not loaded.
        const SDL_FRect FbRect{ m_playerX * SX, m_playerY * SY, PLAYER_W * SX, PLAYER_H * SY };
        SDL_SetRenderDrawColor(m_renderer, 30, 100, 220, 255);
        SDL_RenderFillRect(m_renderer, &FbRect);
    }
}
