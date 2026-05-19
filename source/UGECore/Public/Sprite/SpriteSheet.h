#pragma once
#include <Layers/SDLLayer.h>   // UniqueTexture, LoadTextureFromPhysFS
#include <expected>
#include <string>

// ── SpriteSheet ───────────────────────────────────────────────────────────────
// Owns a single texture and divides it into a uniform grid of equal-sized frames.
// Frames are indexed left-to-right, top-to-bottom starting at 0.
//
// Typical usage in a Scene constructor:
//
//   auto Sheet = SpriteSheet::Load("assets/sprites/player.png", 64, 64);
//   if (Sheet) m_playerSheet = std::move(*Sheet);
//
// Drawing a single frame (e.g. inside Tick()):
//
//   SDL_FRect Dest{ x * SX, y * SY, PLAYER_W * SX, PLAYER_H * SY };
//   m_playerSheet.Draw(m_renderer, FrameIndex, Dest);
//
// Coordinate convention: FrameRect() returns source coordinates in texture
// pixels (same units expected by SDL_RenderTexture's srcRect parameter).
class SpriteSheet
{
public:
    // Non-copyable; movable.
    SpriteSheet()                              = default;
    SpriteSheet(const SpriteSheet&)            = delete;
    SpriteSheet& operator=(const SpriteSheet&) = delete;
    SpriteSheet(SpriteSheet&&)                 = default;
    SpriteSheet& operator=(SpriteSheet&&)      = default;

    // Load a sprite sheet from the PhysFS virtual filesystem.
    // Both the renderer and PhysFSLayer are resolved internally via ServiceLocator.
    // FrameW / FrameH   : pixel dimensions of one frame in the source texture.
    //                     Frames are packed with no gaps — the grid is computed
    //                     purely from texture size / frame size.
    // PaddingH/PaddingV : destination-rect inset applied at draw time (pixels in
    //                     *screen space*).  PaddingH shrinks the rendered width by
    //                     2 × PaddingH (PaddingH from the left, PaddingH from the
    //                     right); PaddingV does the same vertically.  The source
    //                     frame is always sampled at its full FrameW × FrameH size.
    // Returns an error string on failure.
    [[nodiscard]] static std::expected<SpriteSheet, std::string>
    Load(const char* VirtualPath, int FrameW, int FrameH,
         int PaddingH = 0, int PaddingV = 0);

    // Source rectangle (texture-pixel coordinates) for the given frame index.
    // Frames are ordered left-to-right, then top-to-bottom.
    // Returns a zero rect if FrameIndex is out of range.
    [[nodiscard]] SDL_FRect FrameRect(int FrameIndex) const;

    // Render frame FrameIndex scaled to fit Dest on the given renderer.
    // FlipMode defaults to SDL_FLIP_NONE; pass SDL_FLIP_HORIZONTAL to mirror
    // left↔right (e.g. a walking sprite changing direction), or
    // SDL_FLIP_VERTICAL to mirror top↔bottom.
    void Draw(SDL_Renderer* Renderer, int FrameIndex, const SDL_FRect& Dest,
              SDL_FlipMode FlipMode = SDL_FLIP_NONE) const;

    // Total number of frames derived from texture dimensions and frame size.
    [[nodiscard]] int FrameCount() const { return m_cols * m_rows; }

    // Frame grid dimensions and padding.
    [[nodiscard]] int Cols()     const { return m_cols;     }
    [[nodiscard]] int Rows()     const { return m_rows;     }
    [[nodiscard]] int FrameW()   const { return m_frameW;   }
    [[nodiscard]] int FrameH()   const { return m_frameH;   }
    [[nodiscard]] int PaddingH() const { return m_paddingH; }
    [[nodiscard]] int PaddingV() const { return m_paddingV; }

    // Effective rendered dimensions after padding is applied (source pixels).
    // These reflect the actual visible sprite area: FrameW - 2*PaddingH wide,
    // FrameH - 2*PaddingV tall.  Use these to derive aspect-correct render rects.
    [[nodiscard]] int RenderedW() const { return m_frameW - 2 * m_paddingH; }
    [[nodiscard]] int RenderedH() const { return m_frameH - 2 * m_paddingV; }

    // Returns true if this sheet holds a valid texture.
    [[nodiscard]] bool IsValid() const { return m_texture != nullptr; }

private:
    UniqueTexture m_texture{ nullptr };
    int           m_frameW   = 0;
    int           m_frameH   = 0;
    int           m_paddingH = 0;   // horizontal pixels between columns
    int           m_paddingV = 0;   // vertical pixels between rows
    int           m_cols     = 0;
    int           m_rows     = 0;
};

