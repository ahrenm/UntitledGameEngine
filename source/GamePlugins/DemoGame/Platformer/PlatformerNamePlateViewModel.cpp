#include <PlatformerNamePlateViewModel.h>
#include <PlatformerStateTags.h>

void PlatformerNamePlateViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
{
    SetContext(Context);

    auto Ctor = Context->CreateDataModel(ModelName);

    Ctor.Bind("panel_left",     &m_panelLeft);
    Ctor.Bind("panel_top",      &m_panelTop);
    Ctor.Bind("panel_width",    &m_panelWidth);
    Ctor.Bind("panel_visible",  &m_panelVisible);
    Ctor.Bind("character_name", &m_characterName);

    m_model = Ctor.GetModelHandle();

    // ── Bind character name from persistent store ─────────────────────────────
    // Shows/hides the plate and re-centres it when the name changes.
    m_nameBinding = APPSTATE_BIND("character.name", std::string{},
        [this](const std::string&, const AppStateValue& Val)
        {
            if (const auto* S = std::get_if<std::string>(&Val))
                m_characterName = *S;
            else
                m_characterName.clear();

            m_panelVisible = m_characterName.empty() ? 0 : 1;
            updatePosition();
            m_model.DirtyVariable("character_name");
            m_model.DirtyVariable("panel_visible");
        });

    // Seed name from current store value.
    if (const auto* V = m_nameBinding.GetValue())
        if (const auto* S = std::get_if<std::string>(V))
            m_characterName = *S;
    m_panelVisible = m_characterName.empty() ? 0 : 1;

    // ── Bind nameplate anchor X from transient store ──────────────────────────
    m_posXBinding = APPSTATE_BIND_TRANSIENT(TAG_NAMEPLATE_X.data(), 0.0f,
        [this](const std::string&, const AppStateValue& Val)
        {
            if (const auto* F = std::get_if<float>(&Val)) m_anchorX = *F;
            updatePosition();
        });
    if (const auto* V = m_posXBinding.GetValue())
        if (const auto* F = std::get_if<float>(V)) m_anchorX = *F;

    // ── Bind nameplate anchor Y from transient store ──────────────────────────
    m_posYBinding = APPSTATE_BIND_TRANSIENT(TAG_NAMEPLATE_Y.data(), 0.0f,
        [this](const std::string&, const AppStateValue& Val)
        {
            if (const auto* F = std::get_if<float>(&Val)) m_anchorY = *F;
            updatePosition();
        });
    if (const auto* V = m_posYBinding.GetValue())
        if (const auto* F = std::get_if<float>(V)) m_anchorY = *F;

    // Compute initial position.
    updatePosition();
}

// ── updatePosition ────────────────────────────────────────────────────────────
// Recomputes panel_left / panel_top from the current anchor and name length,
// then dirties the model variables so RmlUi picks up the change.
void PlatformerNamePlateViewModel::updatePosition()
{
    const float estimatedW = static_cast<float>(m_characterName.size()) * APPROX_CHAR_W + PADDING_H;
    m_panelWidth = estimatedW;
    m_panelLeft  = m_anchorX - estimatedW / 2.0f;
    m_panelTop   = m_anchorY - PLATE_H - GAP_Y;

    m_model.DirtyVariable("panel_width");
    m_model.DirtyVariable("panel_left");
    m_model.DirtyVariable("panel_top");
}
