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