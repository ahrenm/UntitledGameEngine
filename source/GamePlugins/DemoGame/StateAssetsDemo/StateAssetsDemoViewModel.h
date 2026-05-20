#pragma once
#include <GameClasses/ViewModel.h>
#include <ViewModelRegistry.h>
#include <Layers/UGEDataLayer.h>
#include "StateAssetsDemoKeys.h"   // shared KEY_* DataStore keys (no Scene dependency)
#include <string>

// ── StateAssetsDemoViewModel ───────────────────────────────────────────────────
// Thin UI data model for the State / Assets demo page (data-model="state-assets-demo").
// The "character.*" DataStore keys are the single source of truth; this ViewModel
// caches local copies for the RML two-way bindings, pushes them into the store on
// Save, and data-binds to the character fields + the "character.data" container so
// a Scene-driven Load refreshes the UI reactively (see StateAssetsDemoKeys.h).
class StateAssetsDemoViewModel : public ViewModel
{
public:
    REGISTER_VIEWMODEL("state-assets-demo", StateAssetsDemoViewModel)

    void RegisterWith(Rml::Context* Context, const char* ModelName) override;

private:
    Rml::DataModelHandle m_model;

    // ── Table 1: Character data entry (cached copies of character.*) ──────────
    std::string m_characterName;
    std::string m_strength;
    std::string m_mana;

    // ── Table 1: First three "character.data.<key>" pairs (from the container) ─
    std::string m_kvKey1, m_kvVal1;
    std::string m_kvKey2, m_kvVal2;
    std::string m_kvKey3, m_kvVal3;

    // ── Table 1: Status bar ───────────────────────────────────────────────────
    std::string m_statusText         = "Ready.";
    int         m_loadButtonVisible  = 0;

    // ── Table 2: Sample text loaded from VFS ─────────────────────────────────
    std::string m_sampleText;

    // Pushes the cached character fields and free KV pairs into the persistent
    // "character.*" keys ahead of a save request.
    void pushCharacterToStore();

    // Reads the first three "character.data.<key>" entries from the store into the
    // kv members and dirties the bound RML variables (display refresh on Load).
    void readContainerIntoUI();

    // Scene ↔ UI reactive bindings.
    DataBinding m_nameBinding;      // character.name      → m_characterName
    DataBinding m_strengthBinding;  // character.strength  → m_strength
    DataBinding m_manaBinding;      // character.mana      → m_mana
    DataBinding m_dataBinding;      // character.data.*    → kv members (prefix watch)
    DataBinding m_statusBinding;    // stateAssets.status  (scene → UI)
    DataBinding m_loadBinding;      // stateAssets.loadVisible (scene → UI)
};
