#pragma once
#include <GameClasses/ViewModel.h>
#include <ViewModelRegistry.h>
#include <Layers/UGEDataLayer.h>
#include <string>

// ── TeaPotDemoViewModel ────────────────────────────────────────────────────────
// Data model for the Teapot Demo overlay (data-model="teapot-demo").
// Provides navigation and a light-colour selector that writes the chosen colour
// name to the transient AppState key KEY_LIGHT_COLOR so TeaPotDemoScene can
// react and call Render3DLayer::SetLightColor().
class TeaPotDemoViewModel : public ViewModel
{
public:
    REGISTER_VIEWMODEL("teapot-demo", TeaPotDemoViewModel)

    // Transient key shared with TeaPotDemoScene.
    static constexpr const char* KEY_LIGHT_COLOR = "teapot3d.light.color";

    void RegisterWith(Rml::Context* Context, const char* ModelName) override;

private:
    Rml::DataModelHandle m_model;

    std::string     m_lightColor = "white";   // bound to the <select> display
    AppStateBinding m_lightColorBinding;
};

