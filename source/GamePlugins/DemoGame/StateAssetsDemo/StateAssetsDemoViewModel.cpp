#include <StateAssetsDemoViewModel.h>
#include "StateAssetsDemoKeys.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>

using namespace StateAssetsDemoKeys;   // KEY_* shared DataStore keys

void StateAssetsDemoViewModel::RegisterWith(Rml::Context* Context, const char* ModelName)
{
    SetContext(Context);

    // ── Load sample text from virtual filesystem ──────────────────────────────
    if (auto* FS = GetPhysFSLayer())
    {
        auto Bytes = FS->ReadFile("assets/Text/sampleText.txt");
        if (!Bytes.empty())
            m_sampleText = std::string(reinterpret_cast<const char*>(Bytes.data()), Bytes.size());
        else
            m_sampleText = "(File not found: assets/Text/sampleText.txt)";
    }
    else
    {
        m_sampleText = "(PhysFSLayer unavailable)";
    }

    auto Ctor = Context->CreateDataModel(ModelName);

    // ── Table 1 bindings ─────────────────────────────────────────────────────
    Ctor.Bind("character_name",     &m_characterName);
    Ctor.Bind("strength",           &m_strength);
    Ctor.Bind("mana",               &m_mana);

    Ctor.Bind("kv_key1", &m_kvKey1);  Ctor.Bind("kv_val1", &m_kvVal1);
    Ctor.Bind("kv_key2", &m_kvKey2);  Ctor.Bind("kv_val2", &m_kvVal2);
    Ctor.Bind("kv_key3", &m_kvKey3);  Ctor.Bind("kv_val3", &m_kvVal3);

    Ctor.Bind("status_text",        &m_statusText);
    Ctor.Bind("load_button_visible",&m_loadButtonVisible);

    // ── Table 2 bindings ─────────────────────────────────────────────────────
    Ctor.Bind("sample_text", &m_sampleText);

    // ── Event callbacks — pure UI→Scene triggers ──────────────────────────────
    // Save: push the cached character.* fields into the store, then bump the
    // request trigger so the scene validates + serialises. Load: just bump the
    // request trigger; the scene's Load fires our bindings to refresh the UI.
    Ctor.BindEventCallback("onSave",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            pushCharacterToStore();
            if (auto* Data = GetDataLayer())
                Data->Store.Set(std::string(KEY_SAVE_REQUEST), DataValue{1});
        });

    Ctor.BindEventCallback("onLoad",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (auto* Data = GetDataLayer())
                Data->Store.Set(std::string(KEY_LOAD_REQUEST), DataValue{1});
        });

    Ctor.BindEventCallback("onPrevious",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (auto* SDL = GetSDLLayer())
                SDL->LoadScene("lua-tests");
        });

    Ctor.BindEventCallback("onNext",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (auto* SDL = GetSDLLayer())
                SDL->LoadScene("teapot-demo");
        });

    m_model = Ctor.GetModelHandle();

    // ── Scene → UI: named character fields ────────────────────────────────────
    // Persistent (default) target so these entries serialise to Game.sav. Each
    // callback mirrors the stored value into its cached copy and refreshes RML.
    m_nameBinding = DATA_BIND(KEY_CHARACTER_NAME.data(), std::string{},
        [this](const Tag&, const DataValue& Val)
        {
            if (const auto* S = Val.TryAs<std::string>()) m_characterName = *S;
            m_model.DirtyVariable("character_name");
        });
    if (const auto* V = m_nameBinding.GetValue())
        if (const auto* S = V->TryAs<std::string>()) m_characterName = *S;

    m_strengthBinding = DATA_BIND(KEY_CHARACTER_STRENGTH.data(), std::string{},
        [this](const Tag&, const DataValue& Val)
        {
            if (const auto* S = Val.TryAs<std::string>()) m_strength = *S;
            m_model.DirtyVariable("strength");
        });
    if (const auto* V = m_strengthBinding.GetValue())
        if (const auto* S = V->TryAs<std::string>()) m_strength = *S;

    m_manaBinding = DATA_BIND(KEY_CHARACTER_MANA.data(), std::string{},
        [this](const Tag&, const DataValue& Val)
        {
            if (const auto* S = Val.TryAs<std::string>()) m_mana = *S;
            m_model.DirtyVariable("mana");
        });
    if (const auto* V = m_manaBinding.GetValue())
        if (const auto* S = V->TryAs<std::string>()) m_mana = *S;

    // ── Scene → UI: "character.data" container (prefix watch) ─────────────────
    // A single subscription fires for any "character.data.*" child change; we
    // re-scan and surface the first three pairs. Wrapped in a DataBinding so the
    // prefix subscription is released with the ViewModel.
    if (auto* Data = GetDataLayer())
    {
        auto& Store = Data->Store;
        const std::string DataPrefix = std::string(KEY_CHARACTER_DATA) + ".";
        auto Token = Store.SubscribePrefix(DataPrefix,
            [this](const Tag&, const DataValue&) { readContainerIntoUI(); });
        m_dataBinding = DataBinding(&Store, Token, DataPrefix);
    }
    readContainerIntoUI();   // seed initial display

    // ── Scene → UI: status line ───────────────────────────────────────────────
    m_statusBinding = DATA_BIND(KEY_STATUS.data(), std::string("Ready."),
        [this](const Tag&, const DataValue& Val)
        {
            if (const auto* S = Val.TryAs<std::string>())
                m_statusText = *S;
            m_model.DirtyVariable("status_text");
        }, Transient);
    if (const auto* V = m_statusBinding.GetValue())
        if (const auto* S = V->TryAs<std::string>())
            m_statusText = *S;

    // ── Scene → UI: load-button visibility (seeded by the scene on construction) ─
    m_loadBinding = DATA_BIND(KEY_LOAD_VISIBLE.data(), 0,
        [this](const Tag&, const DataValue& Val)
        {
            if (const auto* I = Val.TryAs<int>())
                m_loadButtonVisible = *I;
            m_model.DirtyVariable("load_button_visible");
        }, Transient);
    if (const auto* V = m_loadBinding.GetValue())
        if (const auto* I = V->TryAs<int>())
            m_loadButtonVisible = *I;
}

