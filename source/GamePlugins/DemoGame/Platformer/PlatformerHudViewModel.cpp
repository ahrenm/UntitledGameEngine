#include <PlatformerHudViewModel.h>
#include <ServiceLocator.h>

void PlatformerHudViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
{
    SetContext(Context);

    auto Ctor = Context->CreateDataModel(ModelName);
    Ctor.Bind("score", &m_score);

    Ctor.BindEventCallback("onNext",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            //if (onNext)
            //    onNext();
                GetSDLLayer()->LoadScene("lua-tests");
        });

    m_model = Ctor.GetModelHandle();

    // -- Score � transient AppState subscription -------------------------------
    // Subscribe to "Platformer2d.Score" so SetScore() is called whenever the
    // scene updates the store.
    m_scoreBinding = DATA_BIND(PlatformerHudViewModel::SCORE_KEY, 0,
        [this](const Tag&, const DataValue& Val) {
            if (const int* I = Val.TryAs<int>()) SetScore(*I);
        }, Transient);
    // Initialise from whatever value is already in the store.
    if (const auto* Val = m_scoreBinding.GetValue())
        if (const int* I = Val->TryAs<int>()) SetScore(*I);

    // Make this instance retrievable via ServiceLocator::Get<PlatformerHudViewModel>().
    ServiceLocator::Provide(this);
}

void PlatformerHudViewModel::SetScore(int Score)
{
    m_score = Score;
    m_model.DirtyVariable("score");
}

