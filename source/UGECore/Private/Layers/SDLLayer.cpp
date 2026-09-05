#include <Layers/SDLLayer.h>
#include <Layers/LoggingLayer.h>
#include <Layers/PhysFSLayer.h>
#include <Render/Renderer2D.h>
#include <ServiceLocator.h>
#include <UGEApplication.h>
#include <SDL3_image/SDL_image.h>
#include <format>
#include <utility>

// ── SDLLayer::Create ──────────────────────────────────────────────────────────
std::expected<std::unique_ptr<AppLayer>, std::string> SDLLayer::Create()
{
    const auto& S = UGEApplication::Settings;

    auto App = std::unique_ptr<SDLLayer>(new SDLLayer());
    if (!SDL_Init(SDL_INIT_VIDEO))
        return std::unexpected(std::format("SDL_Init failed: {}", SDL_GetError()));
    App->m_sdlInit = true;

    // No renderer flags are required for SDL_GPU; the device is attached to the
    // window via SDL_ClaimWindowForGPUDevice below.
    App->m_window.reset(SDL_CreateWindow(S.WindowTitle.c_str(), S.WindowWidth, S.WindowHeight, 0));
    if (!App->m_window)
        return std::unexpected(std::format("SDL_CreateWindow failed: {}", SDL_GetError()));

    // ── GPU device ─────────────────────────────────────────────────────────────
    // All engine shaders are shipped as SPIR-V only (see assets/shaders/), so the
    // device must run a backend that consumes SPIR-V. Requesting SPIRV alone makes
    // SDL pick a compatible backend (Vulkan on Windows/Linux). Note: the default
    // Windows backend is D3D12, which only accepts DXBC/DXIL — requesting DXIL/MSL
    // here would select it and then fail at SDL_CreateGPUShader. Debug mode is left
    // off to avoid requiring validation layers on end-user machines.
    App->m_device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV,
        /*debug_mode=*/false, /*name=*/nullptr);
    if (!App->m_device)
        return std::unexpected(std::format("SDL_CreateGPUDevice failed: {}", SDL_GetError()));

    if (!SDL_ClaimWindowForGPUDevice(App->m_device, App->m_window.get()))
        return std::unexpected(std::format("SDL_ClaimWindowForGPUDevice failed: {}", SDL_GetError()));

    // ── Reference resolution ───────────────────────────────────────────────────
    // Retained for P2's letterbox projection (Renderer2D). SDL_GPU has no direct
    // equivalent of SDL_SetRenderLogicalPresentation; scaling is handled in the
    // projection matrix instead.
    const int RefW = (S.RefWidth  > 0) ? S.RefWidth  : S.WindowWidth;
    const int RefH = (S.RefHeight > 0) ? S.RefHeight : S.WindowHeight;
    App->m_refWidth  = RefW;
    App->m_refHeight = RefH;

    // ── Core 2D renderer ───────────────────────────────────────────────────────
    // Owns the textured/colored quad pipelines and the letterbox projection. Must
    // be created after the device is claimed to the window (queries swapchain fmt).
    auto Renderer = Renderer2D::Create(App->m_device, App->m_window.get());
    if (!Renderer)
        return std::unexpected(std::format("Renderer2D::Create failed: {}", Renderer.error()));
    App->m_renderer2D = std::move(*Renderer);
    App->m_renderer2D->SetReferenceSize(RefW, RefH);

    if (auto* Log = ServiceLocator::TryGet<LoggingLayer>())
    {
        App->SetLogFunction(Log->MakeSink());
        App->Log(std::format("[SDL] GPU device driver: {}",
                             SDL_GetGPUDeviceDriver(App->m_device)));
    }

    return std::unique_ptr<AppLayer>(App.release());
}

// ── SDLLayer lifecycle ────────────────────────────────────────────────────────
SDLLayer::~SDLLayer()
{
    if (m_logFn) SDL_SetLogOutputFunction(nullptr, nullptr);
    // Release GPU-backed members before the device that owns them.
    m_bgTexture = GpuTexture{};
    m_renderer2D.reset();
    if (m_device)
    {
        // Detach the swapchain from the window before destroying the device,
        // then destroy the device before the window it was claimed for.
        if (m_window) SDL_ReleaseWindowFromGPUDevice(m_device, m_window.get());
        SDL_DestroyGPUDevice(m_device);
        m_device = nullptr;
    }
    m_window.reset();
    if (m_sdlInit) SDL_Quit();
}

