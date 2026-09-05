#include <Sprite/SpriteSheet.h>
#include <Render/Renderer2D.h>
#include <Layers/SDLLayer.h>
#include <ServiceLocator.h>

// ── SpriteSheet::Load ─────────────────────────────────────────────────────────
std::expected<SpriteSheet, std::string>
SpriteSheet::Load(const char* VirtualPath, int FrameW, int FrameH,
                  int PaddingH, int PaddingV)
{
    if (FrameW <= 0 || FrameH <= 0)
        return std::unexpected(std::string("SpriteSheet: FrameW and FrameH must be > 0"));

    auto Result = LoadTextureFromPhysFS(VirtualPath);
    if (!Result)
        return std::unexpected(Result.error());

    const int TexW = Result->Width();
    const int TexH = Result->Height();

    SpriteSheet Sheet;
    Sheet.m_texture  = std::move(*Result);
    Sheet.m_frameW   = FrameW;
    Sheet.m_frameH   = FrameH;
    Sheet.m_paddingH = PaddingH;
    Sheet.m_paddingV = PaddingV;
    // Frames are packed with no gaps in the source texture.
    Sheet.m_cols = TexW / FrameW;
    Sheet.m_rows = TexH / FrameH;

    if (Sheet.m_cols <= 0 || Sheet.m_rows <= 0)
        return std::unexpected(std::string("SpriteSheet: frame size larger than texture"));

    return Sheet;
}

// ── SpriteSheet::FrameRect ────────────────────────────────────────────────────
// Returns the source rectangle (texture pixels) for the given frame index.
// Padding is a destination-only concept and does not affect source sampling.
SDL_FRect SpriteSheet::FrameRect(int FrameIndex) const
{
    if (FrameIndex < 0 || FrameIndex >= FrameCount())
        return SDL_FRect{ 0.0f, 0.0f, 0.0f, 0.0f };

    const int Col = FrameIndex % m_cols;
    const int Row = FrameIndex / m_cols;
    return SDL_FRect{
        static_cast<float>(Col * m_frameW),
        static_cast<float>(Row * m_frameH),
        static_cast<float>(m_frameW),
        static_cast<float>(m_frameH)
    };
}

// ── SpriteSheet::Draw ─────────────────────────────────────────────────────────
// Applies PaddingH / PaddingV as an inset on the destination rect before drawing.
// The source frame is always sampled at full size.
void SpriteSheet::Draw(Renderer2D& Renderer, int FrameIndex,
                       const SDL_FRect& Dest, SDL_FlipMode FlipMode) const
{
    if (!m_texture.IsValid()) return;
    const SDL_FRect Src = FrameRect(FrameIndex);

    // Inset the destination rect by the padding values.
    const SDL_FRect InsetDest{
        Dest.x + static_cast<float>(m_paddingH),
        Dest.y + static_cast<float>(m_paddingV),
        Dest.w - static_cast<float>(2 * m_paddingH),
        Dest.h - static_cast<float>(2 * m_paddingV)
    };

    Renderer.DrawTexture(m_texture, InsetDest, &Src, FlipMode);
}

