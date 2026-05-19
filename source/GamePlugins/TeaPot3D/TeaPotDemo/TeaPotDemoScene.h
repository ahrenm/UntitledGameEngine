#pragma once
#include <GameClasses/SceneObject.h>
#include <SceneRegistry.h>
#include <Layers/UDEDataLayer.h>

// ── TeaPotDemoScene ────────────────────────────────────────────────────────────
// A full-viewport 3D scene that renders the Utah teapot (assets/3d/teapot.obj)
// using Render3DLayer.  No 2-D background is shown — the 3D layer clears to a
// solid dark colour before the model is rasterised.
//
// The teapot rotates continuously around the Y-axis.  The active light colour
// is driven by the transient AppState key TeaPotDemoViewModel::KEY_LIGHT_COLOR,
// which is written by the colour dropdown in the UI overlay.
//
// Activate via SDL.LoadScene("teapot-demo") from Lua or
// SDLLayer::LoadScene("teapot-demo") from C++.
//
// The UI overlay is driven by TeaPotDemoViewModel (data-model="teapot-demo").
class TeaPotDemoScene : public SceneObject
{
public:
    REGISTER_SCENE("teapot-demo", TeaPotDemoScene)

    TeaPotDemoScene(SDL_Renderer* Renderer, SDL_Window* Window);
    ~TeaPotDemoScene() override;

    void Update()              override;
    void Tick(float deltaTime) override;

private:
    float m_yaw = 0.0f;   // accumulated rotation around Y-axis (degrees)

    AppStateBinding m_lightColorBinding;

    // Maps a colour name (from the UI dropdown) to RGB [0,1] values and
    // immediately applies it to Render3DLayer.
    void applyLightColor(const std::string& ColorName);
};

