#include <TeaPotDemoViewModel.h>
#include <Layers/SDLLayer.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

// ── RegisterWith ──────────────────────────────────────────────────────────────
void TeaPotDemoViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
{
    SetContext(Context);

    // Seed the transient store with the default colour before the model is built.
    if (auto* Data = GetDataLayer())
        Data->Store.Set(KEY_LIGHT_COLOR, DataValue{m_lightColor});

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

            if (auto* Data = GetDataLayer())
                Data->Store.Set(KEY_LIGHT_COLOR, DataValue{m_lightColor});
        });

    // ── Navigation ───────────────────────────────────────────────────────────
    BindLoadScene(Ctor, "onPrevious", "state-assets-demo");

    m_model = Ctor.GetModelHandle();

    // Subscribe so external writes to KEY_LIGHT_COLOR (e.g. from Lua console)
    // are reflected back in the bound variable.
    VM_BIND_MODEL_STRING(m_lightColorBinding, KEY_LIGHT_COLOR, m_lightColor,
                         m_model, "light_color", Transient);
}

