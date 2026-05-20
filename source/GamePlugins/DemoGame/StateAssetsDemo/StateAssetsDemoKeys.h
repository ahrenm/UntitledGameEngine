#pragma once
#include <string_view>

// ── State / Assets demo shared DataStore keys ─────────────────────────────────
// Two namespaces of keys, one source of truth each:
//   • "stateAssets.*" — transient UI-behaviour keys (action triggers + feedback).
//   • "character.*"    — the persistent character data the demo saves to Game.sav.
//
// The ViewModel caches local copies of the character fields, pushes them into the
// "character.*" keys on Save, and data-binds to those same keys (plus the
// "character.data" container) so a Scene-driven Load refreshes the UI reactively.
namespace StateAssetsDemoKeys
{
    // ── UI behaviour (transient) ──────────────────────────────────────────────
    // UI → Scene action triggers (the ViewModel bumps these on button clicks).
    inline constexpr std::string_view KEY_SAVE_REQUEST = "stateAssets.saveRequest";
    inline constexpr std::string_view KEY_LOAD_REQUEST = "stateAssets.loadRequest";

    // Scene → UI feedback (the ViewModel subscribes to these).
    inline constexpr std::string_view KEY_STATUS       = "stateAssets.status";
    inline constexpr std::string_view KEY_LOAD_VISIBLE = "stateAssets.loadVisible";

    // ── Character data (persistent, serialised to Game.sav) ───────────────────
    // The named character fields — the single source of truth for both classes.
    inline constexpr std::string_view KEY_CHARACTER_NAME     = "character.name";
    inline constexpr std::string_view KEY_CHARACTER_STRENGTH = "character.strength";
    inline constexpr std::string_view KEY_CHARACTER_MANA     = "character.mana";

    // Free key/value pairs live under the "character.data" container as
    // "character.data.<key>" entries.  The ViewModel watches this prefix and
    // surfaces the first three children into the UI.
    inline constexpr std::string_view KEY_CHARACTER_DATA     = "character.data";
}


