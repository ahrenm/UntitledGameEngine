#pragma once
#include <SDL3/SDL.h>
#include <expected>
#include <memory>
#include <string>

class GpuTexture;

// ── Renderer2D ────────────────────────────────────────────────────────────────
// Core convenience renderer for the common 2D case (background, sprites) under
// the SDL_GPU migration (see docs/sprints/SDL_GPU_Migration.md, P2).
//
// Renderer2D owns the textured-quad and flat-color graphics pipelines (built from
// the precompiled SPIR-V shaders under assets/shaders/), plus a linear sampler and
// a small reusable vertex/transfer buffer. It records draws into SDLLayer's shared
// per-frame command buffer — it never acquires or submits a command buffer itself.
//
// Coordinate space: draw rects are expressed in *reference pixels* (0,0 at the
// top-left, +X right, +Y down, extent RefWidth × RefHeight). Renderer2D applies a
// letterbox viewport plus an orthographic projection so reference-space content is
// centred and uniformly scaled to fit the current swapchain, matching the old
// SDL_SetRenderLogicalPresentation behaviour. The letterbox math lives here and is
// queryable by plugins via Projection() / Viewport().
//
// Each Draw* call records its own copy pass (vertex upload) + render pass
// (LOAD_OP_LOAD, so it composites over prior passes). This is intentionally simple
// for the migration; batching is a documented future optimisation.
class Renderer2D
{
public:
    // Build the pipelines/sampler for the given device. Window is used only to
    // query the swapchain texture format the pipelines must target.
    [[nodiscard]] static std::expected<std::unique_ptr<Renderer2D>, std::string>
    Create(SDL_GPUDevice* Device, SDL_Window* Window);

    ~Renderer2D();
    Renderer2D(const Renderer2D&)            = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    // Reference (logical) resolution used to compute the letterbox projection.
    void SetReferenceSize(int RefWidth, int RefHeight);

    // Bind the in-flight frame's command buffer + swapchain texture and recompute
    // the letterbox viewport/projection for the current swapchain size. Call once
    // per frame (from SDLLayer::BeginFrame) before any Draw* calls.
    void SetFrame(SDL_GPUCommandBuffer* CommandBuffer, SDL_GPUTexture* SwapchainTexture,
                  Uint32 SwapchainWidth, Uint32 SwapchainHeight);

    // Draw Texture scaled into Dst (reference pixels). Src (texture pixels) selects
    // a sub-rectangle; pass nullptr for the whole texture. Flip mirrors the source
    // sampling; Color modulates the sampled texels (default opaque white).
    void DrawTexture(const GpuTexture& Texture, const SDL_FRect& Dst,
                     const SDL_FRect* Src = nullptr,
                     SDL_FlipMode Flip = SDL_FLIP_NONE,
                     SDL_FColor Color = SDL_FColor{ 1.0f, 1.0f, 1.0f, 1.0f });

    // Fill Dst (reference pixels) with a flat color.
    void DrawColoredQuad(const SDL_FRect& Dst, SDL_FColor Color);

    // Column-major 4×4 orthographic projection (reference space → clip space).
    [[nodiscard]] const float*         Projection() const { return m_projection; }
    [[nodiscard]] const SDL_GPUViewport& Viewport()  const { return m_viewport;   }

private:
    Renderer2D() = default;
    void recomputeProjection();

    // Records one quad (6 vertices, triangle list) using the given pipeline and
    // optional texture. Texture may be null for the flat-color pipeline.
    void drawQuad(SDL_GPUGraphicsPipeline* Pipeline, SDL_GPUTexture* Texture,
                  const SDL_FRect& Dst, const SDL_FRect* Src, SDL_FlipMode Flip,
                  SDL_FColor Color, int TexW, int TexH);

    SDL_GPUDevice* m_device = nullptr;  // non-owning

    SDL_GPUGraphicsPipeline* m_texturedPipeline = nullptr;
    SDL_GPUGraphicsPipeline* m_colorPipeline     = nullptr;
    SDL_GPUSampler*          m_sampler           = nullptr;
    SDL_GPUBuffer*           m_vertexBuffer      = nullptr;
    SDL_GPUTransferBuffer*   m_transfer          = nullptr;

    // Per-frame bound state (set by SetFrame; valid until EndFrame).
    SDL_GPUCommandBuffer* m_cmdBuf       = nullptr;
    SDL_GPUTexture*       m_swapchainTex = nullptr;
    Uint32                m_swapchainW   = 0;
    Uint32                m_swapchainH   = 0;

    int   m_refWidth  = 0;
    int   m_refHeight = 0;
    float m_projection[16] = {};
    SDL_GPUViewport m_viewport{};
};
