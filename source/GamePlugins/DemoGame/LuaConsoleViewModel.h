#pragma once
#include <WindowViewModel.h>
#include <ViewModelRegistry.h>
#include <functional>
#include <vector>
#include <string>

// ── LuaConsoleViewModel ───────────────────────────────────────────────────────
// Owns all data that the UI panel reads and writes.
// Call RegisterWith() once, then use the typed mutators — the RmlUi model
// is dirtied automatically so the view stays in sync.
//
// Log lines are owned by LoggingLayer. After RegisterWith(), call BindLogData()
// with a reference to LoggingLayer::Lines() and assign the returned callback to
// LoggingLayer::onLog so the RmlUi model is dirtied on every new log entry.


class LuaConsoleViewModel : public WindowViewModel
{
public:
    REGISTER_VIEWMODEL("lua-console", LuaConsoleViewModel)

    LuaConsoleViewModel();

    // Register all variables with a new RmlUi data model on the given context.
    void RegisterWith(Rml::Context* Context, const char* ModelName) override;

    // Called after RegisterWith(). Returns an onLog callback that, when invoked,
    // pushes each line into the data model and dirties it.
    [[nodiscard]] std::function<void(const std::string&)>
    BindLogData(std::vector<std::string>& Lines);

    // ── IEventHandler (via ViewModel) ─────────────────────────────────────────
    // Handles keyboard submit/toggle and mouse drag for the panel.
    // Returns true when consumed so the event is not forwarded further.
    bool HandleEvent(SDL_Event& Event) override;
    void PostRmlUpdate() override;

    [[nodiscard]] const std::string& InputText() const { return m_inputText; }


private:
    static constexpr const char* INPUT_TEXT_DATA_KEY = "input_text";

    std::vector<std::string>* m_logLines  = nullptr;
    std::string            m_inputText;

    Rml::DataModelHandle   m_model;


    [[nodiscard]] bool isInputTextFocused() const;
    void submitInputText();
    void queueAutoScrollToBottom();


    int m_pendingAutoScrollFrames = 0;
};
