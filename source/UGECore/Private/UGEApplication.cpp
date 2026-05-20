#include <UGEApplication.h>
#include <Layers/LoggingLayer.h>
#include <Layers/LuaLayer.h>
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
    //TODO: consider moving target FPS to launch settings/transient
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
                Layer->Tick(deltaTime);
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
