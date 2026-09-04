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
            if (auto* Data = GetDataLayer())
                Data->Store.Set(KEY_STATE_STOP, DataValue{m_stateStop});
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
            if (auto* Data = GetDataLayer())
                Data->Store.Set(KEY_COIN_DIRECTION, DataValue{1});
        });

    Ctor.BindEventCallback("onSelectLeft",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            m_coinDirection = "-1";
            m_model.DirtyVariable("coin_direction");
            if (auto* Data = GetDataLayer())
                Data->Store.Set(KEY_COIN_DIRECTION, DataValue{-1});
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

            if (auto* Data = GetDataLayer())
                Data->Store.Set(KEY_STATE_STOP, DataValue{0});
        });

    BindLoadScene(Ctor, "onPrevious", "platformer");
    BindLoadScene(Ctor, "onNext", "state-assets-demo");

    m_model = Ctor.GetModelHandle();

    // Bind transient state keys — set initial values then react to external changes.
    m_coinXBinding = DATA_BIND(KEY_COIN_X, m_coinLeft,
        [this](const Tag&, const DataValue& Val)
        {
            if (const auto* F = Val.TryAs<float>())
                SetCoinPosition(*F, m_coinTop);
        }, Transient);

    m_coinYBinding = DATA_BIND(KEY_COIN_Y, m_coinTop,
        [this](const Tag&, const DataValue& Val)
        {
            if (const auto* F = Val.TryAs<float>())
                SetCoinPosition(m_coinLeft, *F);
        }, Transient);
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
    if (auto* Data = GetDataLayer())
        Data->Store.Set(KEY_STATE_STOP, DataValue{m_stateStop});
}

