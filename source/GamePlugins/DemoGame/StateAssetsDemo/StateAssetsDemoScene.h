#pragma once
#include <GameClasses/SceneObject.h>
#include <SceneRegistry.h>

// ── StateAssetsDemoScene ───────────────────────────────────────────────────────
// Demonstrates AppState / asset-pipeline features.
// Activate via SDL.LoadScene("state-assets-demo") from Lua or
// SDLLayer::LoadScene("state-assets-demo") from C++.
//
// The UI is driven by StateAssetsDemoViewModel (data-model="state-assets-demo").
class StateAssetsDemoScene : public SceneObject
{
public:
    REGISTER_SCENE("state-assets-demo", StateAssetsDemoScene)

    StateAssetsDemoScene(SDL_Renderer* Renderer, SDL_Window* Window);
};

