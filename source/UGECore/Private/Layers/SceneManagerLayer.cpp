#include <Layers/SceneManagerLayer.h>
#include <Layers/SDLLayer.h>
#include <Layers/LuaLayer.h>
#include <Layers/PhysicsLayer.h>
#include <GameClasses/SceneObject.h>
#include <SceneRegistry.h>
#include <ServiceLocator.h>
#include <format>
#include <utility>

// ── SceneManagerLayer::Create ─────────────────────────────────────────────────
std::expected<std::unique_ptr<AppLayer>, std::string> SceneManagerLayer::Create()
{
    auto Layer = std::unique_ptr<SceneManagerLayer>(new SceneManagerLayer());
    return std::unique_ptr<AppLayer>(Layer.release());
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
SceneManagerLayer::~SceneManagerLayer()
{
    // Scene teardown is normally driven explicitly by UGEApplication before the
    // layer stack is destroyed (while SDLLayer/PhysicsLayer are still alive).
    // This is a defensive fallback in case that ordering is ever bypassed.
    UnloadScene();
}

// ── AppLayer ──────────────────────────────────────────────────────────────────
void SceneManagerLayer::Update()
{
    if (m_pendingScene.has_value())
    {
        auto Name = std::move(*m_pendingScene);
        m_pendingScene.reset();
        loadSceneNow(Name.c_str());
    }

    if (m_activeScene)
        m_activeScene->Update();
}

void SceneManagerLayer::Draw(float deltaTime)
{
    if (m_activeScene)
        m_activeScene->Draw(deltaTime);
}

// ── IEventHandler ─────────────────────────────────────────────────────────────
bool SceneManagerLayer::HandleEvent(SDL_Event& Event)
{
    if (m_activeScene)
        return m_activeScene->HandleEvent(Event);
    return false;
}

// ── Scene management ──────────────────────────────────────────────────────────
void SceneManagerLayer::LoadScene(const char* SceneName)
{
    m_pendingScene = SceneName;
}

void SceneManagerLayer::loadSceneNow(const char* SceneName)
{
    UnloadScene();

    auto* Sdl = ServiceLocator::TryGet<SDLLayer>();
    if (!Sdl)
    {
        Log("[Scene] LoadScene: SDLLayer not available");
        return;
    }

    auto NewScene = SceneRegistry::Instance().Create(SceneName, Sdl->Renderer(), Sdl->Window());
    if (!NewScene)
    {
        Log(std::format("[Scene] LoadScene: no scene registered for '{}'", SceneName));
        return;
    }

    m_activeScene = std::move(NewScene);

    // Reset the SDLLayer camera to its default on each new scene load.
    Sdl->SetCamera(Camera2D{});

    if (auto* Scriptable = dynamic_cast<IScriptableObject*>(m_activeScene.get()))
        if (auto* Lua = ServiceLocator::TryGet<LuaLayer>())
            Lua->Register(Scriptable);

    Log(std::format("[Scene] LoadScene: loaded '{}'", SceneName));
}

void SceneManagerLayer::UnloadScene()
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
void SceneManagerLayer::RegisterObject(sol::state& Lua)
{
    auto Scene = Lua.create_named_table("Scene");

    Scene.set_function("Load", [this](const std::string& Name) {
        LoadScene(Name.c_str());
    });
}

void SceneManagerLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}

