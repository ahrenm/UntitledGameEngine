#include <Layers/SDLLayer.h>
#include <Layers/LoggingLayer.h>
#include <Layers/PhysFSLayer.h>
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

    App->m_window.reset(SDL_CreateWindow(S.WindowTitle.c_str(), S.WindowWidth, S.WindowHeight, 0));
    if (!App->m_window)
        return std::unexpected(std::format("SDL_CreateWindow failed: {}", SDL_GetError()));

    App->m_renderer.reset(SDL_CreateRenderer(App->m_window.get(), nullptr));
    if (!App->m_renderer)
        return std::unexpected(std::format("SDL_CreateRenderer failed: {}", SDL_GetError()));

    // ── Logical presentation ───────────────────────────────────────────────────
    const int RefW = (S.RefWidth  > 0) ? S.RefWidth  : S.WindowWidth;
    const int RefH = (S.RefHeight > 0) ? S.RefHeight : S.WindowHeight;
    SDL_SetRenderLogicalPresentation(App->m_renderer.get(), RefW, RefH,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    App->m_refWidth  = RefW;
    App->m_refHeight = RefH;

    SDL_SetRenderDrawBlendMode(App->m_renderer.get(), SDL_BLENDMODE_BLEND);

    if (auto* Log = ServiceLocator::TryGet<LoggingLayer>())
        App->SetLogFunction(Log->MakeSink());

    return std::unique_ptr<AppLayer>(App.release());
}

// ── SDLLayer lifecycle ────────────────────────────────────────────────────────
SDLLayer::~SDLLayer()
{
    if (m_logFn) SDL_SetLogOutputFunction(nullptr, nullptr);
    m_renderer.reset();
    m_window.reset();
    if (m_sdlInit) SDL_Quit();
}

SDLLayer::SDLLayer(SDLLayer&& other) noexcept
    : m_window(std::move(other.m_window))
    , m_renderer(std::move(other.m_renderer))
    , m_sdlInit(std::exchange(other.m_sdlInit, false))
    , m_running(other.m_running)
    , m_logFn(std::move(other.m_logFn))
{}

SDLLayer& SDLLayer::operator=(SDLLayer&& other) noexcept
{
    if (this != &other) {
        if (m_logFn) SDL_SetLogOutputFunction(nullptr, nullptr);
        m_renderer.reset();
        m_window.reset();
        if (m_sdlInit) SDL_Quit();
        m_window      = std::move(other.m_window);
        m_renderer    = std::move(other.m_renderer);
        m_sdlInit     = std::exchange(other.m_sdlInit, false);
        m_running     = other.m_running;
        m_logFn       = std::move(other.m_logFn);
    }
    return *this;
}

// ── SDLLayer methods ──────────────────────────────────────────────────────────
void SDLLayer::ResizeToTexture(SDL_Texture* Tex) const
{
    float W = 0, H = 0;
    SDL_GetTextureSize(Tex, &W, &H);
    SDL_SetWindowSize(m_window.get(), static_cast<int>(W), static_cast<int>(H));
}

std::expected<void, std::string> SDLLayer::SetBackground(const char* VirtualPath)
{
    auto Result = LoadTextureFromPhysFS(VirtualPath);
    if (!Result) return std::unexpected(Result.error());

    m_bgTexture = std::move(*Result);
    ResizeToTexture(m_bgTexture.get());
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

void SDLLayer::BeginFrame() const
{
    SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 0, 255);
    SDL_RenderClear(m_renderer.get());
}

void SDLLayer::EndFrame() const
{
    SDL_RenderPresent(m_renderer.get());
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
    if (m_bgTexture)
        SDL_RenderTexture(m_renderer.get(), m_bgTexture.get(), nullptr, nullptr);
    // Scene geometry is drawn by SceneManagerLayer::Draw() (load order 4.05),
    // which runs immediately after this layer's Draw().
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
std::expected<UniqueTexture, std::string>
LoadTextureFromPhysFS(const char* Path)
{
    auto* Renderer = ServiceLocator::Get<SDLLayer>().Renderer();
    auto& Physfs   = ServiceLocator::Get<PhysFSLayer>();

    SDL_IOStream* Io = Physfs.OpenAsIOStream(Path);
    if (!Io)
        return std::unexpected(std::format("PhysFS could not open: {}", Path));

    UniqueTexture Tex{ IMG_LoadTexture_IO(Renderer, Io, /*closeio=*/true) };
    if (!Tex)
        return std::unexpected(std::format("IMG_LoadTexture_IO failed: {}", SDL_GetError()));

    return Tex;
}
