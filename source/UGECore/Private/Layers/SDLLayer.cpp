#include <Layers/SDLLayer.h>
#include <Layers/LuaLayer.h>
#include <Layers/LoggingLayer.h>
#include <Layers/PhysFSLayer.h>
#include <Layers/PhysicsLayer.h>
#include <GameClasses/SceneObject.h>
#include <SceneRegistry.h>
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
    // Unload the scene first — it may hold SDL resources that must be freed
    // before the renderer and window are destroyed.
    UnloadScene();
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
    , m_activeScene(std::move(other.m_activeScene))
{}

SDLLayer& SDLLayer::operator=(SDLLayer&& other) noexcept
{
    if (this != &other) {
        UnloadScene();
        if (m_logFn) SDL_SetLogOutputFunction(nullptr, nullptr);
        m_renderer.reset();
        m_window.reset();
        if (m_sdlInit) SDL_Quit();
        m_window      = std::move(other.m_window);
        m_renderer    = std::move(other.m_renderer);
        m_sdlInit     = std::exchange(other.m_sdlInit, false);
        m_running     = other.m_running;
        m_logFn       = std::move(other.m_logFn);
        m_activeScene = std::move(other.m_activeScene);
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

// ── IEventHandler ─────────────────────────────────────────────────────────────
bool SDLLayer::HandleEvent(SDL_Event& Event)
{
    if (m_activeScene)
        return m_activeScene->HandleEvent(Event);
    return false;
}

// ── AppLayer ──────────────────────────────────────────────────────────────────
void SDLLayer::Update()
{
    if (m_pendingScene.has_value())
    {
        auto Name = std::move(*m_pendingScene);
        m_pendingScene.reset();
        loadSceneNow(Name.c_str());
    }

    PollEvents(m_eventHandler);
    if (m_activeScene)
        m_activeScene->Update();
}

void SDLLayer::Draw(float deltaTime)
{
    if (m_bgTexture)
        SDL_RenderTexture(m_renderer.get(), m_bgTexture.get(), nullptr, nullptr);

    if (m_activeScene)
        m_activeScene->Draw(deltaTime);
    // Physics body debug overlay is rendered by PhysicsLayer::Draw() when
    // the "debug.show_collision" transient key is set (via Physics.ShowCollision()).
}

// ── Scene management ──────────────────────────────────────────────────────────
void SDLLayer::LoadScene(const char* SceneName)
{
    m_pendingScene = SceneName;
}

void SDLLayer::loadSceneNow(const char* SceneName)
{
    UnloadScene();

    auto NewScene = SceneRegistry::Instance().Create(SceneName, m_renderer.get(), m_window.get());
    if (!NewScene)
    {
        Log(std::format("[SDL] LoadScene: no scene registered for '{}'", SceneName));
        return;
    }

    m_activeScene = std::move(NewScene);
    m_camera = Camera2D{};

    if (auto* Scriptable = dynamic_cast<IScriptableObject*>(m_activeScene.get()))
        if (auto* Lua = ServiceLocator::TryGet<LuaLayer>())
            Lua->Register(Scriptable);

    Log(std::format("[SDL] LoadScene: loaded '{}'", SceneName));
}

void SDLLayer::UnloadScene()
{
    m_pendingScene.reset();

    if (!m_activeScene) return;

    if (auto* Scriptable = dynamic_cast<IScriptableObject*>(m_activeScene.get()))
        if (auto* Lua = ServiceLocator::TryGet<LuaLayer>())
            Lua->Unregister(Scriptable);

    // Destroy scene first — all PhysicsBodyHandle members release their b2 bodies here.
    m_activeScene.reset();

    // Tear down the physics world after all handles have been released.
    if (auto* Physics = ServiceLocator::TryGet<PhysicsLayer>())
        Physics->ShutdownPhysics();
}

// ── IScriptableObject ─────────────────────────────────────────────────────────
void SDLLayer::RegisterObject(sol::state& Lua)
{
    auto Sdl = Lua.create_named_table("SDL");

    Sdl.set_function("SetBackground", [this](const std::string& Path) {
        if (auto Result = SetBackground(Path.c_str()); !Result)
            Log("[SDL] SetBackground error: " + Result.error());
    });

    Sdl.set_function("LoadScene", [this](const std::string& Name) {
        LoadScene(Name.c_str());
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
