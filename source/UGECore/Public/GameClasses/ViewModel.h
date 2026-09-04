#pragma once
#include <GameClasses/GameObjectBase.h>
#include <ViewModelRegistry.h>
#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

// ── ViewModel ─────────────────────────────────────────────────────────────────
// Abstract base class for all RmlUi data-model view models.
// Subclasses self-register via REGISTER_VIEWMODEL so RmlUILayer can instantiate
// them automatically when a matching data-model attribute is found.
class ViewModel : public GameObjectBase
{
public:
    virtual ~ViewModel() = default;

    // Called by RmlUILayer immediately after instantiation, before the element
    // tree that references this model is parsed or injected.
    virtual void RegisterWith(Rml::Context* Context, const char* ModelName) = 0;

    // Optional event handler.  RmlUILayer iterates ViewModels in reverse order
    // and calls this before forwarding the event to RmlUi itself.
    // Return true  → event consumed; propagation stops.
    // Return false → event continues to the next handler (default).
    virtual bool HandleEvent(SDL_Event& /*Event*/) { return false; }

    // Optional hook called by RmlUILayer after Context::Update().
    // Useful when work must happen after document/layout changes are applied.
    virtual void PostRmlUpdate() {}

protected:
    void SetContext(Rml::Context* Context) { m_context = Context; }
    [[nodiscard]] Rml::Context* Context() const { return m_context; }

