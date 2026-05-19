#pragma once
#include <../../UGECore/Public/GameClasses/ViewModel.h>
#include <../../UGECore/Public/ViewModelRegistry.h>
#include <Layers/UDEDataLayer.h>
#include <functional>
#include <string>

// -- PlatformerHudViewModel ----------------------------------------------------
// Data model for the platformer HUD overlay (data-model="platformer-hud").
// Self-registers with ViewModelRegistry; RmlUILayer instantiates it automatically
// when platformer.rml is loaded.
//
// Data-model variables
//   score        � current integer score displayed in the HUD
//
// Event callbacks
//   onNext       � fired when the player clicks the Next arrow button
//
// Public API
//   SetScore(int)  � update the displayed score and dirty the model
//   onNext         � assign a callback to handle the Next button click

class PlatformerHudViewModel : public ViewModel
{
public:
    REGISTER_VIEWMODEL("platformer-hud", PlatformerHudViewModel)

    // Shared state key � used by PlatformerScene (write) and this ViewModel (read).
    static constexpr const char* SCORE_KEY = "plat2d.Score";

    // Called by RmlUILayer before the document tree is parsed.
    void RegisterWith(Rml::Context* Context, const char* ModelName) override;

    // -- Score -----------------------------------------------------------------
    [[nodiscard]] int Score() const { return m_score; }
    void SetScore(int Score);

    // -- Next button callback --------------------------------------------------
    // Assign a handler before or after registration; called on every onNext event.
    std::function<void()> onNext;

private:
    Rml::DataModelHandle m_model;

    int              m_score        = 0;
    AppStateBinding  m_scoreBinding;  // transient subscription to "Platformer2d.Score"
};

