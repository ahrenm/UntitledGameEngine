#include <Layers/RmlUILayer.h>
#include <ServiceLocator.h>
#include <Layers/PhysFSLayer.h>
#include <Layers/LoggingLayer.h>
#include <Layers/UDEDataLayer.h>
#include <Layers/SDLLayer.h>
#include <GameClasses/ViewModel.h>
#include <ViewModelRegistry.h>
#include <RmlUi/Debugger.h>
#include <sol/sol.hpp>
#include <expected>
#include <memory>
#include <ranges>
#include <cctype>

// ── AppSystemInterface::LogMessage ────────────────────────────────────────────
bool RmlUILayer::AppSystemInterface::LogMessage(Rml::Log::Type Type, const Rml::String& Message)
{
    Rml::String Tag;
    switch (Type) {
        case Rml::Log::LT_ERROR:   Tag = "[RML ERROR] "; break;
        case Rml::Log::LT_WARNING: Tag = "[RML WARN]  "; break;
        case Rml::Log::LT_INFO:    Tag = "[RML INFO]  "; break;
        default:                   Tag = "[RML]       "; break;
    }
    Rml::String Entry = Tag + Message;

    if (onLog)
        onLog(std::move(Entry));
    else
        buffer.push_back(std::move(Entry));

    return true;
}

// ── Factory ───────────────────────────────────────────────────────────────────
std::expected<std::unique_ptr<RmlUILayer>, std::string>
RmlUILayer::Create()
{
    // Fetch SDLLayer from ServiceLocator to get window and renderer
    auto* SdlLayer = ServiceLocator::TryGet<SDLLayer>();
    if (!SdlLayer)
        return std::unexpected("RmlUILayer::Create: SDLLayer not found in ServiceLocator");

    SDL_Window* Window = SdlLayer->Window();
    SDL_Renderer* Renderer = SdlLayer->Renderer();

    if (!Window || !Renderer)
        return std::unexpected("RmlUILayer::Create: SDLLayer window or renderer is null");

    auto Layer = std::unique_ptr<RmlUILayer>(new RmlUILayer(Window, Renderer));

    if (auto* Log = ServiceLocator::TryGet<LoggingLayer>())
        Layer->SetLogFunction(Log->MakeSink());

    return Layer;
}

// ── RmlUILayer ────────────────────────────────────────────────────────────────
RmlUILayer::RmlUILayer(SDL_Window* Window, SDL_Renderer* Renderer)
    : m_systemInterface(Window)
    , m_renderInterface(Renderer)
    , m_window(Window)
{
    Rml::SetFileInterface(&m_fileInterface);
    Rml::SetRenderInterface(&m_renderInterface);
    Rml::SetSystemInterface(&m_systemInterface);
    Rml::Initialise();

    int W = 0, H = 0;
    SDL_GetWindowSize(Window, &W, &H);
    m_context = Rml::CreateContext("main", Rml::Vector2i(W, H));
    Rml::Debugger::Initialise(m_context);
}

RmlUILayer::~RmlUILayer()
{
    // Destroy all ViewModels before shutting down RmlUi so their destructors
    // can still access a live context if needed.
    m_pageViewModels.clear();
    Rml::Shutdown();
}

std::string RmlUILayer::NormalizeDocumentSlug(std::string_view VirtualPath)
{
    const auto SlashPos = VirtualPath.find_last_of("/\\");
    const std::string_view FileName = (SlashPos == std::string_view::npos)
        ? VirtualPath
        : VirtualPath.substr(SlashPos + 1);

    const auto DotPos = FileName.find_last_of('.');
    const std::string_view Stem = (DotPos == std::string_view::npos)
        ? FileName
        : FileName.substr(0, DotPos);

    std::string Slug;
    Slug.reserve(Stem.size());
    bool PendingUnderscore = false;
    for (const unsigned char Ch : Stem)
    {
        if (std::isalnum(Ch))
        {
            if (PendingUnderscore && !Slug.empty())
                Slug.push_back('_');
            Slug.push_back(static_cast<char>(std::tolower(Ch)));
            PendingUnderscore = false;
        }
        else if (!Slug.empty())
        {
            PendingUnderscore = true;
        }
    }

    while (!Slug.empty() && Slug.back() == '_')
        Slug.pop_back();
    if (!Slug.empty() && Slug.front() == '_')
        Slug.erase(Slug.begin());

    return Slug;
}

// ── ViewModel scanning and instantiation ──────────────────────────────────────

