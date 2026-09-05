#pragma once
#include <Layers/AppLayer.h>
#include <IScriptableObject.h>
#include <LayerRegistry.h>
#include "Render3DPrimitives.h"
#include <SDL3/SDL.h>
#include <expected>
#include <memory>
#include <optional>
#include <string>

//IMPORTANT ASSUMPTION: This demo 3d render is based around the idea of displaying a single .obj model at a time.
//It is not a general-purpose 3D engine, and it does not support multiple models or complex scene graphs.
//It is intended to demonstrate how to integrate a simple 3D render into the UGE framework,
//with a focus on loading a single model, applying transformations, and rendering it with basic lighting.

// ── Render3DObjectLayer ───────────────────────────────────────────────────────
// Engine layer that composites a GPU-rendered 3D viewport onto the shared
// SDL_GPU swapchain.  It sits between SDLLayer (2-D scene) and RmlUILayer (UI) in
// the push order so the 3D pass renders on top of the 2D background / scene and
// below RmlUi overlays.
//
// Rendering uses a real SDL_GPU graphics pipeline: vertex + fragment shaders
// (MVP transform, world-space directional diffuse + ambient lighting) with a
// dedicated depth buffer for correct occlusion.  The plugin records its render
// pass into SDLLayer's shared per-frame command buffer, obtaining the raw
// SDL_GPUDevice / command buffer / swapchain via SDLLayer accessors.
//
// Lifecycle
// ---------
//   Render3DObjectLayer is always present in the layer stack but is a no-op until
//   Activate() is called explicitly.  Deactivate() releases all 3D resources.
//   LoadModel() / UnloadModel() can be called independently of activation state.
//
// Viewport
// --------
//   Use SetViewport() to position and size the 3D render area in swapchain
//   pixels.  If the viewport is not set (width/height <= 0) the layer auto-fills
//   it to the full swapchain each frame.
//
// Per-frame control (typical usage from a SceneObject)
// ----------------------------------------------------
//   if (auto* R = GetRender3DObjectLayer()) {
//       R->SetModelRotation(m_yaw += delta * 45.0f, m_pitch, 0.0f);
//       R->Light().Color = { r, g, b };
//   }
//
// Load order: 4.5 — must follow SDLLayer (4) to access the SDL_GPU device via ServiceLocator.
// Fetches the SDL_GPUDevice pointer from SDLLayer at initialization time.
class Render3DObjectLayer : public AppLayer, public IScriptableObject
{
public:
    REGISTER_LAYER("render3d", 4.5f, Render3DObjectLayer)

    // ── Factory ───────────────────────────────────────────────────────────────
    [[nodiscard]] static std::expected<std::unique_ptr<Render3DObjectLayer>, std::string>
    Create();

    ~Render3DObjectLayer() override;

