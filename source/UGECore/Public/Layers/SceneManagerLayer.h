#pragma once
#include "AppLayer.h"
#include "../IScriptableObject.h"
#include "../IEventHandler.h"
#include "../LayerRegistry.h"
#include <expected>
#include <memory>
#include <optional>
#include <string>

class SceneObject;

// ── SceneManagerLayer ─────────────────────────────────────────────────────────
// Load order: 4.05 — sits between SDLLayer (4.0) and PhysicsLayer (4.1) so that:
//   Update() — pending-scene resolve + Scene::Update() runs after SDLLayer polls
//              input (4.0) and before PhysicsLayer steps the world (4.1), so
//              scene logic sets impulses before physics integrates them.
//   Draw()   — Scene::Draw() runs after SDLLayer blits the background (4.0) and
//              before PhysicsLayer composites its debug overlay (4.1), so scene
//              geometry renders beneath physics debug and any RmlUi overlay.
//
// Owns exactly one active SceneObject at a time.  Loading a new scene tears down
// the previous one automatically.  The renderer and window required to construct
// a scene are fetched from SDLLayer via ServiceLocator at load time.
//
// If a scene implements IScriptableObject it is registered/unregistered with
// LuaLayer alongside its lifetime.  Scene teardown also shuts down the physics
// world (via PhysicsLayer) so the next scene starts fresh.
class SceneManagerLayer : public AppLayer, public IScriptableObject, public IEventHandler
{
public:
    REGISTER_LAYER("scene-manager", 4.05f, SceneManagerLayer)

    [[nodiscard]] static std::expected<std::unique_ptr<AppLayer>, std::string> Create();

    ~SceneManagerLayer() override;
    SceneManagerLayer(const SceneManagerLayer&)            = delete;
    SceneManagerLayer& operator=(const SceneManagerLayer&) = delete;

    // ── Scene management ──────────────────────────────────────────────────────
    // Queue a registered scene for loading on the next Update(); tears down any
    // previously active scene first.  If the scene implements IScriptableObject
    // it is automatically registered with LuaLayer.
    void LoadScene(const char* SceneName);

    // Destroy the active scene (unregisters from LuaLayer if applicable and shuts
    // down the physics world).  No-op if no scene is loaded.
    void UnloadScene();

    [[nodiscard]] SceneObject* ActiveScene() const { return m_activeScene.get(); }

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void Update() override;               // resolves pending scene, then Scene::Update()
    void Draw(float deltaTime) override;  // Scene::Draw(deltaTime)

    // ── IEventHandler ─────────────────────────────────────────────────────────
    // Forwards the event to the active scene's HandleEvent().
    // Returns the scene's result; false if no scene is loaded.
    bool HandleEvent(SDL_Event& Event) override;

    // ── IScriptableObject ─────────────────────────────────────────────────────
    // Registers the "Scene" Lua table with the following functions:
    //   Scene.Load(sceneName: string)
    void RegisterObject(sol::state& Lua) override;

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void RegisterWithServiceLocator() override;

private:
    SceneManagerLayer() = default;

    std::unique_ptr<SceneObject> m_activeScene;
    std::optional<std::string>   m_pendingScene;

    void loadSceneNow(const char* SceneName);
};

