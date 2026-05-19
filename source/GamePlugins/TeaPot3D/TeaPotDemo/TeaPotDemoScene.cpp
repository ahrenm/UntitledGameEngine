#include <TeaPotDemoScene.h>
#include <TeaPotDemoViewModel.h>
#include <Render3DLayer/Render3DLayer.h>
#include <Layers/SDLLayer.h>
#include <Layers/RmlUILayer.h>
#include <ServiceLocator.h>

// ── Colour table ──────────────────────────────────────────────────────────────
namespace {
    struct ColorEntry { const char* Name; float R, G, B; };
    static constexpr ColorEntry COLOR_TABLE[] = {
        { "white",  1.00f, 1.00f, 1.00f },
        { "red",    1.00f, 0.20f, 0.20f },
        { "green",  0.20f, 1.00f, 0.30f },
        { "blue",   0.30f, 0.55f, 1.00f },
        { "gold",   1.00f, 0.80f, 0.15f },
        { "purple", 0.80f, 0.20f, 1.00f },
        { "cyan",   0.20f, 0.90f, 0.90f },
    };
}

// ── Constructor ───────────────────────────────────────────────────────────────
TeaPotDemoScene::TeaPotDemoScene(SDL_Renderer* Renderer, SDL_Window* Window)
    : SceneObject(Renderer, Window)
{

    // ── 3-D setup ─────────────────────────────────────────────────────────────
    if (auto* R = ServiceLocator::TryGet<Render3DLayer>())
    {
        // Camera: slightly above and behind, looking at the origin.
        R->SetCamera(0.0f, 2.5f, 8.0f,
                     0.0f, 0.0f, 0.0f,
                     60.0f);

        // Directional light from upper-right, white with mild blue ambient.
        // Negate the vector: the renderer uses the direction as "light travels toward
        // surface", so (-X, -Y, -Z) places the source at upper-right-front.
        R->SetLightDirection(-1.0f, -1.5f, -1.0f);
        R->SetLightColor(1.0f, 1.0f, 1.0f);
        R->SetAmbientColor(0.35f, 0.35f, 0.45f);

        // Dark navy clear colour matching the UI colour scheme.
        R->SetViewportClearColor({30, 30, 46, 255});

        // Queue the model — actual parse happens on the first Update().
        R->LoadModel("assets/3d/teapot.obj");

        // Activate fills the viewport to the full renderer size automatically.
        R->Activate();
    }

    // ── Light colour binding — reacts to the UI colour dropdown ───────────────
    // The ViewModel seeds KEY_LIGHT_COLOR to "white" before RegisterWith returns,
    // so this subscription will fire immediately with the default and keep in sync.
    m_lightColorBinding = APPSTATE_BIND_TRANSIENT(
        TeaPotDemoViewModel::KEY_LIGHT_COLOR, std::string("white"),
        [this](const std::string&, const AppStateValue& Val)
        {
            if (const auto* S = std::get_if<std::string>(&Val))
                applyLightColor(*S);
        });

    // Seed from current stored value (covers the case where the ViewModel seeded
    // it before the scene subscribed).
    if (const auto* V = m_lightColorBinding.GetValue())
        if (const auto* S = std::get_if<std::string>(V))
            applyLightColor(*S);

    // ── UI overlay ────────────────────────────────────────────────────────────
    if (auto* UI = GetUILayer())
        UI->LoadDocument("assets/ui/teapot_demo.rml");
}

// ── Destructor ────────────────────────────────────────────────────────────────
TeaPotDemoScene::~TeaPotDemoScene()
{
    // Release all 3-D resources when the scene is torn down so the next scene
    // starts with a clean Render3DLayer state.
    if (auto* R = ServiceLocator::TryGet<Render3DLayer>())
        R->Deactivate();
}

// ── applyLightColor (private) ─────────────────────────────────────────────────
// Looks up ColorName in COLOR_TABLE and calls SetLightColor(); falls back to
// white if the name is not found.
void TeaPotDemoScene::applyLightColor(const std::string& ColorName)
{
    auto* R = ServiceLocator::TryGet<Render3DLayer>();
    if (!R) return;

    for (const auto& Entry : COLOR_TABLE)
    {
        if (ColorName == Entry.Name)
        {
            R->SetLightColor(Entry.R, Entry.G, Entry.B);
            return;
        }
    }

    // Unknown name — fall back to white.
    R->SetLightColor(1.0f, 1.0f, 1.0f);
}

// ── Update ────────────────────────────────────────────────────────────────────
void TeaPotDemoScene::Update()
{
    // No per-frame logic needed; rotation is applied in Tick().
}

// ── Tick ──────────────────────────────────────────────────────────────────────
void TeaPotDemoScene::Tick(float DeltaTime)
{
    if (auto* R = ServiceLocator::TryGet<Render3DLayer>())
    {
        // Rotate the teapot 30 degrees per second around the Y-axis.
        m_yaw += DeltaTime * 30.0f;
        if (m_yaw >= 360.0f) m_yaw -= 360.0f;
        R->SetModelRotation(0.0f, m_yaw, 0.0f);
    }
}

