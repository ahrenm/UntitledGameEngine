#include <UGEApplication.h>
#include <Layers/LoggingLayer.h>
#include <Layers/LuaLayer.h>
#include <Layers/SceneManagerLayer.h>
#include <ServiceLocator.h>
#include <IEventHandler.h>
#include <LayerRegistry.h>
#include <SDL3/SDL.h>
#include <ranges>
#include <utility>



// Static definition
LaunchSettings UGEApplication::Settings;


std::expected<std::unique_ptr<UGEApplication>, std::string>
UGEApplication::Create(int Argc, char* Argv[], LaunchSettings LaunchConfig)
{
    Settings = std::move(LaunchConfig);

    auto App = std::unique_ptr<UGEApplication>(new UGEApplication());

    // BASE UGECore LAYERS
    // LoggingLayer | PhysFSLayer (virtual file system) | UGEDataLayer| SDLLayer | LUALayer| RmlUILayer

    // ── Push all registered layers in load order ─────────────────────────
    for (const auto& Name : LayerRegistry::Instance().Names())
    {
        auto Result = LayerRegistry::Instance().Create(Name);
        if (!Result) return std::unexpected(Result.error());
        App->pushLayer(std::move(*Result));
    }

    // ── Fetch core observer pointer used by Run() ──────────────────────────────
    App->m_sdlLayer = ServiceLocator::TryGet<SDLLayer>();
    if (!App->m_sdlLayer)
        return std::unexpected("UGEApplication: SDLLayer failed to initialize");

    auto* luaLayer = ServiceLocator::TryGet<LuaLayer>();

    // ── Register all IScriptableObjects with LuaLayer ────────────────────────
    if (luaLayer)
        for (auto& Layer : App->m_layers)
            if (auto* Scriptable = dynamic_cast<IScriptableObject*>(Layer.get()))
                luaLayer->Register(Scriptable);

    // ── Event handler ─────────────────────────────────────────────────────────
    App->m_sdlLayer->SetEventHandler([A = App.get()](SDL_Event& E) {
        A->dispatchEvent(E);
    });

    // ── Init script ───────────────────────────────────────────────────────────
    if (!Settings.InitScript.empty())
    {
        if (!luaLayer)
            return std::unexpected("UGEApplication: LuaLayer is required when InitScript is set");
        if (auto R = luaLayer->ExecuteFile(Settings.InitScript.c_str()); !R)
            return std::unexpected(R.error());
    }

    if (auto* logLayer = ServiceLocator::TryGet<LoggingLayer>())
        logLayer->Log("Application init complete.");
    return App;
}

void UGEApplication::Run()
{
    //TODO: consider moving target FPS to launch settings/datastore
    constexpr Uint64 TARGET_FRAME_NS = 1'000'000'000ULL / 30; // ~33.33 ms
    Uint64 NextFrame = SDL_GetTicksNS();
    Uint64 LastFrame = NextFrame;

    while (m_sdlLayer->IsRunning())
    {
        const Uint64 Now = SDL_GetTicksNS();

        if (Now >= NextFrame)
        {
            // Actual elapsed seconds since the last tick fired.
            const float deltaTime = static_cast<float>(Now - LastFrame) / 1'000'000'000.0f;
            LastFrame = Now;

            NextFrame += TARGET_FRAME_NS;
            if (NextFrame < Now)
                NextFrame = Now + TARGET_FRAME_NS;

            for (auto& Layer : m_layers)
                Layer->Update();

            m_sdlLayer->BeginFrame();
            for (auto& Layer : m_layers)
                Layer->Draw(deltaTime);
            m_sdlLayer->EndFrame();
        }
        else
        {
            SDL_DelayNS(NextFrame - Now);
        }
    }
}

void UGEApplication::dispatchEvent(SDL_Event& Event)
{
    for (auto& Layer : m_layers | std::views::reverse)
        if (auto* Handler = dynamic_cast<IEventHandler*>(Layer.get()))
            if (Handler->HandleEvent(Event))
                return;
}

UGEApplication::~UGEApplication()
{
    // Tear the active scene down while every layer it depends on is still alive.
    // Scenes may own GPU textures (freed via SDLLayer's device) and physics
    // bodies (freed via PhysicsLayer).  Doing this here — before the m_layers
    // vector destroys its elements — preserves the ordering guarantees that used
    // to be provided by SDLLayer owning the scene directly.
    if (auto* SceneMgr = ServiceLocator::TryGet<SceneManagerLayer>())
        SceneMgr->UnloadScene();

    // Destroy layers in reverse of push (load) order (LIFO). Higher-order layers
    // (RmlUILayer, Render3DObjectLayer, …) own GPU resources created from the device
    // owned by SDLLayer (load order 4.0). A std::vector otherwise destroys its
    // elements front-to-back, which would free the device before those layers can
    // release their resources — a use-after-free. Tearing down last-pushed-first,
    // while the ServiceLocator entries are still valid, lets each layer release
    // against a live device. SDLLayer, having the lowest load order, is destroyed
    // last (after all GPU-owning layers).
    while (!m_layers.empty())
        m_layers.pop_back();

    ServiceLocator::Clear();
}
