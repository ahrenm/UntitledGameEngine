#include <Layers/SDLLayer.h>
#include <Layers/LuaLayer.h>
#include <Layers/LoggingLayer.h>
#include <Layers/PhysFSLayer.h>
#include <GameClasses/SceneObject.h>
#include <GameClasses/BoxCollision.h>
#include <GameClasses/BoxCollisionRegistry.h>
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

    SDL_SetRenderDrawBlendMode(App->m_renderer.get(), SDL_BLENDMODE_BLEND);
    App->InitCollisionHooks();

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
    // Only deactivate if we are the currently live registry — guards against
    // the moved-from temporary in Create() nulling out s_active after the move.
    if (BoxCollisionRegistry::Active() == &m_collisionRegistry)
        BoxCollisionRegistry::SetActive(nullptr);
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
{
    // If the moved-from registry was the active one, re-point to our new address.
    if (BoxCollisionRegistry::Active() == &other.m_collisionRegistry)
        BoxCollisionRegistry::SetActive(&m_collisionRegistry);
}

SDLLayer& SDLLayer::operator=(SDLLayer&& other) noexcept
{
    if (this != &other) {
        UnloadScene();
        if (BoxCollisionRegistry::Active() == &m_collisionRegistry)
            BoxCollisionRegistry::SetActive(nullptr);
        if (m_logFn) SDL_SetLogOutputFunction(nullptr, nullptr);
        m_renderer.reset();
        m_window.reset();
        if (m_sdlInit) SDL_Quit();
        m_window       = std::move(other.m_window);
        m_renderer     = std::move(other.m_renderer);
        m_sdlInit      = std::exchange(other.m_sdlInit, false);
        m_running      = other.m_running;
        m_logFn        = std::move(other.m_logFn);
        m_activeScene  = std::move(other.m_activeScene);

        if (BoxCollisionRegistry::Active() == &other.m_collisionRegistry)
            BoxCollisionRegistry::SetActive(&m_collisionRegistry);
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
    // Process any pending scene load requested since last frame.
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

void SDLLayer::Tick(float deltaTime)
{
    if (m_bgTexture)
        SDL_RenderTexture(m_renderer.get(), m_bgTexture.get(), nullptr, nullptr);

    if (m_activeScene)
        m_activeScene->Tick(deltaTime);

    // ── Collision debug overlay ────────────────────────────────────────────────
    if (m_showCollisionBoxes && !m_collisionRegistry.Empty())
    {
        SDL_SetRenderDrawBlendMode(m_renderer.get(), SDL_BLENDMODE_BLEND);

        for (const auto& Entry : m_collisionRegistry.Entries())
        {
            if (!Entry.box) continue;
            const SDL_FRect Rect {
                Entry.box->X, Entry.box->Y, Entry.box->W, Entry.box->H
            };
            if (Entry.box->Enabled)
            {
                // Semi-transparent fill — active (green)
                SDL_SetRenderDrawColor(m_renderer.get(), 0, 255, 100, 40);
                SDL_RenderFillRect(m_renderer.get(), &Rect);
                // Solid outline
                SDL_SetRenderDrawColor(m_renderer.get(), 0, 255, 100, 220);
                SDL_RenderRect(m_renderer.get(), &Rect);
            }
            else
            {
                // Disabled — grey outline only, no fill
                SDL_SetRenderDrawColor(m_renderer.get(), 160, 160, 160, 120);
                SDL_RenderRect(m_renderer.get(), &Rect);
            }
        }
    }
}

// ── Scene management ──────────────────────────────────────────────────────────
void SDLLayer::LoadScene(const char* SceneName)
{
    // Queue the load; the actual swap happens at the top of the next Update()
    // so any in-progress frame completes cleanly first.
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

    // If the scene is also an IScriptableObject, hook it into Lua automatically.
    if (auto* Scriptable = dynamic_cast<IScriptableObject*>(m_activeScene.get()))
        if (auto* Lua = ServiceLocator::TryGet<LuaLayer>())
            Lua->Register(Scriptable);

    Log(std::format("[SDL] LoadScene: loaded '{}'", SceneName));
}

void SDLLayer::UnloadScene()
{
    m_pendingScene.reset(); // cancel any queued load

    if (!m_activeScene) return;

    // Unhook from Lua before destroying.
    if (auto* Scriptable = dynamic_cast<IScriptableObject*>(m_activeScene.get()))
        if (auto* Lua = ServiceLocator::TryGet<LuaLayer>())
            Lua->Unregister(Scriptable);

    m_activeScene.reset();
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

    Sdl.set_function("ShowCollision", [this]() {
        const bool Next = !m_showCollisionBoxes;
        ShowCollisionBoxes(Next);
        Log(std::format("[SDL] Collision overlay {}", Next ? "ON" : "OFF"));
    });
}

void SDLLayer::RegisterWithServiceLocator()
{
    // SDLLayer::Create() returns unique_ptr<AppLayer>, so pushLayer<AppLayer> would
    // not register the concrete SDLLayer* type. Register explicitly here.
    ServiceLocator::Provide(this);
}

// ── Collision registry ────────────────────────────────────────────────────────
CollisionHandle SDLLayer::RegisterCollision(BoxCollision* Box, std::string Label)
{
    return m_collisionRegistry.Register(Box, std::move(Label));
}

void SDLLayer::InitCollisionHooks()
{
    BoxCollisionRegistry::SetActive(&m_collisionRegistry);
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
