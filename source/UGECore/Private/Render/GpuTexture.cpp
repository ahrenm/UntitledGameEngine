#include <Render/GpuTexture.h>
#include <format>
#include <utility>

// ── GpuTexture lifecycle ──────────────────────────────────────────────────────
GpuTexture::~GpuTexture()
{
    release();
}

GpuTexture::GpuTexture(GpuTexture&& Other) noexcept
    : m_device(std::exchange(Other.m_device, nullptr))
    , m_texture(std::exchange(Other.m_texture, nullptr))
    , m_width(std::exchange(Other.m_width, 0))
    , m_height(std::exchange(Other.m_height, 0))
{}

GpuTexture& GpuTexture::operator=(GpuTexture&& Other) noexcept
{
    if (this != &Other)
    {
        release();
        m_device  = std::exchange(Other.m_device, nullptr);
        m_texture = std::exchange(Other.m_texture, nullptr);
        m_width   = std::exchange(Other.m_width, 0);
        m_height  = std::exchange(Other.m_height, 0);
    }
    return *this;
}

void GpuTexture::release()
{
    if (m_device && m_texture)
        SDL_ReleaseGPUTexture(m_device, m_texture);
    m_device  = nullptr;
    m_texture = nullptr;
    m_width   = 0;
    m_height  = 0;
}

// ── GpuTexture::FromSurface ───────────────────────────────────────────────────
std::expected<GpuTexture, std::string>
GpuTexture::FromSurface(SDL_GPUDevice* Device, SDL_Surface* Surface)
{
    if (!Device)  return std::unexpected(std::string("GpuTexture::FromSurface: null device"));
    if (!Surface) return std::unexpected(std::string("GpuTexture::FromSurface: null surface"));

    // Normalise to a tightly-packed RGBA32 surface so the upload matches the
    // R8G8B8A8_UNORM texture format regardless of the source pixel layout.
    SDL_Surface* Rgba = Surface;
    const bool   OwnsRgba = (Surface->format != SDL_PIXELFORMAT_RGBA32);
    if (OwnsRgba)
    {
        Rgba = SDL_ConvertSurface(Surface, SDL_PIXELFORMAT_RGBA32);
        if (!Rgba)
            return std::unexpected(std::format("SDL_ConvertSurface failed: {}", SDL_GetError()));
    }

    const Uint32 W = static_cast<Uint32>(Rgba->w);
    const Uint32 H = static_cast<Uint32>(Rgba->h);

    // Create the destination GPU texture.
    SDL_GPUTextureCreateInfo TexInfo{};
    TexInfo.type                 = SDL_GPU_TEXTURETYPE_2D;
    TexInfo.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    TexInfo.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    TexInfo.width                = W;
    TexInfo.height               = H;
    TexInfo.layer_count_or_depth = 1;
    TexInfo.num_levels           = 1;

    SDL_GPUTexture* Texture = SDL_CreateGPUTexture(Device, &TexInfo);
    if (!Texture)
    {
        if (OwnsRgba) SDL_DestroySurface(Rgba);
        return std::unexpected(std::format("SDL_CreateGPUTexture failed: {}", SDL_GetError()));
    }

    // Stage the pixels in a transfer buffer.
    const Uint32 ByteCount = W * H * 4u;

    SDL_GPUTransferBufferCreateInfo TransferInfo{};
    TransferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    TransferInfo.size  = ByteCount;

    SDL_GPUTransferBuffer* Transfer = SDL_CreateGPUTransferBuffer(Device, &TransferInfo);
    if (!Transfer)
    {
        SDL_ReleaseGPUTexture(Device, Texture);
        if (OwnsRgba) SDL_DestroySurface(Rgba);
        return std::unexpected(std::format("SDL_CreateGPUTransferBuffer failed: {}", SDL_GetError()));
    }

    auto* Mapped = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(Device, Transfer, /*cycle=*/false));
    if (!Mapped)
    {
        SDL_ReleaseGPUTransferBuffer(Device, Transfer);
        SDL_ReleaseGPUTexture(Device, Texture);
        if (OwnsRgba) SDL_DestroySurface(Rgba);
        return std::unexpected(std::format("SDL_MapGPUTransferBuffer failed: {}", SDL_GetError()));
    }

    // Copy row by row to strip any source pitch padding (transfer expects a
    // tightly-packed W*4 stride).
    const auto* Src = static_cast<const Uint8*>(Rgba->pixels);
    const int   Pitch = Rgba->pitch;
    for (Uint32 Row = 0; Row < H; ++Row)
        SDL_memcpy(Mapped + Row * W * 4u, Src + Row * Pitch, W * 4u);

    SDL_UnmapGPUTransferBuffer(Device, Transfer);

    // Record and submit the copy pass.
    SDL_GPUCommandBuffer* Cmd = SDL_AcquireGPUCommandBuffer(Device);
    if (!Cmd)
    {
        SDL_ReleaseGPUTransferBuffer(Device, Transfer);
        SDL_ReleaseGPUTexture(Device, Texture);
        if (OwnsRgba) SDL_DestroySurface(Rgba);
        return std::unexpected(std::format("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError()));
    }

    SDL_GPUCopyPass* Copy = SDL_BeginGPUCopyPass(Cmd);

    SDL_GPUTextureTransferInfo Source{};
    Source.transfer_buffer = Transfer;
    Source.offset          = 0;
    Source.pixels_per_row   = W;
    Source.rows_per_layer   = H;

    SDL_GPUTextureRegion Dest{};
    Dest.texture   = Texture;
    Dest.w         = W;
    Dest.h         = H;
    Dest.d         = 1;

    SDL_UploadToGPUTexture(Copy, &Source, &Dest, /*cycle=*/false);
    SDL_EndGPUCopyPass(Copy);
    SDL_SubmitGPUCommandBuffer(Cmd);

    SDL_ReleaseGPUTransferBuffer(Device, Transfer);
    if (OwnsRgba) SDL_DestroySurface(Rgba);

    GpuTexture Result;
    Result.m_device  = Device;
    Result.m_texture = Texture;
    Result.m_width   = static_cast<int>(W);
    Result.m_height  = static_cast<int>(H);
    return Result;
}
