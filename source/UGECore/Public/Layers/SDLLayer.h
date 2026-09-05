#pragma once
#include "AppLayer.h"
#include "../IScriptableObject.h"
#include "../LayerRegistry.h"
#include "../Camera2D.h"
#include <Render/GpuTexture.h>
#include <SDL3/SDL.h>
#include <expected>
#include <functional>
#include <memory>
#include <string>

class Renderer2D;


// ── RAII deleters ─────────────────────────────────────────────────────────────
struct SDLWindowDeleter   { void operator()(SDL_Window*   P) const { SDL_DestroyWindow(P);   } };

using UniqueWindow   = std::unique_ptr<SDL_Window,   SDLWindowDeleter>;

// ── GpuFrameContext ───────────────────────────────────────────────────────────
// The raw SDL_GPU handles that make up the in-flight frame. SDLLayer owns the
// single command buffer + swapchain acquisition per frame; layers and plugins
// record their draws into this shared context (design principle: Core owns the
// plumbing, plugins keep raw SDL access). Valid only between BeginFrame()/EndFrame().
struct GpuFrameContext
{
    SDL_GPUCommandBuffer* CommandBuffer    = nullptr;
    SDL_GPUTexture*       SwapchainTexture = nullptr;
    Uint32                Width            = 0;
    Uint32                Height           = 0;
};

// ── Free helpers ──────────────────────────────────────────────────────────────
// Loads an image from the PhysFS virtual filesystem into a GPU texture.
// The SDL_GPUDevice (via SDLLayer) and PhysFSLayer are resolved via ServiceLocator.
[[nodiscard]] std::expected<GpuTexture, std::string>
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
    [[nodiscard]] bool          IsRunning() const { return m_running;        }

    // ── SDL_GPU handles ───────────────────────────────────────────────────────
    // The GPU device is created and owned by SDLLayer; it is non-owning from a
    // consumer's perspective. Plugins may use it directly to build their own
    // pipelines (e.g. Render3DObjectLayer). The command buffer / swapchain texture are
    // only valid between BeginFrame() and EndFrame().
    [[nodiscard]] SDL_GPUDevice*        Device()           const { return m_device;       }
    [[nodiscard]] SDL_GPUCommandBuffer* CommandBuffer()    const { return m_cmdBuf;        }
    [[nodiscard]] SDL_GPUTexture*       SwapchainTexture() const { return m_swapchainTex;  }
    [[nodiscard]] Uint32                SwapchainWidth()   const { return m_swapchainW;    }
    [[nodiscard]] Uint32                SwapchainHeight()  const { return m_swapchainH;    }

    // Aggregate accessor for the in-flight frame's raw GPU handles.
    [[nodiscard]] GpuFrameContext Frame() const
    {
        return GpuFrameContext{ m_cmdBuf, m_swapchainTex, m_swapchainW, m_swapchainH };
    }

    // Core convenience 2D renderer (textured/colored quads, letterbox projection).
    // Owned by SDLLayer; records into the shared per-frame command buffer. Non-null
    // after Create(). Layers/plugins draw sprites and fills through this.
    [[nodiscard]] Renderer2D* Get2DRenderer() const { return m_renderer2D.get(); }

    // True while a valid swapchain image has been acquired for this frame
    // (false when minimized). Layers should skip drawing when this is false.
    [[nodiscard]] bool IsFrameActive() const { return m_frameActive; }

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

    // Resize the window to the given pixel dimensions (used to match the window
    // to a freshly-loaded background texture).
    void ResizeToTexture(int Width, int Height) const;
    std::expected<void, std::string> SetBackground(const char* VirtualPath);
    [[nodiscard]] const GpuTexture* Background() const { return &m_bgTexture; }

    void SetLogFunction(std::function<void(std::string)> Fn);
    void PollEvents(const std::function<void(SDL_Event&)>& Handler);
    void SetEventHandler(std::function<void(SDL_Event&)> Handler);

    // Acquire a command buffer + swapchain image and clear it — called by
    // Application::Run() before the per-layer Draw() pass. No-op draws follow
    // when the window is minimized (IsFrameActive() == false).
    void BeginFrame();

    // Submit the frame's command buffer — called by Application::Run() after the
    // per-layer Draw() pass.
    void EndFrame();

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
    SDL_GPUDevice* m_device = nullptr;   // owned; released in dtor after window detach
    std::unique_ptr<Renderer2D> m_renderer2D;  // owned; built in Create()
    GpuTexture     m_bgTexture;
    bool           m_sdlInit  = false;
    bool           m_running  = true;
    int            m_refWidth  = 1600;
    int            m_refHeight = 1200;
    Camera2D       m_camera;

    // ── Per-frame GPU state (valid between BeginFrame()/EndFrame()) ────────────
    SDL_GPUCommandBuffer* m_cmdBuf       = nullptr;
    SDL_GPUTexture*       m_swapchainTex = nullptr;
    Uint32                m_swapchainW   = 0;
    Uint32                m_swapchainH   = 0;
    bool                  m_frameActive  = false;

    std::shared_ptr<std::function<void(std::string)>> m_logFn;
    std::function<void(SDL_Event&)>                   m_eventHandler;
};