    // Binds a data-model event callback that loads the named scene via
    // SceneManagerLayer.  Collapses the common "onPrevious"/"onNext" navigation
    // boilerplate.
    void BindLoadScene(Rml::DataModelConstructor& Ctor,
                       const char* EventName, const char* SceneName)
    {
        Ctor.BindEventCallback(EventName,
            [this, SceneName](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
            {
                if (auto* SceneMgr = GetSceneManagerLayer())
                    SceneMgr->LoadScene(SceneName);
            });
    }

    // The RmlUi debugger may occupy document index 0, so scan all documents.
    static Rml::Element* FindElementInContext(Rml::Context* Context, const Rml::String& Id)
    {
        if (!Context) return nullptr;
        for (int i = 0; i < Context->GetNumDocuments(); ++i)
            if (auto* Doc = Context->GetDocument(i))
                if (auto* El = Doc->GetElementById(Id))
                    return El;
        return nullptr;
    }

    Rml::Context* m_context = nullptr;
};


// ── VM_BIND_MODEL_* ───────────────────────────────────────────────────────────
// ViewModel-scoped reactive bind helpers. Each one wraps the system DATA_BIND
// (available here via GameObjectBase → UGEDataLayer.h) to perform the exact
// pattern repeated across the page ViewModels:
//   1. subscribe to Key_ (seeding it with a default if absent),
//   2. mirror the stored value into a local Member_ on every change,
//   3. dirty the named RmlUi model variable Var_ so the view refreshes,
//   4. seed Member_ from the current stored value immediately.
//
// These stay ViewModel-owned rather than living in the system-wide DataLayer
// header, since dirtying an Rml model variable is a UI concern.
//
// Args:
//   Binding_ : DataBinding lvalue to assign into
//   Key_     : const char* / std::string tag
//   Member_  : local member to keep in sync
//   Model_   : Rml::DataModelHandle lvalue to dirty
//   Var_     : model variable name (const char*) to dirty on change
//   Target_  : optional (Persistent default / Transient)
#define VM_BIND_MODEL_STRING_IMPL(Binding_, Key_, Member_, Model_, Var_, Target_)  \
    do {                                                                            \
        (Binding_) = DATA_BIND_IMPL((Key_), std::string{},                         \
            [&](const Tag&, const DataValue& vmVal)                                \
            {                                                                       \
                if (const std::string* vmS = vmVal.TryAs<std::string>()) (Member_) = *vmS; \
                (Model_).DirtyVariable(Var_);                                       \
            }, Target_);                                                            \
        if (const DataValue* vmV = (Binding_).GetValue())                          \
            if (const std::string* vmS = vmV->TryAs<std::string>()) (Member_) = *vmS; \
    } while(0)

#define VM_BIND_MODEL_STRING_GET(_1, _2, _3, _4, _5, _6, NAME, ...) NAME
#define VM_BIND_MODEL_STRING(...) \
    VM_BIND_MODEL_STRING_GET(__VA_ARGS__, VM_BIND_MODEL_STRING_6, VM_BIND_MODEL_STRING_5)(__VA_ARGS__)
#define VM_BIND_MODEL_STRING_5(B_, K_, M_, Mdl_, V_) \
    VM_BIND_MODEL_STRING_IMPL(B_, K_, M_, Mdl_, V_, Persistent)
#define VM_BIND_MODEL_STRING_6(B_, K_, M_, Mdl_, V_, T_) \
    VM_BIND_MODEL_STRING_IMPL(B_, K_, M_, Mdl_, V_, T_)


#define VM_BIND_MODEL_INT_IMPL(Binding_, Key_, Member_, Model_, Var_, Target_)     \
    do {                                                                            \
        (Binding_) = DATA_BIND_IMPL((Key_), 0,                                     \
            [&](const Tag&, const DataValue& vmVal)                                \
            {                                                                       \
                if (const int* vmI = vmVal.TryAs<int>()) (Member_) = *vmI;         \
                (Model_).DirtyVariable(Var_);                                       \
            }, Target_);                                                            \
        if (const DataValue* vmV = (Binding_).GetValue())                          \
            if (const int* vmI = vmV->TryAs<int>()) (Member_) = *vmI;              \
    } while(0)

#define VM_BIND_MODEL_INT_GET(_1, _2, _3, _4, _5, _6, NAME, ...) NAME
#define VM_BIND_MODEL_INT(...) \
    VM_BIND_MODEL_INT_GET(__VA_ARGS__, VM_BIND_MODEL_INT_6, VM_BIND_MODEL_INT_5)(__VA_ARGS__)
#define VM_BIND_MODEL_INT_5(B_, K_, M_, Mdl_, V_) \
    VM_BIND_MODEL_INT_IMPL(B_, K_, M_, Mdl_, V_, Persistent)
#define VM_BIND_MODEL_INT_6(B_, K_, M_, Mdl_, V_, T_) \
    VM_BIND_MODEL_INT_IMPL(B_, K_, M_, Mdl_, V_, T_)


#define VM_BIND_MODEL_FLOAT_IMPL(Binding_, Key_, Member_, Model_, Var_, Target_)   \
    do {                                                                            \
        (Binding_) = DATA_BIND_IMPL((Key_), 0.0f,                                  \
            [&](const Tag&, const DataValue& vmVal)                                \
            {                                                                       \
                if (const float* vmF = vmVal.TryAs<float>()) (Member_) = *vmF;     \
                (Model_).DirtyVariable(Var_);                                       \
            }, Target_);                                                            \
        if (const DataValue* vmV = (Binding_).GetValue())                          \
            if (const float* vmF = vmV->TryAs<float>()) (Member_) = *vmF;          \
    } while(0)

#define VM_BIND_MODEL_FLOAT_GET(_1, _2, _3, _4, _5, _6, NAME, ...) NAME
#define VM_BIND_MODEL_FLOAT(...) \
    VM_BIND_MODEL_FLOAT_GET(__VA_ARGS__, VM_BIND_MODEL_FLOAT_6, VM_BIND_MODEL_FLOAT_5)(__VA_ARGS__)
#define VM_BIND_MODEL_FLOAT_5(B_, K_, M_, Mdl_, V_) \
    VM_BIND_MODEL_FLOAT_IMPL(B_, K_, M_, Mdl_, V_, Persistent)
#define VM_BIND_MODEL_FLOAT_6(B_, K_, M_, Mdl_, V_, T_) \
    VM_BIND_MODEL_FLOAT_IMPL(B_, K_, M_, Mdl_, V_, T_)
