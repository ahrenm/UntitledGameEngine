#include <Render/Renderer2D.h>
#include <Render/GpuTexture.h>
#include <Layers/PhysFSLayer.h>
#include <ServiceLocator.h>
#include <format>
#include <utility>
#include <vector>

namespace
{
    // Interleaved 2D vertex: position (px), texcoord, color — matches the
    // SDL_GPUVertexAttribute layout below and the sprite.vert.hlsl input.
    struct Vertex2D
    {
        float X, Y;
        float U, V;
        float R, G, B, A;
    };

    constexpr Uint32 QUAD_VERTEX_COUNT = 6;                         // two triangles
    constexpr Uint32 QUAD_BYTES        = QUAD_VERTEX_COUNT * sizeof(Vertex2D);

    // Load a precompiled SPIR-V shader from the VFS and create an SDL_GPUShader.
    std::expected<SDL_GPUShader*, std::string>
    LoadShader(SDL_GPUDevice* Device, const char* VirtualPath, SDL_GPUShaderStage Stage,
               Uint32 NumSamplers, Uint32 NumUniformBuffers)
    {
        auto& Physfs = ServiceLocator::Get<PhysFSLayer>();
        std::vector<std::byte> Code = Physfs.ReadFile(VirtualPath);
        if (Code.empty())
            return std::unexpected(std::format("Renderer2D: could not read shader '{}'", VirtualPath));

        SDL_GPUShaderCreateInfo Info{};
        Info.code                 = reinterpret_cast<const Uint8*>(Code.data());
        Info.code_size            = Code.size();
        Info.entrypoint           = "main";
        Info.format               = SDL_GPU_SHADERFORMAT_SPIRV;
        Info.stage                = Stage;
        Info.num_samplers         = NumSamplers;
        Info.num_uniform_buffers  = NumUniformBuffers;

        SDL_GPUShader* Shader = SDL_CreateGPUShader(Device, &Info);
        if (!Shader)
            return std::unexpected(std::format("SDL_CreateGPUShader('{}') failed: {}",
                                               VirtualPath, SDL_GetError()));
        return Shader;
    }

    // Build a textured/colored quad pipeline sharing the sprite vertex shader.
    SDL_GPUGraphicsPipeline*
    MakePipeline(SDL_GPUDevice* Device, SDL_GPUShader* Vertex, SDL_GPUShader* Fragment,
                 SDL_GPUTextureFormat ColorFormat)
    {
        SDL_GPUVertexBufferDescription VbDesc{};
        VbDesc.slot       = 0;
        VbDesc.pitch      = sizeof(Vertex2D);
        VbDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute Attrs[3]{};
        Attrs[0].location = 0; Attrs[0].buffer_slot = 0;
        Attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        Attrs[0].offset   = offsetof(Vertex2D, X);
        Attrs[1].location = 1; Attrs[1].buffer_slot = 0;
        Attrs[1].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        Attrs[1].offset   = offsetof(Vertex2D, U);
        Attrs[2].location = 2; Attrs[2].buffer_slot = 0;
        Attrs[2].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        Attrs[2].offset   = offsetof(Vertex2D, R);

        SDL_GPUColorTargetBlendState Blend{};
        Blend.enable_blend           = true;
        Blend.src_color_blendfactor  = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        Blend.dst_color_blendfactor  = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        Blend.color_blend_op         = SDL_GPU_BLENDOP_ADD;
        Blend.src_alpha_blendfactor  = SDL_GPU_BLENDFACTOR_ONE;
        Blend.dst_alpha_blendfactor  = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        Blend.alpha_blend_op         = SDL_GPU_BLENDOP_ADD;

        SDL_GPUColorTargetDescription ColorTarget{};
        ColorTarget.format      = ColorFormat;
        ColorTarget.blend_state = Blend;

        SDL_GPUGraphicsPipelineCreateInfo Info{};
        Info.vertex_shader   = Vertex;
        Info.fragment_shader = Fragment;
        Info.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        Info.vertex_input_state.vertex_buffer_descriptions = &VbDesc;
        Info.vertex_input_state.num_vertex_buffers         = 1;
        Info.vertex_input_state.vertex_attributes          = Attrs;
        Info.vertex_input_state.num_vertex_attributes      = 3;
        Info.target_info.color_target_descriptions = &ColorTarget;
        Info.target_info.num_color_targets         = 1;

        return SDL_CreateGPUGraphicsPipeline(Device, &Info);
    }
}

