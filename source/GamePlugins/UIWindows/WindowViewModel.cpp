#include <WindowViewModel.h>
#include <Layers/RmlUILayer.h>
#include <Layers/UDEDataLayer.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <utility>

WindowViewModel::WindowViewModel(std::string WindowTag)
    : m_windowTag(std::move(WindowTag))
{
    if (LoadRuntimeState())
    {
        // Seed the transient store with the loaded values before bindings are
        // active so the keys exist when setupTransientBindings() subscribes to them.
        SaveRuntimeState();
        // Bindings established last — the SaveRuntimeState() above must not fire
        // callbacks before the RmlUi model is ready.
        setupTransientBindings();
    }
}

std::string WindowViewModel::buildRuntimePrefix(const std::string& DocSlug, const std::string& WindowTag)
{
    if (DocSlug.empty() || WindowTag.empty())
        return {};
    return std::format("uiRuntime.{}.{}", DocSlug, WindowTag);
}

std::string WindowViewModel::docRuntimePrefix() const
{
    return m_activeDocSlug.empty() ? std::string{} : std::format("uiRuntime.{}", m_activeDocSlug);
}

std::string WindowViewModel::currentWinZMaxKey() const
{
    const auto Prefix = docRuntimePrefix();
    return Prefix.empty() ? std::string{} : Prefix + ".currentWinZMax";
}

bool WindowViewModel::loadValue(const std::string& Key, float& OutValue) const
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer || Key.empty()) return false;
    const auto* Val = DataLayer->Transient.Get(Key);
    if (!Val) return false;
    if (const auto* F = std::get_if<float>(Val)) { OutValue = *F; return true; }
    if (const auto* I = std::get_if<int>(Val))   { OutValue = static_cast<float>(*I); return true; }
    return false;
}

bool WindowViewModel::loadValue(const std::string& Key, int& OutValue) const
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer || Key.empty()) return false;
    const auto* Val = DataLayer->Transient.Get(Key);
    if (!Val) return false;
    if (const auto* I = std::get_if<int>(Val))   { OutValue = *I; return true; }
    if (const auto* F = std::get_if<float>(Val)) { OutValue = static_cast<int>(std::lround(*F)); return true; }
    return false;
}

int WindowViewModel::allocateNextZOrder()
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer) return m_windowZ > 0 ? m_windowZ : 1;

    const auto Key = currentWinZMaxKey();
    int CurrentMax = 0;
    if (!Key.empty())
        loadValue(Key, CurrentMax);

    const int Next = CurrentMax + 1;
    if (!Key.empty())
        DataLayer->Transient.Set(Key, AppStateValue{Next});
    return Next;
}

void WindowViewModel::syncCurrentWinZMax(int ZOrder)
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer) return;

    const auto Key = currentWinZMaxKey();
    if (Key.empty()) return;

    int CurrentMax = 0;
    if (!loadValue(Key, CurrentMax) || ZOrder > CurrentMax)
        DataLayer->Transient.Set(Key, AppStateValue{ZOrder});
}

bool WindowViewModel::LoadRuntimeState()
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer) return false;

    const auto* ActiveDocValue = DataLayer->Transient.Get("uiRuntime.activeDoc");
    if (!ActiveDocValue || !std::holds_alternative<std::string>(*ActiveDocValue))
        return false;

    m_activeDocSlug = RmlUILayer::NormalizeDocumentSlug(std::get<std::string>(*ActiveDocValue));
    if (m_activeDocSlug.empty())
        return false;

    m_runtimePrefix = buildRuntimePrefix(m_activeDocSlug, m_windowTag);
    if (m_runtimePrefix.empty())
        return false;

    const auto LeftKey      = m_runtimePrefix + ".posX";
    const auto TopKey       = m_runtimePrefix + ".posY";
    const auto ZKey         = m_runtimePrefix + ".z";
    const auto VisibleKey   = m_runtimePrefix + ".visible";

    loadValue(LeftKey, m_windowLeft);
    loadValue(TopKey,  m_windowTop);

    if (!loadValue(ZKey, m_windowZ))
        m_windowZ = allocateNextZOrder();
    else
        syncCurrentWinZMax(m_windowZ);

    int VisibleValue = 0;
    if (loadValue(VisibleKey, VisibleValue))
        m_visible = (VisibleValue != 0);


    return true;
}

