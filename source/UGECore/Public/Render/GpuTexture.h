#pragma once
#include <SDL3/SDL.h>
#include <expected>
#include <string>

// ── GpuTexture ────────────────────────────────────────────────────────────────
// RAII wrapper over an SDL_GPUTexture plus its pixel dimensions. It is the
// engine's texture asset type under the SDL_GPU migration, replacing the legacy
// SDL 2D renderer texture (see docs/sprints/SDL_GPU_Migration.md, P2).
//
// A GpuTexture holds a non-owning pointer to the SDL_GPUDevice that created it
// (needed to release the underlying texture) and owns the SDL_GPUTexture handle.
// It is move-only; moving transfers ownership and leaves the source empty.
//
// Textures are created in RGBA8 (SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) with
// SAMPLER usage, suitable for the Renderer2D textured-quad pipeline.
class GpuTexture
{
public:
    GpuTexture() = default;
    ~GpuTexture();

    // Move-only.
    GpuTexture(const GpuTexture&)            = delete;
    GpuTexture& operator=(const GpuTexture&) = delete;
    GpuTexture(GpuTexture&& Other) noexcept;
    GpuTexture& operator=(GpuTexture&& Other) noexcept;

    // Upload an SDL_Surface into a new GPU texture. The surface is converted to
    // RGBA32 as required; the caller retains ownership of Surface. Performs a
    // one-shot transfer-buffer upload on a dedicated command buffer (submitted
    // immediately), so it is safe to call outside the per-frame command buffer.
    // Returns an error string on failure.
    [[nodiscard]] static std::expected<GpuTexture, std::string>
    FromSurface(SDL_GPUDevice* Device, SDL_Surface* Surface);

    [[nodiscard]] SDL_GPUTexture* Handle() const { return m_texture; }
    [[nodiscard]] int             Width()  const { return m_width;   }
    [[nodiscard]] int             Height() const { return m_height;  }
    [[nodiscard]] bool            IsValid() const { return m_texture != nullptr; }

private:
    void release();

    SDL_GPUDevice*  m_device  = nullptr;  // non-owning; needed to release m_texture
    SDL_GPUTexture* m_texture = nullptr;  // owned
    int             m_width   = 0;
    int             m_height  = 0;
};