std::vector<std::string> RmlUILayer::extractDataModelNames(std::string_view RmlText)
{
    std::vector<std::string> Names;
    constexpr std::string_view Token = "data-model=\"";
    size_t Pos = 0;
    while ((Pos = RmlText.find(Token, Pos)) != std::string_view::npos) {
        Pos += Token.size();
        const auto End = RmlText.find('"', Pos);
        if (End == std::string_view::npos) break;
        Names.emplace_back(RmlText.substr(Pos, End - Pos));
        Pos = End + 1;
    }
    return Names;
}

void RmlUILayer::instantiateViewModel(std::string_view ModelName)
{
    auto Vm = ViewModelRegistry::Instance().Create(ModelName);
    if (!Vm) return; // no registered factory — model name has no ViewModel

    Vm->RegisterWith(m_context, std::string(ModelName).c_str());
    m_pageModelNames.emplace_back(ModelName);
    m_pageViewModels.push_back(std::move(Vm));
}

void RmlUILayer::processFragments(Rml::ElementDocument* Doc)
{
    auto& Fs = ServiceLocator::Get<PhysFSLayer>();

    Rml::ElementList Containers;
    Doc->QuerySelectorAll(Containers, "[data-fragment]");

    for (auto* El : Containers) {
        const auto* Attr = El->GetAttribute("data-fragment");
        if (!Attr) continue;
        const auto FragPath = Attr->Get<Rml::String>();

        // Read fragment text so we can scan for data-model before injecting.
        const auto Bytes = Fs.ReadFile(FragPath.c_str());
        if (Bytes.empty()) {
            if (auto* Log = ServiceLocator::TryGet<LoggingLayer>())
                Log->Log("[RmlUILayer] Fragment not found: " + FragPath);
            continue;
        }

        const std::string_view FragText(reinterpret_cast<const char*>(Bytes.data()), Bytes.size());

        // Register ViewModels for any data-model attributes in the fragment
        // BEFORE the markup is injected so RmlUi can resolve them immediately.
        for (const auto& dataModelsToLoad : extractDataModelNames(FragText))
            instantiateViewModel(dataModelsToLoad);

        El->SetInnerRML(Rml::String(FragText));
    }
}

// ── unloadCurrentPage ─────────────────────────────────────────────────────────
void RmlUILayer::unloadCurrentPage()
{
    if (!m_currentPage) return;

    // Remove every data model from the context BEFORE destroying ViewModels.
    // RmlUi's Context::LoadDocument() calls Context::Update() internally, which
    // would iterate live DataViews that hold raw pointers into ViewModel member
    // data (e.g. the std::vector* inside a data-for binding).  If the ViewModel
    // is already destroyed those pointers dangle — resulting in a segfault inside
    // ArrayDefinition::Size.  RemoveDataModel severs the context's reference to
    // those views so no dangling access can occur.
    for (const auto& Name : m_pageModelNames)
        m_context->RemoveDataModel(Name);
    m_pageModelNames.clear();

    // Now safe to destroy ViewModels — context no longer references their data.
    m_pageViewModels.clear();
    m_context->UnloadDocument(m_currentPage);
    m_currentPage = nullptr;
}

// ── LoadDocument (public) ─────────────────────────────────────────────────────
// Records the requested path; the actual load (and any teardown) is deferred
// to Update().  If called multiple times before Update() fires, last write wins.
void RmlUILayer::LoadDocument(const char* VirtualPath)
{
    m_pendingPage = VirtualPath;
}

// ── UnloadPage (public) ───────────────────────────────────────────────────────
void RmlUILayer::UnloadPage()
{
    m_pendingPage.reset(); // cancel any queued load
    unloadCurrentPage();
}

