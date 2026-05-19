#pragma once
#include <Sprite/SpriteSheet.h>
#include <SDL3/SDL.h>

// ── AnimatedSprite ────────────────────────────────────────────────────────────
// Stateful animation controller referencing a SpriteSheet.
// Does not own the sheet — the scene that creates the AnimatedSprite must ensure
// the SpriteSheet outlives it.
//
// Frames within the sheet are addressed as a contiguous sub-range:
//   [StartFrame, StartFrame + FrameCount)
//
// Timing is expressed in seconds so playback speed is frame-rate-independent.
// At the engine's 30 FPS fixed step, pass DeltaSeconds = 1.0f / 30.0f each tick.
//
// Typical usage inside a Scene:
//
//   // Construction (member initialiser)
//   m_walkAnim = AnimatedSprite{ &m_playerSheet,
//                                /*StartFrame=*/  0,
//                                /*FrameCount=*/  4,
//                                /*FrameDuration=*/0.12f };
//
//   // Update (once per frame, before Tick)
//   m_walkAnim.Update(1.0f / 30.0f);
//
//   // Tick (render pass)
//   SDL_FRect Dest{ ... };
//   m_walkAnim.Draw(m_renderer, Dest);
struct AnimatedSprite
{
    const SpriteSheet* Sheet         = nullptr;  // non-owning
    int                StartFrame    = 0;        // first frame index in the sheet
    int                FrameCount    = 1;        // number of frames to cycle through
    float              FrameDuration = 0.1f;     // seconds per frame

    // ── Advance the animation by DeltaSeconds. ────────────────────────────────
    // Call once per Update() pass.
    void Update(float DeltaSeconds)
    {
        if (!Sheet || FrameCount <= 1) return;

        m_elapsed += DeltaSeconds;
        while (m_elapsed >= FrameDuration)
        {
            m_elapsed      -= FrameDuration;
            m_currentFrame  = (m_currentFrame + 1) % FrameCount;
        }
    }

    // ── Draw the current frame scaled to fit Dest. ────────────────────────────
    // FlipMode mirrors the sprite — use SDL_FLIP_HORIZONTAL to reverse the
    // sprite when the character changes walking direction.
    void Draw(SDL_Renderer* Renderer, const SDL_FRect& Dest,
              SDL_FlipMode FlipMode = SDL_FLIP_NONE) const
    {
        if (!Sheet) return;
        Sheet->Draw(Renderer, StartFrame + m_currentFrame, Dest, FlipMode);
    }

    // ── Reset to the first frame and clear elapsed time. ─────────────────────
    void Reset()
    {
        m_currentFrame = 0;
        m_elapsed      = 0.0f;
    }

    // ── Current absolute frame index in the sheet. ────────────────────────────
    [[nodiscard]] int CurrentFrameIndex() const { return StartFrame + m_currentFrame; }

private:
    int   m_currentFrame = 0;
    float m_elapsed      = 0.0f;
};

