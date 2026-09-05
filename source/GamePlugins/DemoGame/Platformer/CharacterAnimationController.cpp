#include "CharacterAnimationController.h"
#include <Layers/SDLLayer.h>
#include <Render/Renderer2D.h>

// ── Constructor ───────────────────────────────────────────────────────────────
CharacterAnimationController::CharacterAnimationController(Renderer2D* Renderer)
    : AnimationControllerBase(Renderer)
{}

// ── LoadAnimations ────────────────────────────────────────────────────────────
void CharacterAnimationController::LoadAnimations(const char* DatasetKey)
{
    LoadAnimation(DatasetKey, "character.walk", ANIM_WALK);
    LoadAnimation(DatasetKey, "character.jump", ANIM_JUMP);
    LoadAnimation(DatasetKey, "character.fall", ANIM_FALL);
}

// ── Tick ──────────────────────────────────────────────────────────────────────
void CharacterAnimationController::Tick(float DeltaTime, const CharacterAnimInput& Input)
{
    // ── 1. Flip mode ──────────────────────────────────────────────────────────
    const SDL_FlipMode Flip = Input.FacingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    // ── 2. Screen-space rect ──────────────────────────────────────────────────
    // Scale the sheet's raw pixel dimensions to the logical SpriteH game-unit
    // height while preserving aspect ratio — matching the original pipeline that
    // kept render height == PLAYER_H and derived render width from the aspect.
    // Then centre the scaled sprite within the SpriteW collision boundary and
    // convert to SDL screen space via the SDLLayer global camera.
    //
    // Note: always uses SDLLayer::GetCamera() (global camera). If the owning
    // scene applies a per-scene camera override for scrolling, adjust
    // Input.PlayerX / PlayerY to be relative to that camera before passing in.
    const SDL_FPoint rawSize = GetRenderSize(ANIM_WALK);
    const float      scaledH = Input.SpriteH;
    const float      scaledW = (rawSize.y > 0.0f)
                                   ? scaledH * (rawSize.x / rawSize.y)
                                   : Input.SpriteW;
    const float      offX    = (Input.SpriteW - scaledW) / 2.0f;

    SDL_FRect rect{};
    if (auto* sdl = GetSDLLayer())
    {
        rect = sdl->GetCamera().WorldRect(
            Input.PlayerX + offX, Input.PlayerY,
            scaledW, scaledH,
            static_cast<float>(sdl->RefHeight()));
    }

    // ── 3-5. Select, advance, draw ────────────────────────────────────────────
    if (!Input.IsGrounded && Input.VelocityY > JUMP_VY_THRESHOLD
        && IsAnimationValid(ANIM_JUMP))
    {
        SetCurrentAnimation(ANIM_JUMP);
        AnimationControllerBase::Tick(DeltaTime);
        Draw(rect, Flip);
    }
    else if (!Input.IsGrounded && IsAnimationValid(ANIM_FALL))
    {
        SetCurrentAnimation(ANIM_FALL);
        AnimationControllerBase::Tick(DeltaTime);
        Draw(rect, Flip);
    }
    else if (Input.IsGrounded && IsAnimationValid(ANIM_WALK))
    {
        SetCurrentAnimation(ANIM_WALK);
        if (Input.IsMoving)
            AnimationControllerBase::Tick(DeltaTime);
        Draw(rect, Flip);
    }
    else
    {
        // No valid sheets loaded — render a solid-colour placeholder at the
        // full logical sprite bounds (no centering offset).
        SDL_FRect fallbackRect{};
        if (auto* sdl = GetSDLLayer())
        {
            fallbackRect = sdl->GetCamera().WorldRect(
                Input.PlayerX, Input.PlayerY,
                Input.SpriteW, Input.SpriteH,
                static_cast<float>(sdl->RefHeight()));
        }
        DrawFallback(fallbackRect);
    }
}

// ── DrawFallback ──────────────────────────────────────────────────────────────
void CharacterAnimationController::DrawFallback(const SDL_FRect& Rect)
{
    if (m_renderer2D)
        m_renderer2D->DrawColoredQuad(Rect, SDL_FColor{ 30 / 255.0f, 100 / 255.0f, 220 / 255.0f, 1.0f });
}

