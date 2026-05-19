#pragma once
#include <GameClasses/ViewModel.h>
#include <Layers/UDEDataLayer.h>
#include <ViewModelRegistry.h>
#include <string>

// ── PlatformerNamePlateViewModel ──────────────────────────────────────────────
// Drives the plat2d-nameplate fragment that floats above the player sprite.
//
// Data model variables:
//   panel_left     — computed left offset (px, reference space) — centred on anchor
//   panel_top      — computed top offset  (px, reference space) — above anchor
//   panel_visible  — 1 when character.name is non-empty, else 0
//   character_name — mirrors persistent "character.name" for display
//
// Position anchor (TAG_NAMEPLATE_X / TAG_NAMEPLATE_Y) is top-centre of the
// player sprite written each frame by PlatformerCharacter::Update().
// The ViewModel offsets leftward by half the estimated rendered label width
// to visually centre the plate over the character.

class PlatformerNamePlateViewModel : public ViewModel
{
public:
    REGISTER_VIEWMODEL("plat2d-nameplate", PlatformerNamePlateViewModel)

    void RegisterWith(Rml::Context* Context, const char* ModelName) override;

private:
    // ── Centering assumptions (demo-quality hardcoded values) ─────────────────
    // LatoLatin bold at 26px ≈ 15px average character width, 24px total padding.
    static constexpr float APPROX_CHAR_W = 15.0f;
    static constexpr float PADDING_H     = 00.0f;  // horizontal padding total
    static constexpr float PLATE_H       = 40.0f;  // plate height in reference px
    static constexpr float GAP_Y         = 10.0f;  // gap between plate bottom and sprite top

    // ── Data model variables ──────────────────────────────────────────────────
    float       m_panelLeft    = 0.0f;
    float       m_panelTop     = 0.0f;
    float       m_panelWidth   = 0.0f;
    int         m_panelVisible = 0;
    std::string m_characterName;

    // ── Cached anchor position from transient store ───────────────────────────
    float m_anchorX = 0.0f;
    float m_anchorY = 0.0f;

    // ── Reactive bindings ─────────────────────────────────────────────────────
    AppStateBinding m_nameBinding;
    AppStateBinding m_posXBinding;
    AppStateBinding m_posYBinding;

    Rml::DataModelHandle m_model;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void updatePosition();
};
