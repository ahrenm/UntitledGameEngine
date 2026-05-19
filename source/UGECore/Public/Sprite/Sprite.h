#pragma once
#include <Layers/SDLLayer.h>   // UniqueTexture, LoadTextureFromPhysFS
#include <expected>
#include <string>

// ── Sprite ────────────────────────────────────────────────────────────────────
// Owns a single SDL texture loaded from the PhysFS virtual filesystem.
// Unlike SpriteSheet there are no frames — the entire texture is the sprite.
//
// Optional padding shrinks the destination rect at draw time (same convention
// as SpriteSheet): PaddingH insets left + right by PaddingH px each;
// PaddingV insets top + bottom by PaddingV px each.  The source texture is
// always sampled at its full size.
//
// Typical usage in a Scene constructor:
//
//   if (auto S = Sprite::Load("assets/ui/icon.png"))
//       m_icon = std::move(*S);
//
// Drawing (inside Tick()):
//
//   SDL_FRect Dest{ x * SX, y * SY, W * SX, H * SY };
//   m_icon.Draw(m_renderer, Dest);
//   m_icon.Draw(m_renderer, Dest, SDL_FLIP_HORIZONTAL);  // mirrored
class Sprite
{
public:
    // Non-copyable; movable.
    Sprite()                         = default;
    Sprite(const Sprite&)            = delete;
    Sprite& operator=(const Sprite&) = delete;
    Sprite(Sprite&&)                 = default;
    Sprite& operator=(Sprite&&)      = default;

    // Load a sprite from the PhysFS virtual filesystem.
    // Both the renderer and PhysFSLayer are resolved internally via ServiceLocator.
    // PaddingH / PaddingV : destination-rect inset applied at draw time (pixels
    //                       in screen space).  The source is sampled at full size.
    // Returns an error string on failure.
    [[nodiscard]] static std::expected<Sprite, std::string>
    Load(const char* VirtualPath,
         int PaddingH = 0, int PaddingV = 0);

    // Render the sprite scaled to fit Dest.
    // FlipMode defaults to SDL_FLIP_NONE; pass SDL_FLIP_HORIZONTAL to mirror
    // left↔right, or SDL_FLIP_VERTICAL to mirror top↔bottom.
    void Draw(SDL_Renderer* Renderer, const SDL_FRect& Dest,
              SDL_FlipMode FlipMode = SDL_FLIP_NONE) const;

    // Natural texture dimensions in pixels.
    [[nodiscard]] int Width()  const { return m_width;  }
    [[nodiscard]] int Height() const { return m_height; }

    // Padding values set at load time.
    [[nodiscard]] int PaddingH() const { return m_paddingH; }
    [[nodiscard]] int PaddingV() const { return m_paddingV; }

    // Effective rendered dimensions after padding is applied.
    // Use these to derive aspect-correct destination rects.
    [[nodiscard]] int RenderedW() const { return m_width  - 2 * m_paddingH; }
    [[nodiscard]] int RenderedH() const { return m_height - 2 * m_paddingV; }

    // Returns true if this sprite holds a valid texture.
    [[nodiscard]] bool IsValid() const { return m_texture != nullptr; }

private:
    UniqueTexture m_texture{ nullptr };
    int           m_width    = 0;
    int           m_height   = 0;
    int           m_paddingH = 0;
    int           m_paddingV = 0;
};