// ── loadDocumentNow (private) ─────────────────────────────────────────────────
void RmlUILayer::loadDocumentNow(const std::string& VirtualPath)
{
    auto& Fs = ServiceLocator::Get<PhysFSLayer>();
    auto* DataLayer = ServiceLocator::TryGet<UDEDataLayer>();

    m_activeDocumentSlug = NormalizeDocumentSlug(VirtualPath);
    if (DataLayer && !m_activeDocumentSlug.empty())
    {
        DataLayer->Transient.Set("uiRuntime.activeDoc", AppStateValue{m_activeDocumentSlug});
        const std::string CurrentMaxKey = "uiRuntime." + m_activeDocumentSlug + ".currentWinZMax";
        if (!DataLayer->Transient.Has(CurrentMaxKey))
            DataLayer->Transient.Set(CurrentMaxKey, AppStateValue{0});
    }


    // Read the file as text to extract inline data-model names.
    // ViewModels must be registered BEFORE RmlUi parses the markup.
    const auto pageBytes = Fs.ReadFile(VirtualPath.c_str());
    const auto dataModelsToLoad = extractDataModelNames(
        std::string_view(reinterpret_cast<const char*>(pageBytes.data()), pageBytes.size()));

    for (const auto& Name : dataModelsToLoad)
        instantiateViewModel(Name);

    auto* Doc = m_context->LoadDocument(VirtualPath);
    if (!Doc) {
        if (auto* Log = ServiceLocator::TryGet<LoggingLayer>())
            Log->Log("[RmlUILayer] loadDocumentNow failed: " + VirtualPath);
        // Roll back any ViewModels registered for this failed load.
        m_pageViewModels.clear();
        return;
    }

    m_currentPage = Doc;

    // Inject data-fragment (page fragments) elements and wire their ViewModels.
    processFragments(m_currentPage);

    m_currentPage->Show();
}

// ── SetLogFunction ─────────────────────────────────────────────────────────────
void RmlUILayer::SetLogFunction(const std::function<void(Rml::String)>& Fn)
{
    m_systemInterface.onLog = Fn;
    for (auto& Msg : m_systemInterface.buffer)
        Fn(std::move(Msg));
    m_systemInterface.buffer.clear();
}

void RmlUILayer::RmlUpdate()   { m_context->Update();              }
void RmlUILayer::BeginFrame()
{
    // RenderInterface_SDL::BeginFrame() resets the viewport and blend mode but
    // also calls SDL_RenderClear, which would wipe the background already drawn
    // by SDLLayer::Tick().  We replicate only the two state-setup calls here.
    SDL_SetRenderViewport(SDL_GetRenderer(m_window), nullptr);
    SDL_SetRenderDrawBlendMode(SDL_GetRenderer(m_window), SDL_BLENDMODE_BLEND);
}
void RmlUILayer::RenderFrame() { m_context->Render();              }
void RmlUILayer::EndFrame()    { m_renderInterface.EndFrame();     }

void RmlUILayer::Frame(SDL_Renderer* Renderer, SDL_Texture* BgTexture)
{
    m_context->Update();
    m_renderInterface.BeginFrame();
    SDL_RenderTexture(Renderer, BgTexture, nullptr, nullptr);
    m_context->Render();
    m_renderInterface.EndFrame();
}

void RmlUILayer::ProcessEvent(SDL_Window* Window, SDL_Event& Event)
{
    RmlSDL::InputEventHandler(m_context, Window, Event);
}

// ── IEventHandler ─────────────────────────────────────────────────────────────
bool RmlUILayer::HandleEvent(SDL_Event& Event)
{
    // Give every ViewModel a chance to consume the event first (reverse order).
    for (auto& Vm : m_pageViewModels | std::views::reverse)
        if (Vm->HandleEvent(Event))
            return true;

    // RmlSDL::InputEventHandler returns true if the event is still propagating
    // (RmlUi did not handle it), and false if RmlUi consumed it.
    // Invert: return true (stop propagation) when RmlUi handled the event,
    // and false (continue propagating) when RmlUi left it unhandled.
    // This ensures that e.g. typing into a focused <input> field does not
    // leak keypresses through to the active scene.
    const bool StillPropagating = RmlSDL::InputEventHandler(m_context, m_window, Event);
    return !StillPropagating;
}

void RmlUILayer::Update()
{
    // If a page load was requested, tear down any current page then load the new one.
    if (m_pendingPage.has_value())
    {
        auto Path = std::move(*m_pendingPage);
        m_pendingPage.reset();
        unloadCurrentPage();
        loadDocumentNow(Path);
    }

    RmlUpdate();

    // Allow ViewModels to run logic that depends on post-layout values.
    for (auto& Vm : m_pageViewModels)
        Vm->PostRmlUpdate();
}

void RmlUILayer::Tick(float /*deltaTime*/)
{
    BeginFrame();
    RenderFrame();
    EndFrame();
}

// ── ScriptableObject ──────────────────────────────────────────────────────────
void RmlUILayer::RegisterObject(sol::state& Lua)
{
    auto Ui = Lua.create_named_table("UI");

    Ui.set_function("LoadFont", [this](const std::string& Path) {
        LoadFont(Path.c_str());
    });

    Ui.set_function("LoadDocument", [this](const std::string& Path) {
        LoadDocument(Path.c_str());
    });
}

void RmlUILayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}
