#pragma once
#include <cstdint>
#include <string>

// ── DataSource ────────────────────────────────────────────────────────────────
// Provenance of a stored entry.  Each entry records where it came from via
// per-entry metadata.
enum class DataSource : uint8_t
{
    Unknown = 0,
    TomlDataset,       // loaded from assets/DATA/**.toml (read-only origin)
    Config,            // injected from launch/config
    RuntimeTransient,  // created at runtime, ephemeral
    Serialized,        // restored from / destined for a save file
};

// ── DataMeta ──────────────────────────────────────────────────────────────────
// Per-entry metadata carried alongside every DataValue.
//
//   Serialize  — when true, the entry is included in DataSerializer output.
//   Source     — where the value originated (see DataSource).
//   SourcePath — optional origin path, e.g. "assets/DATA/platformerData.toml".
//
// Default meta ({Serialize=false, Source=RuntimeTransient}) reproduces the old
// Transient-store semantics: runtime-only and never serialised.
struct DataMeta
{
    bool        Serialize = false;
    DataSource  Source    = DataSource::RuntimeTransient;
    std::string SourcePath;
};