// ── Renderer2D::Create ────────────────────────────────────────────────────────
std::expected<std::unique_ptr<Renderer2D>, std::string>
Renderer2D::Create(SDL_GPUDevice* Device, SDL_Window* Window)
{
    if (!Device) return std::unexpected(std::string("Renderer2D::Create: null device"));
    if (!Window) return std::unexpected(std::string("Renderer2D::Create: null window"));

    const SDL_GPUTextureFormat ColorFormat = SDL_GetGPUSwapchainTextureFormat(Device, Window);

    auto Vert = LoadShader(Device, "assets/shaders/sprite.vert.spv",
                           SDL_GPU_SHADERSTAGE_VERTEX, /*samplers=*/0, /*uniforms=*/1);
    if (!Vert) return std::unexpected(Vert.error());

    auto TexFrag = LoadShader(Device, "assets/shaders/sprite.frag.spv",
                              SDL_GPU_SHADERSTAGE_FRAGMENT, /*samplers=*/1, /*uniforms=*/0);
    if (!TexFrag) { SDL_ReleaseGPUShader(Device, *Vert); return std::unexpected(TexFrag.error()); }

    auto ColFrag = LoadShader(Device, "assets/shaders/color.frag.spv",
                              SDL_GPU_SHADERSTAGE_FRAGMENT, /*samplers=*/0, /*uniforms=*/0);
    if (!ColFrag)
    {
        SDL_ReleaseGPUShader(Device, *Vert);
        SDL_ReleaseGPUShader(Device, *TexFrag);
        return std::unexpected(ColFrag.error());
    }

    auto R = std::unique_ptr<Renderer2D>(new Renderer2D());
    R->m_device = Device;

    R->m_texturedPipeline = MakePipeline(Device, *Vert, *TexFrag, ColorFormat);
    R->m_colorPipeline    = MakePipeline(Device, *Vert, *ColFrag, ColorFormat);

    // Shaders can be released once the pipelines are built.
    SDL_ReleaseGPUShader(Device, *Vert);
    SDL_ReleaseGPUShader(Device, *TexFrag);
    SDL_ReleaseGPUShader(Device, *ColFrag);

    if (!R->m_texturedPipeline || !R->m_colorPipeline)
        return std::unexpected(std::format("SDL_CreateGPUGraphicsPipeline failed: {}", SDL_GetError()));

    SDL_GPUSamplerCreateInfo SamplerInfo{};
    SamplerInfo.min_filter     = SDL_GPU_FILTER_LINEAR;
    SamplerInfo.mag_filter     = SDL_GPU_FILTER_LINEAR;
    SamplerInfo.mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    SamplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    SamplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    SamplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    R->m_sampler = SDL_CreateGPUSampler(Device, &SamplerInfo);
    if (!R->m_sampler)
        return std::unexpected(std::format("SDL_CreateGPUSampler failed: {}", SDL_GetError()));

    SDL_GPUBufferCreateInfo VbInfo{};
    VbInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    VbInfo.size  = QUAD_BYTES;
    R->m_vertexBuffer = SDL_CreateGPUBuffer(Device, &VbInfo);
    if (!R->m_vertexBuffer)
        return std::unexpected(std::format("SDL_CreateGPUBuffer failed: {}", SDL_GetError()));

    SDL_GPUTransferBufferCreateInfo TbInfo{};
    TbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    TbInfo.size  = QUAD_BYTES;
    R->m_transfer = SDL_CreateGPUTransferBuffer(Device, &TbInfo);
    if (!R->m_transfer)
        return std::unexpected(std::format("SDL_CreateGPUTransferBuffer failed: {}", SDL_GetError()));

    return R;
}

