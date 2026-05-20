#pragma once
 #include <GameClasses/ViewModel.h>
#include <Layers/UGEDataLayer.h>

#include <initializer_list>
#include <string>
#include <string_view>

// ── WindowViewModel ────────────────────────────────────────────────────────────
// Shared base for reusable RmlUi window-like ViewModels.
//
// Runtime contract:
//   - RmlUILayer writes uiRuntime.activeDoc to the current page slug.
//   - WindowViewModel loads/stores runtime state under:
//       uiRuntime.<activeDoc>.<windowTag>.posX
//       uiRuntime.<activeDoc>.<windowTag>.posY
//       uiRuntime.<activeDoc>.<windowTag>.z
//       uiRuntime.<activeDoc>.<windowTag>.visible
//     plus uiRuntime.<activeDoc>.currentWinZMax
//
// Usage in derived RegisterWith():
//   1. Create the RmlUi data model and bind all variables (including m_visible).
//   2. Call BindWindowStateToModel(model, {"var1", "var2", ...}) to dirty initial
//      state AND subscribe to external writes of the visible key.
class WindowViewModel : public ViewModel
{
public:
    explicit WindowViewModel(std::string WindowTag);

    // Page/runtime identity.
    [[nodiscard]] const std::string& WindowTag() const { return m_windowTag; }
    [[nodiscard]] const std::string& ActiveDocumentSlug() const { return m_activeDocSlug; }
    [[nodiscard]] const std::string& RuntimePrefix() const { return m_runtimePrefix; }

    // Shared window state.
    [[nodiscard]] float Left() const { return m_windowLeft; }
    [[nodiscard]] float Top() const { return m_windowTop; }
    [[nodiscard]] int   ZOrder() const { return m_windowZ; }
    [[nodiscard]] bool  IsVisible() const { return m_visible; }

    void SetPosition(float Left, float Top);
    void SetLeft(float Left);
    void SetTop(float Top);
    void SetZOrder(int ZOrder);
    void RaiseWindowToFront();

    void Show();
    void Hide();

    // Public helpers for explicit state lifecycle control.
    // LoadRuntimeState() is also called by the constructor.
    [[nodiscard]] bool LoadRuntimeState();
    void SaveRuntimeState();

    // Call from RegisterWith() AFTER the RmlUi data model and all variable
    // bindings have been created.  Dirties the listed variables so RmlUi picks
    // up the restored transient state on the first frame.
    // The four transient subscriptions (posX/posY/z/visible) are set up
    // automatically in LoadRuntimeState() — no additional wiring is needed.
    void BindWindowStateToModel(Rml::DataModelHandle& Model,
                                std::initializer_list<const char*> DirtyVariables);

    // Option B clamping: keep the titlebar reachable while allowing the window
    // body to move partially offscreen.  Call after layout has settled so the
    // caller can provide the actual window dimensions in reference pixels.
    void ClampToViewport(float CanvasWidth, float CanvasHeight,
                         float WindowWidth, float WindowHeight,
                         float MinVisibleWidth = 80.0f,
                         float TitlebarHeight = 48.0f);

protected:
    [[nodiscard]] Rml::Element* FindElement(const Rml::String& Id) const
    {
        return FindElementInContext(Context(), Id);
    }

    // Binds panel_left/panel_top/panel_z/panel_visible to shared members.
    void BindCommonWindowVars(Rml::DataModelConstructor& Ctor);

    // Binds a close event callback that hides the window.
    // Default event name matches existing markup convention.
    void BindCloseEvent(Rml::DataModelConstructor& Ctor,
                        const char* EventName = "onClose");

    // Syncs common window vars + optional extras into the RmlUi model.
    void SyncWindowModel(Rml::DataModelHandle& Model,
                         std::initializer_list<const char*> ExtraDirtyVars = {});

    // Dirts only the shared panel_* variables.
    void DirtyCommonWindowVars(Rml::DataModelHandle& Model) const;

    // Shared drag behavior used by window-like view models.
    // Returns true if the event was consumed.
    bool HandleWindowDragEvent(SDL_Event& Event, const char* DragHandleId);

    // Writes a single value to the transient store under m_runtimePrefix + Suffix.
    // The corresponding binding callback then updates the local member and calls
    // OnRuntimeStateChanged() — callers must NOT assign the member directly.
    void setTransient(const char* Suffix, AppStateValue Value);

    // Optional hook called whenever runtime state (position, z-order, visibility) changes.
    // Derived classes should override to dirty relevant RmlUi model variables.
    virtual void OnRuntimeStateChanged();

private:
    [[nodiscard]] static std::string buildRuntimePrefix(const std::string& DocSlug,
                                                       const std::string& WindowTag);

    [[nodiscard]] bool loadValue(const std::string& Key, float& OutValue) const;
    [[nodiscard]] bool loadValue(const std::string& Key, int& OutValue) const;
    [[nodiscard]] std::string docRuntimePrefix() const;
    [[nodiscard]] std::string currentWinZMaxKey() const;
    [[nodiscard]] int allocateNextZOrder();
    void syncCurrentWinZMax(int ZOrder);

    // Sets up (or refreshes) the four transient bindings after m_runtimePrefix is known.
    // Called at the end of LoadRuntimeState() so the initial SaveRuntimeState() write
    // does not trigger callbacks before the RmlUi model is ready.
    void setupTransientBindings();

    std::string m_windowTag;
    std::string m_activeDocSlug;
    std::string m_runtimePrefix;
    Rml::DataModelHandle m_windowModel;

    // One binding per transient-backed property.
    // Callbacks call OnRuntimeStateChanged() so derived classes dirty their model vars.
    AppStateBinding m_posXBinding;
    AppStateBinding m_posYBinding;
    AppStateBinding m_zBinding;
    AppStateBinding m_visibleBinding;

protected:
    struct DragState {
        bool active = false;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
    } m_drag;

    float m_windowLeft = 0.0f;
    float m_windowTop  = 0.0f;
    int   m_windowZ    = 0;
    bool  m_visible    = false;
};

