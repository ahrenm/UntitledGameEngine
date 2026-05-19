#pragma once
#include "RuntimeStore.h"

// ── StoreSerializer ─────────────────────────────────────────────────
// TOML-based serialiser/deserialiser for RuntimeStore.
// Owned by UDEDataLayer; access via UDEDataLayer::SaveState / LoadState.
//
// Dot-notation keys are expanded into a nested TOML hierarchy on Save:
//   "ui.console.visible" = 1  →  [ui.console]  /  visible = 1
//   "physics.gravity"  = 9.8  →  [physics]     /  gravity = 9.800000
//
// On Load the hierarchy is collapsed back into flat dot-notation keys and merged
// into the store via Set().  Existing keys are overwritten; absent keys are left
// untouched.  TOML booleans are normalised to int (0/1) on read.
class StoreSerializer
{
public:
    // Serialise Store to a TOML file at FilePath (real OS path, not PhysFS).
    // Returns false and logs on I/O failure.
    [[nodiscard]] bool Save(const RuntimeStore& Store, const char* FilePath) const;

    // Deserialise a TOML file at FilePath and merge its values into Store.
    // Returns false and logs on I/O or parse failure.
    [[nodiscard]] bool Load(RuntimeStore& Store, const char* FilePath) const;
};