// ── pushCharacterToStore ─────────────────────────────────────────────────────────

void StateAssetsDemoViewModel::pushCharacterToStore()
{
    auto* Data = GetDataLayer();
    if (!Data) return;
    auto& Store = Data->Store;

    // Named fields — SetValue preserves the persistent meta seeded by DATA_BIND.
    m_nameBinding.SetValue(DataValue{ m_characterName });
    m_strengthBinding.SetValue(DataValue{ m_strength });
    m_manaBinding.SetValue(DataValue{ m_mana });

    // Free KV pairs → "character.data.<key>" container entries (persistent).
    // Skip empty keys so blank rows don't pollute the container.
    //
    // Snapshot the pairs FIRST: each Store.Set fires the "character.data" prefix
    // subscription (readContainerIntoUI), which rewrites the kv members from the
    // store mid-loop. Writing from the snapshot keeps later pairs intact so all
    // three are committed, not just the first.
    const DataMeta    Persist    = DataBindTarget::Persistent();
    const std::string DataPrefix = std::string(KEY_CHARACTER_DATA) + ".";
    const std::array<std::pair<std::string, std::string>, 3> Pairs {{
        { m_kvKey1, m_kvVal1 },
        { m_kvKey2, m_kvVal2 },
        { m_kvKey3, m_kvVal3 },
    }};
    for (const auto& [K, V] : Pairs)
        if (!K.empty())
            Store.Set(DataPrefix + K, DataValue{ V }, Persist);
}

// ── readContainerIntoUI ──────────────────────────────────────────────────────────

void StateAssetsDemoViewModel::readContainerIntoUI()
{
    auto* Data = GetDataLayer();
    if (!Data) return;

    // Collect up to three "character.data.*" pairs. Iteration order is
    // unordered — good enough for this demo's "first three" display.
    const std::string DataPrefix = std::string(KEY_CHARACTER_DATA) + ".";
    std::array<std::pair<std::string, std::string>, 3> Kv;
    std::size_t Slot = 0;

    Data->Store.ForEach([&](const Tag& Key, const DataEntry& Entry)
    {
        if (Slot >= Kv.size()) return;
        const std::string& K = Key.Str();
        if (!K.starts_with(DataPrefix)) return;
        if (const auto* S = Entry.Value.TryAs<std::string>())
        {
            Kv[Slot] = { K.substr(DataPrefix.size()), *S };
            ++Slot;
        }
    });

    m_kvKey1 = Kv[0].first;  m_kvVal1 = Kv[0].second;
    m_kvKey2 = Kv[1].first;  m_kvVal2 = Kv[1].second;
    m_kvKey3 = Kv[2].first;  m_kvVal3 = Kv[2].second;

    m_model.DirtyVariable("kv_key1");  m_model.DirtyVariable("kv_val1");
    m_model.DirtyVariable("kv_key2");  m_model.DirtyVariable("kv_val2");
    m_model.DirtyVariable("kv_key3");  m_model.DirtyVariable("kv_val3");
}