void WindowViewModel::setupTransientBindings()
{
    if (m_runtimePrefix.empty()) return;

    const std::string PosXKey    = m_runtimePrefix + ".posX";
    const std::string PosYKey    = m_runtimePrefix + ".posY";
    const std::string ZKey       = m_runtimePrefix + ".z";
    const std::string VisibleKey = m_runtimePrefix + ".visible";

    m_posXBinding = APPSTATE_BIND_TRANSIENT(PosXKey.c_str(), 0.0f,
        [this](const std::string&, const AppStateValue& V)
        {
            if (const auto* F = std::get_if<float>(&V))       { m_windowLeft = *F; OnRuntimeStateChanged(); }
            else if (const auto* I = std::get_if<int>(&V))    { m_windowLeft = static_cast<float>(*I); OnRuntimeStateChanged(); }
        });

    m_posYBinding = APPSTATE_BIND_TRANSIENT(PosYKey.c_str(), 0.0f,
        [this](const std::string&, const AppStateValue& V)
        {
            if (const auto* F = std::get_if<float>(&V))       { m_windowTop = *F; OnRuntimeStateChanged(); }
            else if (const auto* I = std::get_if<int>(&V))    { m_windowTop = static_cast<float>(*I); OnRuntimeStateChanged(); }
        });

    m_zBinding = APPSTATE_BIND_TRANSIENT(ZKey.c_str(), 0,
        [this](const std::string&, const AppStateValue& V)
        {
            if (const auto* I = std::get_if<int>(&V))         { m_windowZ = *I; syncCurrentWinZMax(*I); OnRuntimeStateChanged(); }
        });

    m_visibleBinding = APPSTATE_BIND_TRANSIENT(VisibleKey.c_str(), 0,
        [this](const std::string&, const AppStateValue& V)
        {
            if (const auto* I = std::get_if<int>(&V))         { m_visible = (*I != 0); OnRuntimeStateChanged(); }
        });
}

void WindowViewModel::SaveRuntimeState()
{
    // Writes all four properties to transient.  Once bindings are active each
    // write fires its callback (member update + OnRuntimeStateChanged).  Before
    // bindings are set up (initial bootstrap in LoadRuntimeState) the writes
    // simply seed the transient store with no side-effects.
    auto* DataLayer = GetDataLayer();
    if (!DataLayer || m_runtimePrefix.empty()) return;

    DataLayer->Transient.Set(m_runtimePrefix + ".posX",    AppStateValue{m_windowLeft});
    DataLayer->Transient.Set(m_runtimePrefix + ".posY",    AppStateValue{m_windowTop});
    DataLayer->Transient.Set(m_runtimePrefix + ".z",       AppStateValue{m_windowZ});
    DataLayer->Transient.Set(m_runtimePrefix + ".visible", AppStateValue{m_visible ? 1 : 0});
    syncCurrentWinZMax(m_windowZ);
}

void WindowViewModel::setTransient(const char* Suffix, AppStateValue Value)
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer || m_runtimePrefix.empty()) return;
    DataLayer->Transient.Set(m_runtimePrefix + Suffix, std::move(Value));
}

void WindowViewModel::BindWindowStateToModel(Rml::DataModelHandle& Model,
                                             std::initializer_list<const char*> DirtyVariables)
{
    // Dirty all listed variables so RmlUi picks up restored transient state
    // on the first frame after RegisterWith().
    for (const char* Var : DirtyVariables)
        Model.DirtyVariable(Var);
}

void WindowViewModel::BindCommonWindowVars(Rml::DataModelConstructor& Ctor)
{
    Ctor.Bind("panel_left", &m_windowLeft);
    Ctor.Bind("panel_top", &m_windowTop);
    Ctor.Bind("panel_z", &m_windowZ);
    Ctor.Bind("panel_visible", &m_visible);
}

void WindowViewModel::BindCloseEvent(Rml::DataModelConstructor& Ctor,
                                     const char* EventName)
{
    Ctor.BindEventCallback(EventName,
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            Hide();
        });
}