// ── Renderer2D lifecycle ──────────────────────────────────────────────────────
Renderer2D::~Renderer2D()
{
    if (!m_device) return;
    if (m_transfer)          SDL_ReleaseGPUTransferBuffer(m_device, m_transfer);
    if (m_vertexBuffer)      SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
    if (m_sampler)           SDL_ReleaseGPUSampler(m_device, m_sampler);
    if (m_texturedPipeline)  SDL_ReleaseGPUGraphicsPipeline(m_device, m_texturedPipeline);
    if (m_colorPipeline)     SDL_ReleaseGPUGraphicsPipeline(m_device, m_colorPipeline);
}

// ── Frame / projection ────────────────────────────────────────────────────────
void Renderer2D::SetReferenceSize(int RefWidth, int RefHeight)
{
    m_refWidth  = RefWidth;
    m_refHeight = RefHeight;
    recomputeProjection();
}

void Renderer2D::SetFrame(SDL_GPUCommandBuffer* CommandBuffer, SDL_GPUTexture* SwapchainTexture,
                          Uint32 SwapchainWidth, Uint32 SwapchainHeight)
{
    m_cmdBuf       = CommandBuffer;
    m_swapchainTex = SwapchainTexture;
    m_swapchainW   = SwapchainWidth;
    m_swapchainH   = SwapchainHeight;
    recomputeProjection();
}

void Renderer2D::recomputeProjection()
{
    // Letterbox: uniformly scale the reference area to fit the swapchain, centred.
    const float RefW = static_cast<float>(m_refWidth);
    const float RefH = static_cast<float>(m_refHeight);
    if (RefW <= 0.0f || RefH <= 0.0f || m_swapchainW == 0 || m_swapchainH == 0)
        return;

    const float SwW = static_cast<float>(m_swapchainW);
    const float SwH = static_cast<float>(m_swapchainH);
    const float Scale = SDL_min(SwW / RefW, SwH / RefH);
    const float VpW = RefW * Scale;
    const float VpH = RefH * Scale;

    m_viewport.x         = (SwW - VpW) * 0.5f;
    m_viewport.y         = (SwH - VpH) * 0.5f;
    m_viewport.w         = VpW;
    m_viewport.h         = VpH;
    m_viewport.min_depth = 0.0f;
    m_viewport.max_depth = 1.0f;

    // Column-major orthographic projection mapping reference space
    // (x:[0,RefW] → [-1,1], y:[0,RefH] top-down → [1,-1]) to clip space.
    for (float& F : m_projection) F = 0.0f;
    m_projection[0]  =  2.0f / RefW;   // col0 row0
    m_projection[5]  = -2.0f / RefH;   // col1 row1
    m_projection[10] =  1.0f;          // col2 row2
    m_projection[12] = -1.0f;          // col3 row0 (x translate)
    m_projection[13] =  1.0f;          // col3 row1 (y translate)
    m_projection[15] =  1.0f;          // col3 row3
}

// ── Draw ──────────────────────────────────────────────────────────────────────
void Renderer2D::DrawTexture(const GpuTexture& Texture, const SDL_FRect& Dst,
                             const SDL_FRect* Src, SDL_FlipMode Flip, SDL_FColor Color)
{
    if (!Texture.IsValid()) return;
    drawQuad(m_texturedPipeline, Texture.Handle(), Dst, Src, Flip, Color,
             Texture.Width(), Texture.Height());
}

void Renderer2D::DrawColoredQuad(const SDL_FRect& Dst, SDL_FColor Color)
{
    drawQuad(m_colorPipeline, nullptr, Dst, nullptr, SDL_FLIP_NONE, Color, 0, 0);
}

