#pragma once
#include "AppLayer.h"
#include "../IScriptableObject.h"
#include "../IEventHandler.h"
#include "../LayerRegistry.h"
#include <RmlUi/Core.h>
#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_SDL.h"
#include "../PhysFSFileInterface.h"
#include <SDL3/SDL.h>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ViewModel;

// ── RmlUILayer ────────────────────────────────────────────────────────────────
// Owns RmlUi initialisation, the SDL platform/render interfaces, and the context.
// Also owns the lifetime of all ViewModels instantiated during document loading.
//
// Only ONE page document may be live at a time.  Loading a new page automatically
// tears down the previous page, its fragments, and all associated ViewModels.
//
// On loadDocumentNow():
//   1. The page text is scanned for data-model attributes; ViewModels are
//      instantiated and RegisterWith() is called BEFORE RmlUi parses the markup.
//   2. After load, the DOM is walked for data-fragment attributes.
//   3. Each fragment is read from PhysFS, its data-model attributes are wired,
//      and the markup is injected via SetInnerRML.
//   4. All ViewModels (page-level and fragment-level) are owned by m_pageViewModels
//      and destroyed together when the page is unloaded.
//
// Load order: 6 — runs after LuaLayer and fetches SDL_Window* / SDL_Renderer* from SDLLayer.
class RmlUILayer : public AppLayer, public IScriptableObject, public IEventHandler
{
public:
    REGISTER_LAYER("rmlui", 6.0f, RmlUILayer)

    [[nodiscard]] static std::expected<std::unique_ptr<RmlUILayer>, std::string>
    Create();

    ~RmlUILayer() override;

    RmlUILayer(const RmlUILayer&)            = delete;
    RmlUILayer& operator=(const RmlUILayer&) = delete;

    // Load a font from the PhysFS virtual filesystem.
    static void LoadFont(const char* VirtualPath) { Rml::LoadFontFace(VirtualPath); }

    // Convert a virtual document path into the normalized slug used by uiRuntime.
    [[nodiscard]] static std::string NormalizeDocumentSlug(std::string_view VirtualPath);

    // Queue a page to be loaded on the next Update().  Any previously resident
    // page (including its fragments and ViewModels) is torn down first.
    // If called multiple times before Update(), only the last path is honoured.
    void LoadDocument(const char* VirtualPath);

    // Immediately unload the current page, its fragments, and all ViewModels.
    // No-op if no page is loaded.
    void UnloadPage();

    // Access the raw context (for data models, debugger, etc.)
    [[nodiscard]] Rml::Context* Context() const { return m_context; }

    // Active page slug currently written to uiRuntime.activeDoc.
    [[nodiscard]] const std::string& ActiveDocumentSlug() const { return m_activeDocumentSlug; }

    // Set the log sink. Immediately flushes any messages buffered before this call.
    void SetLogFunction(const std::function<void(Rml::String)>& Fn);

    // Per-frame calls
    void RmlUpdate();
    void BeginFrame();
    void RenderFrame();
    void EndFrame();
    void Frame(SDL_Renderer* Renderer, SDL_Texture* BgTexture);

    // Forward an SDL event to RmlUi
    void ProcessEvent(SDL_Window* Window, SDL_Event& Event);

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void Update() override;
    void Tick(float deltaTime) override;

    // ── IEventHandler ─────────────────────────────────────────────────────────
    // Dispatches to ViewModels in reverse order, then forwards to RmlUi.
    // Returns false so the event continues to SDLLayer (and its active scene).
    // Only returns true if a ViewModel explicitly consumed the event.
    bool HandleEvent(SDL_Event& Event) override;

    // ── IScriptableObject ─────────────────────────────────────────────────────
    // Registers the "UI" Lua table:
    //   UI.LoadFont(virtualPath: string)
    //   UI.LoadDocument(virtualPath: string) -> bool
    void RegisterObject(sol::state& Lua) override;

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void RegisterWithServiceLocator() override;

private:
    // Scan rml text for data-model="..." attribute values.
    static std::vector<std::string> extractDataModelNames(std::string_view RmlText);

    // Instantiate any registered ViewModel for ModelName, call RegisterWith(),
    // and append it to m_pageViewModels.
    void instantiateViewModel(std::string_view ModelName);

    // Walk the loaded document's DOM for data-fragment="..." attributes,
    // then scan each fragment for data-model attributes, wire ViewModels,
    // and inject the markup via SetInnerRML.
    void processFragments(Rml::ElementDocument* Doc);

    // Perform the actual page load, ViewModel wiring, and fragment injection.
    // Called from Update() when m_pendingPage has a value.
    void loadDocumentNow(const std::string& VirtualPath);

    // Destroy all page ViewModels and unload the current RmlUi document.
    // Safe to call when no page is loaded (no-op).
    void unloadCurrentPage();

    // ── Private constructor ───────────────────────────────────────────────────
    RmlUILayer(SDL_Window* Window, SDL_Renderer* Renderer);

    // ── Inner system interface ────────────────────────────────────────────────
    class AppSystemInterface : public SystemInterface_SDL
    {
    public:
        explicit AppSystemInterface(SDL_Window* Window) : SystemInterface_SDL(Window) {}

        std::function<void(Rml::String)> onLog;
        std::vector<Rml::String>         buffer;

        bool LogMessage(Rml::Log::Type Type, const Rml::String& Message) override;
    };

    PhysFSFileInterface  m_fileInterface;
    AppSystemInterface   m_systemInterface;
    RenderInterface_SDL  m_renderInterface;
    SDL_Window*          m_window       = nullptr;  // non-owning; lifetime is SDLLayer's
    Rml::Context*        m_context      = nullptr;
    Rml::ElementDocument* m_currentPage = nullptr;  // non-owning; context holds lifetime
    std::string          m_activeDocumentSlug;

    // Path queued via LoadDocument(); processed (and cleared) in Update().
    std::optional<std::string> m_pendingPage;

    // Owns all ViewModels for the current page (page-level + all fragments).
    // Cleared in unloadCurrentPage().
    std::vector<std::unique_ptr<ViewModel>> m_pageViewModels;

    // Parallel list of data-model names registered with m_context for the current
    // page. Used by unloadCurrentPage() to call RemoveDataModel() before the
    // ViewModels (and their member vectors) are destroyed, preventing dangling
    // pointers inside live DataViewFor / DataViews::Update calls.
    std::vector<std::string> m_pageModelNames;
};

