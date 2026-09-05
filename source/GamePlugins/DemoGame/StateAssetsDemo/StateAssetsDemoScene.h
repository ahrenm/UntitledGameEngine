#pragma once
#include <GameClasses/SceneObject.h>
#include <SceneRegistry.h>
#include <Layers/UGEDataLayer.h>
#include <string_view>

// ── StateAssetsDemoScene ───────────────────────────────────────────────────────
// Demonstrates AppState / asset-pipeline features.
// Activate via Scene.Load("state-assets-demo") from Lua or
// SceneManagerLayer::LoadScene("state-assets-demo") from C++.
//
// The UI is driven by StateAssetsDemoViewModel (data-model="state-assets-demo").
// All save/load business logic lives here in the scene; the ViewModel is a thin
// UI layer that talks to the scene exclusively through transient DataStore keys
// (the same UI→Scene bridge pattern used by TeaPotDemoScene / light colour).
class StateAssetsDemoScene : public SceneObject
{
public:
    REGISTER_SCENE("state-assets-demo", StateAssetsDemoScene)

    StateAssetsDemoScene(Renderer2D* Renderer, SDL_Window* Window);

    // Shared DataStore keys (UI ↔ Scene bridge) live in StateAssetsDemoKeys.h so
    // the ViewModel can reference them without depending on this Scene.

private:
    // Validates numeric fields, writes character.* + character.data.* to the
    // persistent store, serialises to Game.sav, and reports status.
    void handleSaveRequest();

    // Deserialises Game.sav, then pushes character fields and up to three free
    // KV pairs back into the transient form keys the ViewModel is bound to.
    void handleLoadRequest();

    // Convenience: write the Scene → UI status line.
    void setStatus(std::string_view Msg);

    DataBinding m_saveRequestBinding;
    DataBinding m_loadRequestBinding;
};