void Renderer2D::drawQuad(SDL_GPUGraphicsPipeline* Pipeline, SDL_GPUTexture* Texture,
                          const SDL_FRect& Dst, const SDL_FRect* Src, SDL_FlipMode Flip,
                          SDL_FColor Color, int TexW, int TexH)
{
    if (!m_cmdBuf || !m_swapchainTex || !Pipeline) return;

    // Texture coordinates (normalised). Defaults to the whole texture.
    float U0 = 0.0f, V0 = 0.0f, U1 = 1.0f, V1 = 1.0f;
    if (Src && TexW > 0 && TexH > 0)
    {
        U0 = Src->x / TexW;
        V0 = Src->y / TexH;
        U1 = (Src->x + Src->w) / TexW;
        V1 = (Src->y + Src->h) / TexH;
    }
    if (Flip & SDL_FLIP_HORIZONTAL) std::swap(U0, U1);
    if (Flip & SDL_FLIP_VERTICAL)   std::swap(V0, V1);

    const float X0 = Dst.x,          Y0 = Dst.y;
    const float X1 = Dst.x + Dst.w,  Y1 = Dst.y + Dst.h;
    const float R = Color.r, G = Color.g, B = Color.b, A = Color.a;

    const Vertex2D Verts[QUAD_VERTEX_COUNT] = {
        { X0, Y0, U0, V0, R, G, B, A }, // TL
        { X0, Y1, U0, V1, R, G, B, A }, // BL
        { X1, Y0, U1, V0, R, G, B, A }, // TR
        { X1, Y0, U1, V0, R, G, B, A }, // TR
        { X0, Y1, U0, V1, R, G, B, A }, // BL
        { X1, Y1, U1, V1, R, G, B, A }, // BR
    };

    // Stage the vertices (cycle so reuse within a frame doesn't hazard).
    auto* Mapped = static_cast<Vertex2D*>(SDL_MapGPUTransferBuffer(m_device, m_transfer, /*cycle=*/true));
    if (!Mapped) return;
    SDL_memcpy(Mapped, Verts, QUAD_BYTES);
    SDL_UnmapGPUTransferBuffer(m_device, m_transfer);

    SDL_GPUCopyPass* Copy = SDL_BeginGPUCopyPass(m_cmdBuf);
    SDL_GPUTransferBufferLocation Source{};
    Source.transfer_buffer = m_transfer;
    Source.offset          = 0;
    SDL_GPUBufferRegion DstRegion{};
    DstRegion.buffer = m_vertexBuffer;
    DstRegion.offset = 0;
    DstRegion.size   = QUAD_BYTES;
    SDL_UploadToGPUBuffer(Copy, &Source, &DstRegion, /*cycle=*/true);
    SDL_EndGPUCopyPass(Copy);

    // Composite over prior passes (LOAD, no clear).
    SDL_GPUColorTargetInfo ColorInfo{};
    ColorInfo.texture  = m_swapchainTex;
    ColorInfo.load_op  = SDL_GPU_LOADOP_LOAD;
    ColorInfo.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* Pass = SDL_BeginGPURenderPass(m_cmdBuf, &ColorInfo, 1, nullptr);
    SDL_SetGPUViewport(Pass, &m_viewport);
    SDL_BindGPUGraphicsPipeline(Pass, Pipeline);
    SDL_PushGPUVertexUniformData(m_cmdBuf, 0, m_projection, sizeof(m_projection));

    SDL_GPUBufferBinding VbBinding{};
    VbBinding.buffer = m_vertexBuffer;
    VbBinding.offset = 0;
    SDL_BindGPUVertexBuffers(Pass, 0, &VbBinding, 1);

    if (Texture)
    {
        SDL_GPUTextureSamplerBinding TexBinding{};
        TexBinding.texture = Texture;
        TexBinding.sampler = m_sampler;
        SDL_BindGPUFragmentSamplers(Pass, 0, &TexBinding, 1);
    }

    SDL_DrawGPUPrimitives(Pass, QUAD_VERTEX_COUNT, 1, 0, 0);
    SDL_EndGPURenderPass(Pass);
}
