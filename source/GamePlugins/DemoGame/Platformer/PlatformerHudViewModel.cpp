#include <PlatformerHudViewModel.h>
#include <ServiceLocator.h>

void PlatformerHudViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
{
    SetContext(Context);

    auto Ctor = Context->CreateDataModel(ModelName);
    Ctor.Bind("score", &m_score);

    BindLoadScene(Ctor, "onNext", "lua-tests");

    m_model = Ctor.GetModelHandle();

    // ── Score — transient AppState subscription ───────────────────────────────
    // Subscribe to SCORE_KEY so the HUD refreshes whenever the scene updates the
    // store; the macro also seeds m_score from the current value.
    VM_BIND_MODEL_INT(m_scoreBinding, SCORE_KEY, m_score, m_model, "score", Transient);

    // Make this instance retrievable via ServiceLocator::Get<PlatformerHudViewModel>().
    ServiceLocator::Provide(this);
}

void PlatformerHudViewModel::SetScore(int Score)
{
    m_score = Score;
    m_model.DirtyVariable("score");
}

