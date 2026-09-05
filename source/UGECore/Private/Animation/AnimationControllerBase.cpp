#include <Animation/AnimationControllerBase.h>
#include <Render/Renderer2D.h>
#include <Layers/UGEDataLayer.h>

// ── Constructor ───────────────────────────────────────────────────────────────
AnimationControllerBase::AnimationControllerBase(Renderer2D* Renderer)
    : m_renderer2D(Renderer)
{}

// ── LoadAnimation ─────────────────────────────────────────────────────────────
void AnimationControllerBase::LoadAnimation(const char*        DatasetKey,
                                             const std::string& SectionPrefix,
                                             const std::string& AnimName)
{
    auto* data = GetDataLayer();
    if (!data) return;

    auto& store = data->Store;
    const std::string base = std::string(DatasetKey) + "." + SectionPrefix + ".";

    auto getStr = [&](const char* Leaf, std::string Def) -> std::string {
        if (const DataValue* V = store.Get(base + Leaf))
            if (const std::string* P = V->TryAs<std::string>()) return *P;
        return Def;
    };
    auto getInt = [&](const char* Leaf, int Def) -> int {
        if (const DataValue* V = store.Get(base + Leaf))
            if (const int* P = V->TryAs<int>()) return *P;
        return Def;
    };
    auto getFloat = [&](const char* Leaf, float Def) -> float {
        if (const DataValue* V = store.Get(base + Leaf))
            if (const float* P = V->TryAs<float>()) return *P;
        return Def;
    };

    const auto  SpritePath    = getStr  ("sprite",         "");
    const int   FrameW        = getInt  ("frame_w",        64);
    const int   FrameH        = getInt  ("frame_h",        64);
    const int   PaddingH      = getInt  ("padding_h",      0);
    const int   StartFrame    = getInt  ("start_frame",    0);
    const int   FrameCount    = getInt  ("frame_count",    1);
    const float FrameDuration = getFloat("frame_duration", 0.1f);

    if (SpritePath.empty()) return;
    auto Result = SpriteSheet::Load(SpritePath.c_str(), FrameW, FrameH, PaddingH);
    if (!Result) return;

    // SpriteSheet is heap-allocated so its address is stable across future map
    // insertions (which may rehash and move the AnimEntry value-type).
    AnimEntry entry;
    entry.Sheet           = std::make_unique<SpriteSheet>(std::move(*Result));
    entry.Anim.Sheet      = entry.Sheet.get();   // stable pointer — unique_ptr preserves address on move
    entry.Anim.StartFrame    = StartFrame;
    entry.Anim.FrameCount    = FrameCount;
    entry.Anim.FrameDuration = FrameDuration;

    m_animations[AnimName] = std::move(entry);
}

// ── Playback control ──────────────────────────────────────────────────────────
void AnimationControllerBase::SetCurrentAnimation(const std::string& AnimName)
{
    m_currentAnim = AnimName;
}

void AnimationControllerBase::ResetAnimation(const std::string& AnimName)
{
    if (auto* entry = getEntry(AnimName))
        entry->Anim.Reset();
}

bool AnimationControllerBase::IsAnimationValid(const std::string& AnimName) const
{
    const auto* entry = getEntry(AnimName);
    return entry && entry->Sheet && entry->Sheet->IsValid();
}

SDL_FPoint AnimationControllerBase::GetRenderSize(const std::string& AnimName) const
{
    const auto* entry = getEntry(AnimName);
    if (!entry || !entry->Sheet || !entry->Sheet->IsValid())
        return { 0.0f, 0.0f };
    return { static_cast<float>(entry->Sheet->RenderedW()),
             static_cast<float>(entry->Sheet->RenderedH()) };
}

// ── Frame loop ────────────────────────────────────────────────────────────────
void AnimationControllerBase::Tick(float DeltaTime)
{
    if (auto* entry = getEntry(m_currentAnim))
        entry->Anim.Tick(DeltaTime);
}

void AnimationControllerBase::Draw(const SDL_FRect& Rect, SDL_FlipMode FlipMode) const
{
    if (!m_renderer2D) return;
    if (const auto* entry = getEntry(m_currentAnim))
        entry->Anim.Draw(*m_renderer2D, Rect, FlipMode);
}

// ── Internal helpers ──────────────────────────────────────────────────────────
AnimationControllerBase::AnimEntry*
AnimationControllerBase::getEntry(const std::string& Name)
{
    auto it = m_animations.find(Name);
    return it != m_animations.end() ? &it->second : nullptr;
}

const AnimationControllerBase::AnimEntry*
AnimationControllerBase::getEntry(const std::string& Name) const
{
    auto it = m_animations.find(Name);
    return it != m_animations.end() ? &it->second : nullptr;
}

