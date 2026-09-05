#include <Sprite/Sprite.h>
#include <Render/Renderer2D.h>
#include <ServiceLocator.h>
#include <Layers/SDLLayer.h>

// ── Sprite::Load ──────────────────────────────────────────────────────────────
std::expected<Sprite, std::string>
Sprite::Load(const char* VirtualPath, int PaddingH, int PaddingV)
{
    auto Result = LoadTextureFromPhysFS(VirtualPath);
    if (!Result)
        return std::unexpected(Result.error());

    Sprite S;
    S.m_width    = Result->Width();
    S.m_height   = Result->Height();
    S.m_texture  = std::move(*Result);
    S.m_paddingH = PaddingH;
    S.m_paddingV = PaddingV;
    return S;
}

// ── Sprite::Draw ──────────────────────────────────────────────────────────────
// Applies PaddingH / PaddingV as an inset on the destination rect before drawing.
// The source texture is always sampled at full size (nullptr srcRect).
void Sprite::Draw(Renderer2D& Renderer, const SDL_FRect& Dest,
                  SDL_FlipMode FlipMode) const
{
    if (!m_texture.IsValid()) return;

    // Inset destination by padding; source is the full texture (nullptr).
    const SDL_FRect InsetDest{
        Dest.x + static_cast<float>(m_paddingH),
        Dest.y + static_cast<float>(m_paddingV),
        Dest.w - static_cast<float>(2 * m_paddingH),
        Dest.h - static_cast<float>(2 * m_paddingV)
    };

    Renderer.DrawTexture(m_texture, InsetDest, nullptr, FlipMode);
}

