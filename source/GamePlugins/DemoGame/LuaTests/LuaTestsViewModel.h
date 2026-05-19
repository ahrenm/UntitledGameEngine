#pragma once
#include <GameClasses/ViewModel.h>
#include <ViewModelRegistry.h>
#include <Layers/UDEDataLayer.h>
#include <functional>

// ── LuaTestsViewModel ─────────────────────────────────────────────────────────
// Placeholder data model for the Lua test runner panel (data-model="lua-tests").
class LuaTestsViewModel : public ViewModel
{
public:
    REGISTER_VIEWMODEL("lua-tests", LuaTestsViewModel)

    void RegisterWith(Rml::Context* Context, const char* ModelName) override;

    void SetCoinPosition(float Left, float Top);

    // Assign a callback to be invoked when the Next button is clicked.
    std::function<void()> onNext;

    static constexpr const char* KEY_COIN_X         = "luaTests.coinPosition.x";
    static constexpr const char* KEY_COIN_Y         = "luaTests.coinPosition.y";
    static constexpr const char* KEY_COIN_DIRECTION = "luaTests.coinDirection";
    static constexpr const char* KEY_STATE_STOP     = "luaTests.stateStop";

private:
    Rml::DataModelHandle m_model;
    float       m_coinLeft      = 600.0f;
    float       m_coinTop       = 0.0f;
    std::string m_coinDirection = "1";   // bound to <select>; "1" = Right, "-1" = Left
    int         m_stateStop     = 0;

    AppStateBinding m_coinXBinding;
    AppStateBinding m_coinYBinding;

    uint32_t m_tickId = 0;

    // Programmatically set m_stateStop and dirty the model without triggering
    // the onStateStopChange callback double-toggle (the callback reads the actual
    // element state and returns early if it matches m_stateStop already).
    void setStateStop(int Value);
};
