#include <StateAssetsDemoViewModel.h>
#include <stdexcept>

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

    // ── Event callbacks ───────────────────────────────────────────────────────
    Ctor.BindEventCallback("onSave",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            saveToDataLayer();
        });

    Ctor.BindEventCallback("onLoad",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            loadFromDataLayer();
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

    // ── Load-button visibility — driven by KEY_LOAD_VISIBLE transient ─────────
    // Subscribe so future Set() calls (e.g. from saveToDataLayer) auto-update.
    m_loadBinding = APPSTATE_BIND_TRANSIENT(KEY_LOAD_VISIBLE.data(), 0,
        [this](const std::string&, const AppStateValue& Val)
        {
            if (const auto* I = std::get_if<int>(&Val))
                m_loadButtonVisible = *I;
            m_model.DirtyVariable("load_button_visible");
        });

    // Seed from the current stored value (set by scene if Game.sav existed on load).
    if (const auto* V = m_loadBinding.GetValue())
        if (const auto* I = std::get_if<int>(V))
            m_loadButtonVisible = *I;
}

// ── saveToDataLayer ───────────────────────────────────────────────────────────

void StateAssetsDemoViewModel::saveToDataLayer()
{
    // ── Validate numeric fields ───────────────────────────────────────────────
    int strengthVal = 0;
    int manaVal     = 0;

    try { strengthVal = std::stoi(m_strength); }
    catch (const std::exception&)
    {
        m_statusText = "Error: Strength must be a whole number.";
        m_model.DirtyVariable("status_text");
        return;
    }

    try { manaVal = std::stoi(m_mana); }
    catch (const std::exception&)
    {
        m_statusText = "Error: Mana must be a whole number.";
        m_model.DirtyVariable("status_text");
        return;
    }

    // ── Write to persistent store ─────────────────────────────────────────────
    auto* DataLayer = GetDataLayer();
    if (!DataLayer)
    {
        m_statusText = "Error: Data layer unavailable.";
        m_model.DirtyVariable("status_text");
        return;
    }

    auto& Store = DataLayer->State;

    Store.Set("character.name",     AppStateValue{ m_characterName });
    Store.Set("character.strength", AppStateValue{ strengthVal });
    Store.Set("character.mana",     AppStateValue{ manaVal });

    // Free KV pairs — skip any entry whose key field is empty.
    auto writeKv = [&Store](const std::string& Key, const std::string& Val)
    {
        if (!Key.empty())
            Store.Set(Key, AppStateValue{ Val });
    };
    writeKv(m_kvKey1, m_kvVal1);
    writeKv(m_kvKey2, m_kvVal2);
    writeKv(m_kvKey3, m_kvVal3);

    // ── Serialise ─────────────────────────────────────────────────────────────
    if (!DataLayer->SaveState("Game.sav"))
    {
        m_statusText = "Error: Could not write Game.sav.";
        m_model.DirtyVariable("status_text");
        return;
    }

    // ── Update in-memory save slot and UI ─────────────────────────────────────
    m_savedSlot = { m_characterName, m_strength, m_mana,
                    m_kvKey1, m_kvVal1,
                    m_kvKey2, m_kvVal2,
                    m_kvKey3, m_kvVal3 };
    m_hasSave    = true;
    m_statusText = "Saved to Game.sav.";

    // Signal load-button visibility through the transient — the binding
    // subscriber handles m_loadButtonVisible and DirtyVariable("load_button_visible").
    DataLayer->Transient.Set(std::string(KEY_LOAD_VISIBLE), AppStateValue{1});

    m_model.DirtyVariable("status_text");
}

// ── loadFromDataLayer ─────────────────────────────────────────────────────────

void StateAssetsDemoViewModel::loadFromDataLayer()
{
    auto* DataLayer = GetDataLayer();
    if (!DataLayer)
    {
        m_statusText = "Error: Data layer unavailable.";
        m_model.DirtyVariable("status_text");
        return;
    }

    if (!DataLayer->LoadState("Game.sav"))
    {
        m_statusText = "Error: Could not read Game.sav.";
        m_model.DirtyVariable("status_text");
        return;
    }

    auto& Store = DataLayer->State;

    // ── Known character fields ────────────────────────────────────────────────
    m_characterName = Store.GetString("character.name").value_or("");
    if (auto V = Store.GetInt("character.strength")) m_strength = std::to_string(*V);
    if (auto V = Store.GetInt("character.mana"))     m_mana     = std::to_string(*V);

    // ── Free KV pairs — iterate non-character entries, fill slots in order ────
    // Pointers to the three UI slot pairs for concise assignment.
    std::array<std::pair<std::string*, std::string*>, 3> Slots {{
        {&m_kvKey1, &m_kvVal1}, {&m_kvKey2, &m_kvVal2}, {&m_kvKey3, &m_kvVal3}
    }};
    size_t SlotIdx = 0;

    Store.ForEach([&](const std::string& Key, const AppStateValue& Val)
    {
        if (SlotIdx >= 3 || Key.starts_with("character.")) return;
        if (const auto* S = std::get_if<std::string>(&Val))
        {
            *Slots[SlotIdx].first  = Key;
            *Slots[SlotIdx].second = *S;
            ++SlotIdx;
        }
    });

    // Clear any slots not filled from the file.
    for (; SlotIdx < 3; ++SlotIdx)
        *Slots[SlotIdx].first = *Slots[SlotIdx].second = "";

    // ── Update in-memory slot, flags, status ──────────────────────────────────
    m_savedSlot = { m_characterName, m_strength, m_mana,
                    m_kvKey1, m_kvVal1,
                    m_kvKey2, m_kvVal2,
                    m_kvKey3, m_kvVal3 };
    m_hasSave    = true;
    m_statusText = "Loaded from Game.sav. (Mini-bonus: go back to platfomer level)";

    // ── Dirty all bound model variables ──────────────────────────────────────
    for (const char* Var : { "character_name", "strength", "mana",
                              "kv_key1", "kv_val1", "kv_key2", "kv_val2",
                              "kv_key3", "kv_val3", "status_text" })
        m_model.DirtyVariable(Var);
}