void WindowViewModel::DirtyCommonWindowVars(Rml::DataModelHandle& Model) const
{
    if (!Context()) return;
    Model.DirtyVariable("panel_left");
    Model.DirtyVariable("panel_top");
    Model.DirtyVariable("panel_z");
    Model.DirtyVariable("panel_visible");
}

void WindowViewModel::SyncWindowModel(Rml::DataModelHandle& Model,
                                      std::initializer_list<const char*> ExtraDirtyVars)
{
    m_windowModel = Model;
    DirtyCommonWindowVars(Model);
    for (const char* Var : ExtraDirtyVars)
        Model.DirtyVariable(Var);
}

void WindowViewModel::OnRuntimeStateChanged()
{
    DirtyCommonWindowVars(m_windowModel);
}

bool WindowViewModel::HandleWindowDragEvent(SDL_Event& Event, const char* DragHandleId)
{
    if (Event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && Event.button.button == SDL_BUTTON_LEFT)
    {
        if (auto* Handle = FindElementInContext(Context(), DragHandleId))
        {
            const float Mx = Event.button.x, My = Event.button.y;
            const float L  = Handle->GetAbsoluteLeft();
            const float T  = Handle->GetAbsoluteTop();
            const float R  = L + Handle->GetClientWidth();
            const float B  = T + Handle->GetClientHeight();

            if (Mx >= L && Mx <= R && My >= T && My <= B)
            {
                RaiseWindowToFront();
                m_drag = { .active = true,
                           .offsetX = Mx - m_windowLeft,
                           .offsetY = My - m_windowTop };
                return true;
            }
        }
        return false;
    }

    if (Event.type == SDL_EVENT_MOUSE_BUTTON_UP && Event.button.button == SDL_BUTTON_LEFT)
    {
        if (m_drag.active)
        {
            m_drag.active = false;
            return true;
        }
    }

    if (Event.type == SDL_EVENT_MOUSE_MOTION && m_drag.active)
    {
        SetPosition(Event.motion.x - m_drag.offsetX,
                    Event.motion.y - m_drag.offsetY);
        return true;
    }

    return false;
}

void WindowViewModel::SetPosition(float Left, float Top)
{
    setTransient(".posX", AppStateValue{Left});
    setTransient(".posY", AppStateValue{Top});
}

void WindowViewModel::SetLeft(float Left)
{
    setTransient(".posX", AppStateValue{Left});
}

void WindowViewModel::SetTop(float Top)
{
    setTransient(".posY", AppStateValue{Top});
}

void WindowViewModel::SetZOrder(int ZOrder)
{
    setTransient(".z", AppStateValue{ZOrder});
}

void WindowViewModel::RaiseWindowToFront()
{
    // allocateNextZOrder() increments currentWinZMax and returns the new value.
    // Writing it to transient fires the z binding which updates m_windowZ,
    // calls syncCurrentWinZMax, and calls OnRuntimeStateChanged().
    setTransient(".z", AppStateValue{allocateNextZOrder()});
}

void WindowViewModel::Show()
{
    setTransient(".visible", AppStateValue{1});
}

void WindowViewModel::Hide()
{
    setTransient(".visible", AppStateValue{0});
}

void WindowViewModel::ClampToViewport(float CanvasWidth, float CanvasHeight,
                                     float WindowWidth, float WindowHeight,
                                     float MinVisibleWidth, float TitlebarHeight)
{
    if (CanvasWidth <= 0.0f || CanvasHeight <= 0.0f)
        return;

    const float VisibleWidth  = std::max(1.0f, MinVisibleWidth);
    const float VisibleHeight = std::max(1.0f, TitlebarHeight);

    const float MinLeft = -std::max(0.0f, WindowWidth  - VisibleWidth);
    const float MaxLeft =  std::max(0.0f, CanvasWidth  - VisibleWidth);
    const float MinTop  = -std::max(0.0f, WindowHeight - VisibleHeight);
    const float MaxTop  =  std::max(0.0f, CanvasHeight - VisibleHeight);

    // Compute clamped values from current members, then write to transient.
    // Binding callbacks update m_windowLeft/m_windowTop and dirty the model.
    setTransient(".posX", AppStateValue{std::clamp(m_windowLeft, MinLeft, MaxLeft)});
    setTransient(".posY", AppStateValue{std::clamp(m_windowTop,  MinTop,  MaxTop)});
}
