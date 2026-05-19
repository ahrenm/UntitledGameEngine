#include "LuaConsoleViewModel.h"
#include <Layers/LuaLayer.h>
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>

LuaConsoleViewModel::LuaConsoleViewModel()
    : WindowViewModel("lua-console")
{
}

void LuaConsoleViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
{
    SetContext(Context);
    auto Ctor = Context->CreateDataModel(ModelName);
    // Register once: log_lines is a std::vector<std::string> iterated by data-for.
    static bool s_registered = false;
    if (!s_registered)
    {
        Ctor.RegisterArray<std::vector<std::string>>();
        s_registered = true;
    }
    BindCommonWindowVars(Ctor);
    Ctor.Bind("log_lines",     &m_logLines);
    Ctor.Bind("input_text",    &m_inputText);

    Ctor.BindEventCallback("onSubmit",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            submitInputText();
        });

    BindCloseEvent(Ctor);

    m_model = Ctor.GetModelHandle();
    // Dirty initial state; WindowViewModel already owns the transient bindings.
    SyncWindowModel(m_model, {"log_lines", "input_text"});

    // Self-wire the log buffer — LoggingLayer is always available at this point.
    auto& Log = ServiceLocator::Get<LoggingLayer>();
    Log.onLog = BindLogData(Log.Lines());

}

std::function<void(const std::string&)>
LuaConsoleViewModel::BindLogData(std::vector<std::string>& Lines)
{
    m_logLines = &Lines;
    m_model.DirtyVariable("log_lines");
    queueAutoScrollToBottom();

    return [this](const std::string&) {
        m_model.DirtyVariable("log_lines");
        queueAutoScrollToBottom();
    };
}

void LuaConsoleViewModel::queueAutoScrollToBottom()
{
    // Keep two frames queued so both initial load and data-for updates settle.
    if (m_pendingAutoScrollFrames < 2)
        m_pendingAutoScrollFrames = 2;
}

void LuaConsoleViewModel::PostRmlUpdate()
{
    if (m_pendingAutoScrollFrames <= 0) return;

    auto* El = FindElementInContext(Context(), "log-content");
    if (!El) return;

    El->SetScrollTop(El->GetScrollHeight());
    --m_pendingAutoScrollFrames;
}


bool LuaConsoleViewModel::isInputTextFocused() const
{
    auto* Ctx = Context();
    if (!Ctx) return false;
    auto* Focused = Ctx->GetFocusElement();
    if (!Focused) return false;

    const auto* Attr = Focused->GetAttribute("data-value");
    if (!Attr) return false;

    return Attr->Get<Rml::String>() == INPUT_TEXT_DATA_KEY;
}

void LuaConsoleViewModel::submitInputText()
{
    if (m_inputText.empty()) return;

    ServiceLocator::Get<LoggingLayer>().Log("> " + m_inputText);
    std::ignore = ServiceLocator::Get<LuaLayer>().Execute(m_inputText);
    m_inputText.clear();
    m_model.DirtyVariable("input_text");
}


// ── HandleEvent ───────────────────────────────────────────────────────────────
bool LuaConsoleViewModel::HandleEvent(SDL_Event& Event)
{
    if (Event.type == SDL_EVENT_KEY_DOWN)
    {
        const auto Scancode = Event.key.scancode;

        // Toggle visibility via the runtime-backed transient key.
        // Do not mutate m_visible directly; bindings propagate the change.
        if (Scancode == SDL_SCANCODE_GRAVE)
        {
            setTransient(".visible", AppStateValue{IsVisible() ? 0 : 1});
            return true;
        }

        if ((Scancode == SDL_SCANCODE_RETURN || Scancode == SDL_SCANCODE_KP_ENTER)
            && isInputTextFocused())
        {
            submitInputText();
            return true;
        }
    }

    if (HandleWindowDragEvent(Event, "drag-handle"))
        return true;

    return false;
}
