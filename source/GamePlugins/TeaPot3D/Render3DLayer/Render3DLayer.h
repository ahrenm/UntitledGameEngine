#pragma once
#include <Layers/AppLayer.h>
#include <IScriptableObject.h>
#include <LayerRegistry.h>
#include <Render3D/Camera3D.h>
#include <Render3D/Light3D.h>
#include <Render3D/Mesh3D.h>
#include <SDL3/SDL.h>
#include <expected>
#include <memory>
#include <optional>
#include <string>


// ── Render3DLayer ─────────────────────────────────────────────────────────────
// Engine layer that composites a CPU-rasterised 3D viewport onto the SDL
// renderer.  It sits between SDLLayer (2-D scene) and RmlUILayer (UI) in the
// push order so the 3D pass renders on top of the 2D background / scene and
// below RmlUi overlays.
//
// Rendering uses SDL_RenderGeometry() with per-vertex Phong shading computed on
// the CPU and painter's-algorithm (back-to-front face sort) for depth ordering.
// Because the layer draws into the existing SDL_Renderer there is no conflict
// with RmlUi's renderer — no SDL_GPUDevice or OpenGL context is required.
//
// Lifecycle
// ---------
//   Render3DLayer is always present in the layer stack but is a no-op until
//   Activate() is called explicitly.  Deactivate() releases all 3D resources.
//   LoadModel() / UnloadModel() can be called independently of activation state.
//
// Viewport
// --------
//   Use SetViewport() to position and size the 3D render area in SDL window
//   coordinates.  If the viewport is not set before Activate() the layer
//   auto-fills it to match the full renderer output size.
//
// Per-frame control (typical usage from a SceneObject)
// ----------------------------------------------------
//   if (auto* R = GetRender3DLayer()) {
//       R->SetModelRotation(m_yaw += delta * 45.0f, m_pitch, 0.0f);
//       R->Light().Color = { r, g, b };
//   }
//
// Load order: 4.5 — must follow SDLLayer (4) to access the SDL renderer via ServiceLocator.
// Fetches the SDL_Renderer pointer from SDLLayer at initialization time.
class Render3DLayer : public AppLayer, public IScriptableObject
{
public:
    REGISTER_LAYER("render3d", 4.5f, Render3DLayer)

    // ── Factory ───────────────────────────────────────────────────────────────
    [[nodiscard]] static std::expected<std::unique_ptr<Render3DLayer>, std::string>
    Create();

    ~Render3DLayer() override = default;

    Render3DLayer(const Render3DLayer&)            = delete;
    Render3DLayer& operator=(const Render3DLayer&) = delete;

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

    // Fill the viewport area with this color before rendering, using the SDL
    // draw color + SDL_RenderFillRect (respects the active viewport clip).
    // Pass an alpha of 0 to disable (default: no clear, background shows through).
    void SetViewportClearColor(SDL_Color Color);
    void ClearViewportClear();

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void Update() override; // processes pending model load; early-outs if inactive
    void Tick(float deltaTime) override; // CPU rasterization pass; early-outs if inactive

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
    Render3DLayer() = default;

    SDL_Renderer* m_renderer = nullptr; // non-owning; lifetime is SDLLayer's

    bool m_active = false;

    // Mesh ─────────────────────────────────────────────────────────────────────
    std::unique_ptr<Mesh3D>    m_mesh;
    std::optional<std::string> m_pendingModelPath;

    // Viewport ─────────────────────────────────────────────────────────────────
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

    // Optional solid-color clear before the 3D pass (nullopt = transparent / passthrough)
    std::optional<SDL_Color> m_viewportClear;

    // Internal ─────────────────────────────────────────────────────────────────
    void loadModelNow(const std::string& VirtualPath);
    void renderMesh();
};


