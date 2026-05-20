#include <LuaTestsViewModel.h>
#include <Layers/SDLLayer.h>
#include <Layers/LuaLayer.h>

void LuaTestsViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
{
    SetContext(Context);

    auto Ctor = Context->CreateDataModel(ModelName);

    Ctor.Bind("coin_left",      &m_coinLeft);
    Ctor.Bind("coin_top",       &m_coinTop);
    Ctor.Bind("coin_direction", &m_coinDirection);
    Ctor.Bind("state_stop",     &m_stateStop);

    Ctor.BindEventCallback("onStateStopChange",
        [this](Rml::DataModelHandle, Rml::Event& Event, const Rml::VariantList&)
        {
            auto* El = Event.GetTargetElement();
            if (!El) return;
            const int newValue = El->IsPseudoClassSet("checked") ? 1 : 0;
            // If the element already matches our state this was a programmatic
            // update (DirtyVariable triggered the change event after we already
            // wrote the new value) — ignore it to avoid a double-toggle.
            if (newValue == m_stateStop) return;
            m_stateStop = newValue;
            m_model.DirtyVariable("state_stop");
            if (auto* Data = ServiceLocator::TryGet<UGEDataLayer>())
                Data->Transient.Set(KEY_STATE_STOP, AppStateValue{m_stateStop});
        });

    Ctor.BindEventCallback("onHardStop",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (m_tickId == 0) return;
            if (auto* Lua = GetLuaLayer())
                Lua->RemoveTick(m_tickId);
            m_tickId = 0;
        });

    Ctor.BindEventCallback("onSelectRight",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            m_coinDirection = "1";
            m_model.DirtyVariable("coin_direction");
            if (auto* Data = ServiceLocator::TryGet<UGEDataLayer>())
                Data->Transient.Set(KEY_COIN_DIRECTION, AppStateValue{1});
        });

    Ctor.BindEventCallback("onSelectLeft",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            m_coinDirection = "-1";
            m_model.DirtyVariable("coin_direction");
            if (auto* Data = ServiceLocator::TryGet<UGEDataLayer>())
                Data->Transient.Set(KEY_COIN_DIRECTION, AppStateValue{-1});
        });

    Ctor.BindEventCallback("onStartTick",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {

            // Reset Stateful Stop checkbox before starting
            setStateStop(0);


            auto* Lua = GetLuaLayer();
            if (!Lua) return;

            sol::protected_function Fn = Lua->State()["UGE"]["Ticking"]["tickTest"];
            if (!Fn.valid()) return;

            m_tickId = Lua->AddTickFunction(std::move(Fn), "tickTest");

            if (auto* Data = ServiceLocator::TryGet<UGEDataLayer>())
                Data->Transient.Set(KEY_STATE_STOP, AppStateValue{0});
        });

    Ctor.BindEventCallback("onPrevious",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (auto* SDL = GetSDLLayer())
                SDL->LoadScene("platformer");
        });

    Ctor.BindEventCallback("onNext",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (auto* SDL = GetSDLLayer())
                SDL->LoadScene("state-assets-demo");
        });

    m_model = Ctor.GetModelHandle();

    // Bind transient state keys — set initial values then react to external changes.
    m_coinXBinding = APPSTATE_BIND_TRANSIENT(KEY_COIN_X, m_coinLeft,
        [this](const std::string&, const AppStateValue& Val)
        {
            if (const auto* F = std::get_if<float>(&Val))
                SetCoinPosition(*F, m_coinTop);
        });

    m_coinYBinding = APPSTATE_BIND_TRANSIENT(KEY_COIN_Y, m_coinTop,
        [this](const std::string&, const AppStateValue& Val)
        {
            if (const auto* F = std::get_if<float>(&Val))
                SetCoinPosition(m_coinLeft, *F);
        });
}

void LuaTestsViewModel::SetCoinPosition(float Left, float Top)
{
    m_coinLeft = Left;
    m_coinTop  = Top;
    m_model.DirtyVariable("coin_left");
    m_model.DirtyVariable("coin_top");
}

void LuaTestsViewModel::setStateStop(int Value)
{
    m_stateStop = Value;
    m_model.DirtyVariable("state_stop");
    if (auto* Data = ServiceLocator::TryGet<UGEDataLayer>())
        Data->Transient.Set(KEY_STATE_STOP, AppStateValue{m_stateStop});
}