SDLLayer::SDLLayer(SDLLayer&& other) noexcept
    : m_window(std::move(other.m_window))
    , m_device(std::exchange(other.m_device, nullptr))
    , m_renderer2D(std::move(other.m_renderer2D))
    , m_bgTexture(std::move(other.m_bgTexture))
    , m_sdlInit(std::exchange(other.m_sdlInit, false))
    , m_running(other.m_running)
    , m_refWidth(other.m_refWidth)
    , m_refHeight(other.m_refHeight)
    , m_logFn(std::move(other.m_logFn))
{}

SDLLayer& SDLLayer::operator=(SDLLayer&& other) noexcept
{
    if (this != &other) {
        if (m_logFn) SDL_SetLogOutputFunction(nullptr, nullptr);
        m_bgTexture = GpuTexture{};
        m_renderer2D.reset();
        if (m_device)
        {
            if (m_window) SDL_ReleaseWindowFromGPUDevice(m_device, m_window.get());
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
        }
        m_window.reset();
        if (m_sdlInit) SDL_Quit();
        m_window      = std::move(other.m_window);
        m_device      = std::exchange(other.m_device, nullptr);
        m_renderer2D  = std::move(other.m_renderer2D);
        m_bgTexture   = std::move(other.m_bgTexture);
        m_sdlInit     = std::exchange(other.m_sdlInit, false);
        m_running     = other.m_running;
        m_refWidth    = other.m_refWidth;
        m_refHeight   = other.m_refHeight;
        m_logFn       = std::move(other.m_logFn);
    }
    return *this;
}

// ── SDLLayer methods ──────────────────────────────────────────────────────────
void SDLLayer::ResizeToTexture(int Width, int Height) const
{
    SDL_SetWindowSize(m_window.get(), Width, Height);
}

std::expected<void, std::string> SDLLayer::SetBackground(const char* VirtualPath)
{
    auto Result = LoadTextureFromPhysFS(VirtualPath);
    if (!Result) return std::unexpected(Result.error());

    m_bgTexture = std::move(*Result);
    ResizeToTexture(m_bgTexture.Width(), m_bgTexture.Height());
    return {};
}

void SDLLayer::SetLogFunction(std::function<void(std::string)> Fn)
{
    m_logFn = std::make_shared<std::function<void(std::string)>>(std::move(Fn));

    SDL_SetLogOutputFunction([](void* Ud, int, SDL_LogPriority Prio, const char* Msg)
    {
        auto* FnPtr = static_cast<std::function<void(std::string)>*>(Ud);
        if (!FnPtr || !*FnPtr) { fprintf(stderr, "%s\n", Msg); return; }

        const char* Tag = (Prio >= SDL_LOG_PRIORITY_ERROR) ? "[SDL ERROR] " :
                          (Prio >= SDL_LOG_PRIORITY_WARN)  ? "[SDL WARN]  " :
                                                             "[SDL]       ";
        (*FnPtr)(std::string(Tag) + Msg);
    }, m_logFn.get());
}

void SDLLayer::PollEvents(const std::function<void(SDL_Event&)>& Handler)
{
    SDL_Event Event;
    while (SDL_PollEvent(&Event)) {
        if (Event.type == SDL_EVENT_QUIT)
            m_running = false;
        if (Handler) Handler(Event);
    }
}

