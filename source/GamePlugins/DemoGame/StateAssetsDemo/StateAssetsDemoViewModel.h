#pragma once
#include <GameClasses/ViewModel.h>
#include <ViewModelRegistry.h>
#include <Layers/UGEDataLayer.h>
#include <string>

// ── StateAssetsDemoViewModel ───────────────────────────────────────────────────
// Data model for the State / Assets demo page (data-model="state-assets-demo").
class StateAssetsDemoViewModel : public ViewModel
{
public:
    REGISTER_VIEWMODEL("state-assets-demo", StateAssetsDemoViewModel)

    // Transient key that gates load-button visibility.
    // Set to 1 by StateAssetsDemoScene on construction if Game.sav exists,
    // and again by saveToDataLayer after a successful save.
    static constexpr std::string_view KEY_LOAD_VISIBLE = "unhideSaveButton";

    void RegisterWith(Rml::Context* Context, const char* ModelName) override;

private:
    Rml::DataModelHandle m_model;

    // ── Table 1: Character data entry ─────────────────────────────────────────
    std::string m_characterName;
    std::string m_strength;
    std::string m_mana;

    // ── Table 1: Free key/value pairs ─────────────────────────────────────────
    std::string m_kvKey1, m_kvVal1;
    std::string m_kvKey2, m_kvVal2;
    std::string m_kvKey3, m_kvVal3;

    // ── Table 1: Status bar ───────────────────────────────────────────────────
    std::string m_statusText         = "Ready.";
    int         m_loadButtonVisible  = 0;     // ViewModel-only; no transient backer

    // ── Table 2: Sample text loaded from VFS ─────────────────────────────────
    std::string m_sampleText;

    // ── In-memory save slot ───────────────────────────────────────────────────
    struct SaveSlot
    {
        std::string characterName, strength, mana;
        std::string kvKey1, kvVal1;
        std::string kvKey2, kvVal2;
        std::string kvKey3, kvVal3;
    };
    SaveSlot m_savedSlot;
    bool     m_hasSave = false;

    // Reactive binding for load-button visibility (driven by KEY_LOAD_VISIBLE transient).
    AppStateBinding m_loadBinding;

    // Validates strength/mana, writes character data and free KV pairs to the
    // persistent store, serialises to Game.sav, and updates m_statusText.
    void saveToDataLayer();

    // Deserialises Game.sav into the persistent store, reads character fields
    // and up to three free KV pairs, and updates all m_ variables + model.
    void loadFromDataLayer();
};
