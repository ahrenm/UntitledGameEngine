#include <TeaPotDemoViewModel.h>
#include <Layers/SDLLayer.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

// ── RegisterWith ──────────────────────────────────────────────────────────────
void TeaPotDemoViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
{
    SetContext(Context);

    // Seed the transient store with the default colour before the model is built.
    if (auto* Data = ServiceLocator::TryGet<UGEDataLayer>())
        Data->Transient.Set(KEY_LIGHT_COLOR, AppStateValue{m_lightColor});

    auto Ctor = Context->CreateDataModel(ModelName);

    // Bind the current colour name so the select can reflect the active choice.
    Ctor.Bind("light_color", &m_lightColor);

    // ── Light colour dropdown ─────────────────────────────────────────────────
    Ctor.BindEventCallback("onLightColorChange",
        [this](Rml::DataModelHandle, Rml::Event& Event, const Rml::VariantList&)
        {
            auto* El = Event.GetTargetElement();
            if (!El) return;
            auto* FormCtrl = rmlui_dynamic_cast<Rml::ElementFormControl*>(El);
            if (!FormCtrl) return;

            m_lightColor = FormCtrl->GetValue();
            m_model.DirtyVariable("light_color");

            if (auto* Data = ServiceLocator::TryGet<UGEDataLayer>())
                Data->Transient.Set(KEY_LIGHT_COLOR, AppStateValue{m_lightColor});
        });

    // ── Navigation ───────────────────────────────────────────────────────────
    Ctor.BindEventCallback("onPrevious",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (auto* SDL = GetSDLLayer())
                SDL->LoadScene("state-assets-demo");
        });


    m_model = Ctor.GetModelHandle();

    // Subscribe so external writes to KEY_LIGHT_COLOR (e.g. from Lua console)
    // are reflected back in the bound variable.
    m_lightColorBinding = APPSTATE_BIND_TRANSIENT(KEY_LIGHT_COLOR, m_lightColor,
        [this](const std::string&, const AppStateValue& Val)
        {
            if (const auto* S = std::get_if<std::string>(&Val))
            {
                m_lightColor = *S;
                m_model.DirtyVariable("light_color");
            }
        });
}

