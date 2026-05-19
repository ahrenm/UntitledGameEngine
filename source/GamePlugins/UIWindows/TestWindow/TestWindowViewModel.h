#pragma once

#include <WindowViewModel.h>
#include <ViewModelRegistry.h>

#include <string>

class TestWindowViewModel : public WindowViewModel
{
public:
    REGISTER_VIEWMODEL("test-window", TestWindowViewModel)

    TestWindowViewModel();

    void RegisterWith(Rml::Context* Context, const char* ModelName) override;
    bool HandleEvent(SDL_Event& Event) override;

protected:

private:
    std::string m_message = "This is a test window fragment.";
};

