#include <Sprite/Sprite.h>
#include <ServiceLocator.h>
#include <Layers/SDLLayer.h>

// ── Sprite::Load ──────────────────────────────────────────────────────────────
std::expected<Sprite, std::string>
Sprite::Load(const char* VirtualPath, int PaddingH, int PaddingV)
{
    auto Result = LoadTextureFromPhysFS(VirtualPath);
    if (!Result)
        return std::unexpected(Result.error());

    float TexW = 0.0f, TexH = 0.0f;
    if (!SDL_GetTextureSize(Result->get(), &TexW, &TexH))
        return std::unexpected(std::string("Sprite: SDL_GetTextureSize failed: ") + SDL_GetError());

    Sprite S;
    S.m_texture  = std::move(*Result);
    S.m_width    = static_cast<int>(TexW);
    S.m_height   = static_cast<int>(TexH);
    S.m_paddingH = PaddingH;
    S.m_paddingV = PaddingV;
    return S;
}

// ── Sprite::Draw ──────────────────────────────────────────────────────────────
// Applies PaddingH / PaddingV as an inset on the destination rect before
// passing to SDL.  The source texture is always sampled at full size (nullptr
// srcRect = whole texture).
void Sprite::Draw(SDL_Renderer* Renderer, const SDL_FRect& Dest,
                  SDL_FlipMode FlipMode) const
{
    if (!m_texture) return;

    // Inset destination by padding; source is the full texture (nullptr).
    const SDL_FRect InsetDest{
        Dest.x + static_cast<float>(m_paddingH),
        Dest.y + static_cast<float>(m_paddingV),
        Dest.w - static_cast<float>(2 * m_paddingH),
        Dest.h - static_cast<float>(2 * m_paddingV)
    };

    if (FlipMode == SDL_FLIP_NONE)
        SDL_RenderTexture(Renderer, m_texture.get(), nullptr, &InsetDest);
    else
        SDL_RenderTextureRotated(Renderer, m_texture.get(), nullptr, &InsetDest,
                                 0.0, nullptr, FlipMode);
}

