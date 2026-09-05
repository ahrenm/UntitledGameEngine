#pragma once
#include <Animation/AnimationControllerBase.h>

// ── CharacterAnimInput ────────────────────────────────────────────────────────
// Snapshot of game state passed to CharacterAnimationController::Tick() each
// render frame.  Contains all data needed to derive SDL_FlipMode, SDL_FRect,
// and the correct animation clip — keeping that logic inside the controller
// rather than scattered through the owning character.
//
// PlayerX / PlayerY are raw world-space coordinates (no centering offsets).
// SpriteW / SpriteH are the logical collision/sprite dimensions; the controller
// uses them to centre the (potentially narrower) rendered sheet and as the
// fallback rect size when no sheets are loaded.
struct CharacterAnimInput
{
    float PlayerX;   // world-space sprite left edge  (no centering offset)
    float PlayerY;   // world-space sprite bottom edge
    float SpriteW;   // logical sprite width  — used for centering and fallback
    float SpriteH;   // logical sprite height — used for fallback
    bool  FacingLeft; // true → SDL_FLIP_HORIZONTAL
    bool  IsGrounded;
    float VelocityY;  // px/s  (positive = rising, negative = falling)
    bool  IsMoving;   // true when horizontal movement is active
};

// ── CharacterAnimationController ──────────────────────────────────────────────
// Platformer-character animation controller.
// Owns walk / jump / fall clips loaded from a TOML dataset and drives the full
// animation pipeline — selection, frame advance, and draw — inside a single
// Tick() call.
//
// All animation logic lives in Tick():
//   1. Derive SDL_FlipMode from Input.FacingLeft
//   2. Derive SDL_FRect via SDLLayer camera transform
//      (centres the render sheet within the logical SpriteW/SpriteH boundary)
//   3. Select the active clip from physics/movement state
//   4. Advance the frame timer
//   5. Draw — or DrawFallback() when no sheets are loaded
//
// Camera note: Tick() always queries SDLLayer::GetCamera() (the global camera).
// If the owning scene uses a per-scene camera override for scrolling, adjust
// Input.PlayerX / PlayerY to world coordinates relative to that camera, or
// extend this class to accept a Camera2D override.
class CharacterAnimationController : public AnimationControllerBase
{
public:
    explicit CharacterAnimationController(Renderer2D* Renderer);

    // ── Asset loading ─────────────────────────────────────────────────────────
    // Loads walk, jump, and fall clips from:
    //   <DatasetKey> / character.walk.*
    //   <DatasetKey> / character.jump.*
    //   <DatasetKey> / character.fall.*
    // After this call, GetRenderSize("walk") returns the walk sheet dimensions.
    void LoadAnimations(const char* DatasetKey);

    // ── Frame loop ────────────────────────────────────────────────────────────
    // Single call-site for all animation logic each render frame.
    // Derives flip mode and screen rect from Input, selects and advances the
    // appropriate clip, then draws it (or falls back to DrawFallback).
    void Tick(float DeltaTime, const CharacterAnimInput& Input);

    // Renders a solid-colour placeholder rect when no sprite sheets are loaded.
    // Virtual — override to change the fallback appearance in derived classes.
    virtual void DrawFallback(const SDL_FRect& Rect);

private:
    static constexpr const char* ANIM_WALK = "walk";
    static constexpr const char* ANIM_JUMP = "jump";
    static constexpr const char* ANIM_FALL = "fall";

    // Threshold for classifying upward velocity as "jumping" vs "at apex / falling".
    static constexpr float JUMP_VY_THRESHOLD = 0.05f;  // px/s
};