    Render3DObjectLayer(const Render3DObjectLayer&)            = delete;
    Render3DObjectLayer& operator=(const Render3DObjectLayer&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    // Enable the 3D pass.  Auto-fills viewport to full renderer size if the
    // viewport has not been set explicitly.
    void Activate();

    // Disable the 3D pass and free all 3D resources (mesh data).
    // Calling Activate() again restores the layer to a usable state.
    void Deactivate();

    [[nodiscard]] bool IsActive() const { return m_active; }

    // ── Model loading ─────────────────────────────────────────────────────────

    // Queue a .obj model (and its paired .mtl) for loading from the PhysFS VFS.
    // The actual parse happens at the start of the next Update() call.
    // The .mtl file is resolved relative to the directory of the .obj path.
    // May be called before or after Activate().
    void LoadModel(const char* VirtualPath);

    // Immediately clear the currently loaded mesh.  No-op if none is loaded.
    void UnloadModel();

    [[nodiscard]] bool HasModel() const { return m_mesh != nullptr; }

    // ── Viewport ──────────────────────────────────────────────────────────────

    // Set the rendering area within the SDL window.  All coordinates are in SDL
    // renderer (logical) pixels.  Call before or after Activate().
    void SetViewport(SDL_FRect Rect);

    [[nodiscard]] SDL_FRect Viewport() const { return m_viewport; }

    // ── Transform ─────────────────────────────────────────────────────────────

    // Euler rotation applied to the model each frame (degrees, YXZ order).
    void SetModelRotation(float PitchDeg, float YawDeg, float RollDeg);
    [[nodiscard]] float ModelPitch() const { return m_rotPitch; }
    [[nodiscard]] float ModelYaw()   const { return m_rotYaw;   }
    [[nodiscard]] float ModelRoll()  const { return m_rotRoll;  }

    // Uniform scale applied to the model.
    void SetModelScale(float Scale);

    // Translation of the model in world space.
    void SetModelPosition(float X, float Y, float Z);

    // ── Camera ────────────────────────────────────────────────────────────────

    // Direct access to the camera descriptor — mutate fields as needed.
    [[nodiscard]]       Camera3D& Camera()       { return m_camera; }
    [[nodiscard]] const Camera3D& Camera() const { return m_camera; }

    // Convenience scalar setter also exposed to Lua.
    void SetCamera(float EyeX, float EyeY, float EyeZ,
                   float TargetX, float TargetY, float TargetZ,
                   float FovDeg = 60.0f);

    // ── Lighting ──────────────────────────────────────────────────────────────

    // Direct access to the light descriptor — mutate fields as needed.
    [[nodiscard]]       Light3D& Light()       { return m_light; }
    [[nodiscard]] const Light3D& Light() const { return m_light; }

    // Convenience scalar setters also exposed to Lua.
    // Direction is a world-space vector from surface toward light; normalised internally.
    void SetLightDirection(float X, float Y, float Z);
    // Color channels in [0, 1].
    void SetLightColor(float R, float G, float B);
    void SetAmbientColor(float R, float G, float B);

    // ── Viewport clear ────────────────────────────────────────────────────────

    // Fill the viewport area with this color before rendering the mesh. When set,
    // the 3D render pass clears the color target to this opaque color (giving the
    // 3D viewport an opaque backdrop); when unset, the pass composites the mesh
    // over the existing 2D content via a LOAD op. Pass an alpha of 0 to disable.
    void SetViewportClearColor(SDL_Color Color);
    void ClearViewportClear();

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void Update() override; // processes pending model load; early-outs if inactive
    void Draw(float deltaTime) override; // GPU render pass; early-outs if inactive

    // ── IScriptableObject ─────────────────────────────────────────────────────
    // Registers the "Render3D" Lua table:
    //   Render3D.Activate()
    //   Render3D.Deactivate()
    //   Render3D.IsActive() -> bool
    //   Render3D.LoadModel(virtualPath: string)
    //   Render3D.SetViewport(x, y, w, h: number)
    //   Render3D.SetModelRotation(pitch, yaw, roll: number)
    //   Render3D.SetModelScale(scale: number)
    //   Render3D.SetModelPosition(x, y, z: number)
    //   Render3D.SetCamera(eyeX, eyeY, eyeZ, targetX, targetY, targetZ [, fovDeg])
    //   Render3D.SetLightDirection(x, y, z: number)
    //   Render3D.SetLightColor(r, g, b: number)    -- [0, 1]
    //   Render3D.SetAmbientColor(r, g, b: number)  -- [0, 1]
    //   Render3D.SetClearColor(r, g, b, a: integer) -- [0, 255]; a=0 disables clear
    void RegisterObject(sol::state& Lua) override;

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void RegisterWithServiceLocator() override;

private:
    Render3DObjectLayer() = default;

    SDL_GPUDevice* m_device = nullptr; // non-owning; lifetime is SDLLayer's

    // ── GPU pipeline & resources (owned) ────────────────────────────────────────
    SDL_GPUGraphicsPipeline* m_pipeline     = nullptr;
    SDL_GPUBuffer*           m_vertexBuffer = nullptr;
    SDL_GPUBuffer*           m_indexBuffer  = nullptr;
    Uint32                   m_indexCount   = 0;

    // Depth buffer, sized to the swapchain and recreated on resize.
    SDL_GPUTexture*      m_depthTexture = nullptr;
    SDL_GPUTextureFormat m_depthFormat  = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    Uint32               m_depthWidth   = 0;
    Uint32               m_depthHeight  = 0;

    bool m_active = false;

    // Mesh ─────────────────────────────────────────────────────────────────────
    std::unique_ptr<Mesh3D>    m_mesh;
    std::optional<std::string> m_pendingModelPath;

    // Viewport ─────────────────────────────────────────────────────────────────
    // In swapchain pixels. When width/height <= 0 the layer auto-fills the full
    // swapchain each frame.
    SDL_FRect m_viewport { 0.0f, 0.0f, 0.0f, 0.0f };

    // Transform ────────────────────────────────────────────────────────────────
    float m_rotPitch = 0.0f;
    float m_rotYaw   = 0.0f;
    float m_rotRoll  = 0.0f;
    float m_scale    = 1.0f;
    float m_posX     = 0.0f;
    float m_posY     = 0.0f;
    float m_posZ     = 0.0f;

    // Camera & light ───────────────────────────────────────────────────────────
    Camera3D m_camera;
    Light3D  m_light;

    // Optional solid-color clear before the 3D pass (nullopt = composite over 2D)
    std::optional<SDL_Color> m_viewportClear;

    // Internal ─────────────────────────────────────────────────────────────────
    void loadModelNow(const std::string& VirtualPath);
    void uploadMesh();                            // (re)build GPU vertex/index buffers from m_mesh
    bool ensureDepthTexture(Uint32 Width, Uint32 Height); // (re)create depth buffer on resize
    void releaseGpuResources();                   // release pipeline, buffers, and depth texture
    void renderMesh();                            // record the GPU render pass for the current frame
};

