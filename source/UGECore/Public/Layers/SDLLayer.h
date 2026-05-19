#pragma once
#include "AppLayer.h"
#include "../IScriptableObject.h"
#include "../IEventHandler.h"
#include "../LayerRegistry.h"
#include "../GameClasses/BoxCollisionRegistry.h"
#include <SDL3/SDL.h>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>

class SceneObject;

// ── RAII deleters ─────────────────────────────────────────────────────────────
struct SDLWindowDeleter   { void operator()(SDL_Window*   P) const { SDL_DestroyWindow(P);   } };
struct SDLRendererDeleter { void operator()(SDL_Renderer* P) const { SDL_DestroyRenderer(P); } };
struct SDLTextureDeleter  { void operator()(SDL_Texture*  P) const { SDL_DestroyTexture(P);  } };

using UniqueWindow   = std::unique_ptr<SDL_Window,   SDLWindowDeleter>;
using UniqueRenderer = std::unique_ptr<SDL_Renderer, SDLRendererDeleter>;
using UniqueTexture  = std::unique_ptr<SDL_Texture,  SDLTextureDeleter>;

// ── Free helpers ──────────────────────────────────────────────────────────────
// Loads an image from the PhysFS virtual filesystem into an SDL texture.
// Both the renderer and the PhysFSLayer are resolved via ServiceLocator.
[[nodiscard]] std::expected<UniqueTexture, std::string>
LoadTextureFromPhysFS(const char* Path);

// ── SDLLayer ──────────────────────────────────────────────────────────────────
// Load order: 4 — must follow UDEDataLayer (3).
class SDLLayer : public AppLayer, public IScriptableObject, public IEventHandler
{
public:
    REGISTER_LAYER("sdl", 4.0f, SDLLayer)

    [[nodiscard]] static std::expected<std::unique_ptr<AppLayer>, std::string> Create();

    ~SDLLayer() override;
    SDLLayer(const SDLLayer&)            = delete;
    SDLLayer& operator=(const SDLLayer&) = delete;
    SDLLayer(SDLLayer&&) noexcept;
    SDLLayer& operator=(SDLLayer&&) noexcept;

    [[nodiscard]] SDL_Window*   Window()    const { return m_window.get();   }
    [[nodiscard]] SDL_Renderer* Renderer()  const { return m_renderer.get(); }
    [[nodiscard]] bool          IsRunning() const { return m_running;        }

    void ResizeToTexture(SDL_Texture* Tex) const;
    std::expected<void, std::string> SetBackground(const char* VirtualPath);
    [[nodiscard]] SDL_Texture* Background() const { return m_bgTexture.get(); }

    void SetLogFunction(std::function<void(std::string)> Fn);
    void PollEvents(const std::function<void(SDL_Event&)>& Handler);
    void SetEventHandler(std::function<void(SDL_Event&)> Handler);

    // Clear the renderer to black — called by Application::Run() before the Tick pass.
    void BeginFrame() const;

    // Present the completed frame — called by Application::Run() after the Tick pass.
    void EndFrame() const;

    // ── Scene management ──────────────────────────────────────────────────────
    // Instantiate a registered scene by name, tear down any previously active
    // scene first.  If the scene implements IScriptableObject it is automatically
    // registered with LuaLayer.
    void LoadScene(const char* SceneName);

    // Destroy the active scene (calls ScriptAPILayer::Unregister if applicable).
    // No-op if no scene is loaded.
    void UnloadScene();

    [[nodiscard]] SceneObject* ActiveScene() const { return m_activeScene.get(); }

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void Update() override; // polls events, then calls Scene::Update()
    void Tick(float deltaTime) override; // blits background texture, then calls Scene::Tick(deltaTime)

    // ── IEventHandler ─────────────────────────────────────────────────────────
    // Forwards the event to the active scene's HandleEvent().
    // Returns the scene's result; false if no scene is loaded.
    bool HandleEvent(SDL_Event& Event) override;

    // ── IScriptableObject ─────────────────────────────────────────────────────
    // Registers the "SDL" Lua table with the following functions:
    //   SDL.SetBackground(virtualPath: string)
    //   SDL.LoadScene(sceneName: string)
    void RegisterObject(sol::state& Lua) override;

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void RegisterWithServiceLocator() override;

    // ── Collision debug registry ───────────────────────────────────────────────
    // Register a BoxCollision* for debug rendering.  Returns a CollisionHandle
    // that auto-deregisters on destruction — no manual cleanup required.
    [[nodiscard]] CollisionHandle RegisterCollision(BoxCollision* Box, std::string Label = "");

    // Access the registry directly (e.g. to iterate entries).
    [[nodiscard]]       BoxCollisionRegistry& CollisionRegistry()       { return m_collisionRegistry; }
    [[nodiscard]] const BoxCollisionRegistry& CollisionRegistry() const { return m_collisionRegistry; }

    // Toggle the debug overlay that draws all registered collision boxes.
    void ShowCollisionBoxes(bool Show) { m_showCollisionBoxes = Show; }
    [[nodiscard]] bool IsShowingCollisionBoxes() const { return m_showCollisionBoxes; }

    // Activate auto-registration: call once after SDLLayer reaches its final
    // stable address (i.e. after pushLayer in UGEApplication::Create()).
    // BoxCollision value constructors will register from this point forward.
    void InitCollisionHooks();

private:
    SDLLayer() = default;
    UniqueWindow   m_window;
    UniqueRenderer m_renderer;
    UniqueTexture  m_bgTexture{ nullptr };
    bool           m_sdlInit = false;
    bool           m_running = true;

    // Heap-allocated so its address is stable across moves (used as SDL log userdata)
    std::shared_ptr<std::function<void(std::string)>> m_logFn;
    std::function<void(SDL_Event&)>                   m_eventHandler;

    // Active scene — destroyed before renderer/window in UnloadScene() / destructor.
    std::unique_ptr<SceneObject>     m_activeScene;
    std::optional<std::string> m_pendingScene;

    // Perform the actual scene swap — called from Update() when m_pendingScene has a value.
    void loadSceneNow(const char* SceneName);

    BoxCollisionRegistry m_collisionRegistry;
    bool                 m_showCollisionBoxes = false;
};
