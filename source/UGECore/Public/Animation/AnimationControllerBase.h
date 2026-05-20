#pragma once
#include <GameClasses/GameObjectBase.h>
#include <Sprite/AnimatedSprite.h>
#include <Sprite/SpriteSheet.h>
#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <unordered_map>

// ── AnimationControllerBase ───────────────────────────────────────────────────
// Generic TOML-driven animation controller for game objects.
// Manages a named registry of SpriteSheet + AnimatedSprite pairs and renders
// the currently active animation through a bound SDL_Renderer.
//
// Lifecycle
// ---------
//   1. Construct with the SDL_Renderer* that will be used for all Draw calls.
//   2. Call LoadAnimation() for each animation clip (reads from UGEDataLayer).
//   3. Each frame: call Update() then Tick(DeltaTime).
//
// Frame loop contract
// -------------------
//   Update()          — Reserved for AnimationNotify.  Future per-frame event
//                       callbacks (e.g. footstep sounds, hit detection windows)
//                       that inform game state will fire here, before Tick().
//   Tick(DeltaTime)   — Advances the active animation's frame timer.
//   Draw(Rect, Flip)  — Renders the active animation at the given screen rect.
//
// Derive in a game plugin to add state-driven animation selection.
// SpriteSheets are heap-allocated so their addresses remain stable across
// map insertions; AnimatedSprite pointers into them are never dangled.
class AnimationControllerBase : public GameObjectBase
{
public:
    explicit AnimationControllerBase(SDL_Renderer* Renderer);
    virtual ~AnimationControllerBase() = default;

    // ── Asset loading ─────────────────────────────────────────────────────────
    // Reads animation config from the TOML dataset DatasetKey under SectionPrefix
    // (e.g. "character.walk") and registers the clip under AnimName.
    // Expected TOML keys (all relative to SectionPrefix):
    //   sprite, frame_w, frame_h, padding_h, start_frame, frame_count, frame_duration
    void LoadAnimation(const char* DatasetKey, const std::string& SectionPrefix,
                       const std::string& AnimName);

    // ── Playback control ──────────────────────────────────────────────────────
    void SetCurrentAnimation(const std::string& AnimName);
    void ResetAnimation(const std::string& AnimName);

    [[nodiscard]] bool       IsAnimationValid(const std::string& AnimName) const;

    // Returns {RenderedW, RenderedH} for the named animation's sheet.
    // Returns {0, 0} if the animation has not been loaded or its sheet is invalid.
    [[nodiscard]] SDL_FPoint GetRenderSize(const std::string& AnimName) const;

    // ── Frame loop ────────────────────────────────────────────────────────────
    // Reserved for AnimationNotify — future per-frame event callbacks that
    // inform game state will fire here, before Tick() is called each frame.
    virtual void Update() {}

    // Advances the active animation's frame timer by DeltaTime (seconds).
    virtual void Tick(float DeltaTime);

    // Draws the active animation frame scaled to fit Rect.
    void Draw(const SDL_FRect& Rect, SDL_FlipMode FlipMode = SDL_FLIP_NONE) const;

protected:
    SDL_Renderer* m_renderer = nullptr;

    struct AnimEntry
    {
        std::unique_ptr<SpriteSheet> Sheet;  // heap-allocated for stable pointer
        AnimatedSprite               Anim;   // Anim.Sheet points into Sheet above
    };

    [[nodiscard]] AnimEntry*       getEntry(const std::string& Name);
    [[nodiscard]] const AnimEntry* getEntry(const std::string& Name) const;

private:
    std::unordered_map<std::string, AnimEntry> m_animations;
    std::string m_currentAnim;
};

