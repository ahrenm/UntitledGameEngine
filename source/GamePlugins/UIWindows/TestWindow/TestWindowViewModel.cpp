#include "TestWindowViewModel.h"

TestWindowViewModel::TestWindowViewModel()
    : WindowViewModel("test-window")
{
    // Show by default for this test window. This writes the runtime-backed
    // key and lets WindowViewModel bindings update local state.
    setTransient(".visible", AppStateValue{1});
}

void TestWindowViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
{
    SetContext(Context);

    auto Ctor = Context->CreateDataModel(ModelName);
    BindCommonWindowVars(Ctor);
    Ctor.Bind("window_message", &m_message);

    BindCloseEvent(Ctor);

    auto Model = Ctor.GetModelHandle();
    SyncWindowModel(Model, {"window_message"});
}


bool TestWindowViewModel::HandleEvent(SDL_Event& Event)
{
    return HandleWindowDragEvent(Event, "test-window-drag-handle");
}


