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
    m_nameBinding = DATA_BIND("character.name", std::string{},
        [this](const Tag&, const DataValue& Val)
        {
            if (const auto* S = Val.TryAs<std::string>())
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
        if (const auto* S = V->TryAs<std::string>())
            m_characterName = *S;
    m_panelVisible = m_characterName.empty() ? 0 : 1;

    // ── Bind nameplate anchor from transient store ────────────────────────────
    // A single atomic Vec2 — the callback fires once per frame with both
    // components consistent, so the plate re-centres exactly once.
    m_posBinding = DATA_BIND(TAG_NAMEPLATE_POS.data(), Vec2{},
        [this](const Tag&, const DataValue& Val)
        {
            if (const auto* V = Val.TryAs<Vec2>()) m_anchor = *V;
            updatePosition();
        }, Transient);
    if (const auto* V = m_posBinding.GetValue())
        if (const auto* A = V->TryAs<Vec2>()) m_anchor = *A;

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
    m_panelLeft  = m_anchor.X - estimatedW / 2.0f;
    m_panelTop   = m_anchor.Y - PLATE_H - GAP_Y;

    m_model.DirtyVariable("panel_width");
    m_model.DirtyVariable("panel_left");
    m_model.DirtyVariable("panel_top");
}