void SDLLayer::BeginFrame()
{
    m_frameActive  = false;
    m_swapchainTex = nullptr;
    m_swapchainW   = 0;
    m_swapchainH   = 0;

    m_cmdBuf = SDL_AcquireGPUCommandBuffer(m_device);
    if (!m_cmdBuf)
    {
        Log(std::format("[SDL] SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError()));
        return;
    }

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(m_cmdBuf, m_window.get(),
                                               &m_swapchainTex, &m_swapchainW, &m_swapchainH))
    {
        Log(std::format("[SDL] SDL_WaitAndAcquireGPUSwapchainTexture failed: {}", SDL_GetError()));
        SDL_CancelGPUCommandBuffer(m_cmdBuf);
        m_cmdBuf = nullptr;
        return;
    }

    // A null swapchain texture is not an error — it happens while minimized.
    if (!m_swapchainTex || m_swapchainW == 0 || m_swapchainH == 0)
    {
        SDL_CancelGPUCommandBuffer(m_cmdBuf);
        m_cmdBuf       = nullptr;
        m_swapchainTex = nullptr;
        return;
    }

    // Clear the swapchain to black. Subsequent passes (Renderer2D, RmlUi, plugins)
    // load these contents and composite on top.
    SDL_GPUColorTargetInfo ColorInfo{};
    ColorInfo.texture     = m_swapchainTex;
    ColorInfo.load_op     = SDL_GPU_LOADOP_CLEAR;
    ColorInfo.store_op    = SDL_GPU_STOREOP_STORE;
    ColorInfo.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };

    SDL_GPURenderPass* Pass = SDL_BeginGPURenderPass(m_cmdBuf, &ColorInfo, 1, nullptr);
    SDL_EndGPURenderPass(Pass);

    // Bind the in-flight frame to the 2D renderer so this frame's Draw() passes
    // (background here, sprites/scenes in later layers) record into it.
    if (m_renderer2D)
        m_renderer2D->SetFrame(m_cmdBuf, m_swapchainTex, m_swapchainW, m_swapchainH);

    m_frameActive = true;
}

void SDLLayer::EndFrame()
{
    if (!m_cmdBuf) return; // minimized or acquisition failed — nothing to submit
    SDL_SubmitGPUCommandBuffer(m_cmdBuf);
    m_cmdBuf       = nullptr;
    m_swapchainTex = nullptr;
    m_frameActive  = false;
}

void SDLLayer::SetEventHandler(std::function<void(SDL_Event&)> Handler)
{
    m_eventHandler = std::move(Handler);
}

// ── AppLayer ──────────────────────────────────────────────────────────────────
void SDLLayer::Update()
{
    PollEvents(m_eventHandler);
}

void SDLLayer::Draw(float /*deltaTime*/)
{
    // Blit the background texture (if any) filling the reference area. Renderer2D
    // records into the shared command buffer and letterboxes to the swapchain.
    // Scene geometry is drawn by SceneManagerLayer::Draw() (load order 4.05),
    // which runs immediately after this layer's Draw().
    if (!m_frameActive || !m_renderer2D || !m_bgTexture.IsValid())
        return;

    const SDL_FRect Dst{ 0.0f, 0.0f,
                         static_cast<float>(m_refWidth),
                         static_cast<float>(m_refHeight) };
    m_renderer2D->DrawTexture(m_bgTexture, Dst);
}

// ── IScriptableObject ─────────────────────────────────────────────────────────
void SDLLayer::RegisterObject(sol::state& Lua)
{
    auto Sdl = Lua.create_named_table("SDL");

    Sdl.set_function("SetBackground", [this](const std::string& Path) {
        if (auto Result = SetBackground(Path.c_str()); !Result)
            Log("[SDL] SetBackground error: " + Result.error());
    });


    Sdl.set_function("SetCamera", [this](float WorldX, float WorldY) {
        m_camera.WorldX = WorldX;
        m_camera.WorldY = WorldY;
    });

    Sdl.set_function("MoveCamera", [this](float Dx, float Dy) {
        MoveCamera(Dx, Dy);
    });
}

void SDLLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}


// ── Free helpers ──────────────────────────────────────────────────────────────
std::expected<GpuTexture, std::string>
LoadTextureFromPhysFS(const char* Path)
{
    auto* Device = ServiceLocator::Get<SDLLayer>().Device();
    if (!Device)
        return std::unexpected(std::string("LoadTextureFromPhysFS: no GPU device"));

    auto& Physfs = ServiceLocator::Get<PhysFSLayer>();
    SDL_IOStream* Io = Physfs.OpenAsIOStream(Path);
    if (!Io)
        return std::unexpected(std::format("PhysFS could not open: {}", Path));

    // IMG_Load_IO decodes to an SDL_Surface (CPU pixels); closeio frees the stream.
    SDL_Surface* Surface = IMG_Load_IO(Io, /*closeio=*/true);
    if (!Surface)
        return std::unexpected(std::format("IMG_Load_IO failed for '{}': {}", Path, SDL_GetError()));

    auto Texture = GpuTexture::FromSurface(Device, Surface);
    SDL_DestroySurface(Surface);
    if (!Texture)
        return std::unexpected(Texture.error());

    return std::move(*Texture);
}
