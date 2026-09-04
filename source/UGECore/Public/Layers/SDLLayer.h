#pragma once
#include "AppLayer.h"
#include "../IScriptableObject.h"
#include "../LayerRegistry.h"
#include "../Camera2D.h"
#include <SDL3/SDL.h>
#include <expected>
#include <functional>
#include <memory>
#include <string>


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
// Load order: 4 — must follow UGEDataLayer (3).
class SDLLayer : public AppLayer, public IScriptableObject
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

    // ── Logical reference resolution ──────────────────────────────────────────
    // Matches the values passed to SDL_SetRenderLogicalPresentation in Create().
    [[nodiscard]] int RefWidth()  const { return m_refWidth;  }
    [[nodiscard]] int RefHeight() const { return m_refHeight; }

    // ── Camera ────────────────────────────────────────────────────────────────
    // The active 2-D view transform used by SceneObject::WorldRect() helpers.
    // Default camera has FlipY=true, zero offset, zoom=1 — renders Y-up world
    // space mapped transparently to SDL's Y-down screen space.
    // Camera is reset to its default state when a new scene is loaded.
    void                  SetCamera(Camera2D Cam)     { m_camera = Cam; }
    [[nodiscard]] const Camera2D& GetCamera() const   { return m_camera; }
    void MoveCamera(float Dx, float Dy) { m_camera.WorldX += Dx; m_camera.WorldY += Dy; }

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

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void Update() override; // polls events
    void Draw(float deltaTime) override; // blits the background texture

    // ── IScriptableObject ─────────────────────────────────────────────────────
    // Registers the "SDL" Lua table with the following functions:
    //   SDL.SetBackground(virtualPath: string)
    //   SDL.SetCamera(worldX: number, worldY: number)
    //   SDL.MoveCamera(dx: number, dy: number)
    void RegisterObject(sol::state& Lua) override;

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void RegisterWithServiceLocator() override;

private:
    SDLLayer() = default;
    UniqueWindow   m_window;
    UniqueRenderer m_renderer;
    UniqueTexture  m_bgTexture{ nullptr };
    bool           m_sdlInit  = false;
    bool           m_running  = true;
    int            m_refWidth  = 1600;
    int            m_refHeight = 1200;
    Camera2D       m_camera;

    std::shared_ptr<std::function<void(std::string)>> m_logFn;
    std::function<void(SDL_Event&)>                   m_eventHandler;
};
