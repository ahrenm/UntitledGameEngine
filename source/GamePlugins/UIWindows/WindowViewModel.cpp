#include <WindowViewModel.h>
#include <Layers/RmlUILayer.h>
#include <Layers/UGEDataLayer.h>

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
    const auto* Val = DataLayer->Store.Get(Key);
    if (!Val) return false;
    if (const auto* F = Val->TryAs<float>()) { OutValue = *F; return true; }
    if (const auto* I = Val->TryAs<int>())   { OutValue = static_cast<float>(*I); return true; }
    return false;
}

bool WindowViewModel::loadValue(const std::string& Key, int& OutValue) const
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer || Key.empty()) return false;
    const auto* Val = DataLayer->Store.Get(Key);
    if (!Val) return false;
    if (const auto* I = Val->TryAs<int>())   { OutValue = *I; return true; }
    if (const auto* F = Val->TryAs<float>()) { OutValue = static_cast<int>(std::lround(*F)); return true; }
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
        DataLayer->Store.Set(Key, DataValue{Next});
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
        DataLayer->Store.Set(Key, DataValue{ZOrder});
}

bool WindowViewModel::LoadRuntimeState()
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer) return false;

    const auto* ActiveDocValue = DataLayer->Store.Get("uiRuntime.activeDoc");
    const std::string* ActiveDocStr = ActiveDocValue ? ActiveDocValue->TryAs<std::string>() : nullptr;
    if (!ActiveDocStr)
        return false;

    m_activeDocSlug = RmlUILayer::NormalizeDocumentSlug(*ActiveDocStr);
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

    m_posXBinding = DATA_BIND(PosXKey.c_str(), 0.0f,
        [this](const Tag&, const DataValue& V)
        {
            if (const auto* F = V.TryAs<float>())       { m_windowLeft = *F; OnRuntimeStateChanged(); }
            else if (const auto* I = V.TryAs<int>())    { m_windowLeft = static_cast<float>(*I); OnRuntimeStateChanged(); }
        }, Transient);

    m_posYBinding = DATA_BIND(PosYKey.c_str(), 0.0f,
        [this](const Tag&, const DataValue& V)
        {
            if (const auto* F = V.TryAs<float>())       { m_windowTop = *F; OnRuntimeStateChanged(); }
            else if (const auto* I = V.TryAs<int>())    { m_windowTop = static_cast<float>(*I); OnRuntimeStateChanged(); }
        }, Transient);

    m_zBinding = DATA_BIND(ZKey.c_str(), 0,
        [this](const Tag&, const DataValue& V)
        {
            if (const auto* I = V.TryAs<int>())         { m_windowZ = *I; syncCurrentWinZMax(*I); OnRuntimeStateChanged(); }
        }, Transient);

    m_visibleBinding = DATA_BIND(VisibleKey.c_str(), 0,
        [this](const Tag&, const DataValue& V)
        {
            if (const auto* I = V.TryAs<int>())         { m_visible = (*I != 0); OnRuntimeStateChanged(); }
        }, Transient);
}

void WindowViewModel::SaveRuntimeState()
{
    // Writes all four properties to transient.  Once bindings are active each
    // write fires its callback (member update + OnRuntimeStateChanged).  Before
    // bindings are set up (initial bootstrap in LoadRuntimeState) the writes
    // simply seed the transient store with no side-effects.
    auto* DataLayer = GetDataLayer();
    if (!DataLayer || m_runtimePrefix.empty()) return;

    DataLayer->Store.Set(m_runtimePrefix + ".posX",    DataValue{m_windowLeft});
    DataLayer->Store.Set(m_runtimePrefix + ".posY",    DataValue{m_windowTop});
    DataLayer->Store.Set(m_runtimePrefix + ".z",       DataValue{m_windowZ});
    DataLayer->Store.Set(m_runtimePrefix + ".visible", DataValue{m_visible ? 1 : 0});
    syncCurrentWinZMax(m_windowZ);
}

void WindowViewModel::setTransient(const char* Suffix, DataValue Value)
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer || m_runtimePrefix.empty()) return;
    DataLayer->Store.Set(m_runtimePrefix + Suffix, std::move(Value));
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
    setTransient(".posX", DataValue{Left});
    setTransient(".posY", DataValue{Top});
}

void WindowViewModel::SetLeft(float Left)
{
    setTransient(".posX", DataValue{Left});
}

void WindowViewModel::SetTop(float Top)
{
    setTransient(".posY", DataValue{Top});
}

void WindowViewModel::SetZOrder(int ZOrder)
{
    setTransient(".z", DataValue{ZOrder});
}

void WindowViewModel::RaiseWindowToFront()
{
    // allocateNextZOrder() increments currentWinZMax and returns the new value.
    // Writing it to transient fires the z binding which updates m_windowZ,
    // calls syncCurrentWinZMax, and calls OnRuntimeStateChanged().
    setTransient(".z", DataValue{allocateNextZOrder()});
}

void WindowViewModel::Show()
{
    setTransient(".visible", DataValue{1});
}

void WindowViewModel::Hide()
{
    setTransient(".visible", DataValue{0});
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
    setTransient(".posX", DataValue{std::clamp(m_windowLeft, MinLeft, MaxLeft)});
    setTransient(".posY", DataValue{std::clamp(m_windowTop,  MinTop,  MaxTop)});
}
