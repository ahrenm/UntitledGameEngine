#pragma once
#include "DataStore.h"

// ── DataSerializer ──────────────────────────────────────────────────────────────
// TOML persistence for a DataStore.  Replaces the old StoreSerializer, which
// operated on a single-type RuntimeStore.
//
//   Save — walks Store.ForEach and emits only entries whose Meta.Serialize == true,
//          writing each dotted key as a nested TOML hierarchy.  The leaf value is
//          marshalled through the entry type's registered ValueConverter (ToToml),
//          so user types round-trip as long as a converter is registered.
//   Load — parses the TOML file, flattens it back into dotted keys, reconstructs
//          each value via the built-in converters, and Sets every entry with
//          meta { Serialize = true, Source = Serialized }.
//
// Real OS file paths are used (not the PhysFS VFS), matching the old
// SaveState / LoadState contract.  Both methods log and return false on failure.
class DataSerializer
{
public:
    [[nodiscard]] bool Save(const DataStore& Store, const char* FilePath) const;
    [[nodiscard]] bool Load(DataStore& Store, const char* FilePath) const;
};
