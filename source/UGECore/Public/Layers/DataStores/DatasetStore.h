#pragma once
#include "StoreBase.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class UGEDataLayer;  // for friend declaration

// ── DatasetStore ──────────────────────────────────────────────────────────────
// Read-only store backed by TOML files in assets/DATA/.
// Each file is keyed by its filename stem (e.g. "platformerData").
// Scalar access uses dot-notation matching the TOML section/key path.
// Loaded automatically during UGEDataLayer::Create().
//
// Key shape differs from PersistentStore / TransientStore — it is always
// two-part (Dataset + Path) rather than a single flat key.
class DatasetStore
{
public:
    // One row from an array-of-tables entry (e.g. one [[tiles]] block).
    using DataRow   = std::unordered_map<std::string, AppStateValue>;
    // All rows from a single array-of-tables key.
    using DataTable = std::vector<DataRow>;

    DatasetStore()  = default;
    ~DatasetStore() = default;

    // Returns true if a dataset with the given stem name was loaded.
    [[nodiscard]] bool HasDataset(const std::string& Name) const;

    // Scalar typed getters — Path is dot-notation, e.g. "physics.gravity".
    // Returns std::nullopt if the dataset, path, or type does not match.
    [[nodiscard]] std::optional<int>         GetInt   (const std::string& Dataset, const std::string& Path) const;
    [[nodiscard]] std::optional<float>       GetFloat (const std::string& Dataset, const std::string& Path) const;
    [[nodiscard]] std::optional<std::string> GetString(const std::string& Dataset, const std::string& Path) const;

    // Array-of-tables access.  ArrayKey is the top-level TOML array key
    // (e.g. "tiles" for [[tiles]] entries).
    // Returns nullptr if the dataset or array key does not exist.
    [[nodiscard]] const DataTable* GetRows(const std::string& Dataset,
                                           const std::string& ArrayKey) const;

    // ── Iteration ─────────────────────────────────────────────────────────────
    // Visit every scalar key/value pair in the named dataset.
    // No-op if the dataset does not exist.
    // Fn signature: void(const std::string& Key, const AppStateValue& Val)
    using ForEachCallback = std::function<void(const std::string& Key,
                                               const AppStateValue& Val)>;
    void ForEach(const std::string& Dataset, const ForEachCallback& Fn) const;

    // Load a single TOML file from the PhysFS VFS into the dataset store.
    // The dataset key is the filename stem (e.g. "platformerData" from
    // "assets/DATA/platformerData.toml").  Logs and returns false on failure.
    bool LoadFile(const std::string& VfsPath);

private:
    friend class UGEDataLayer;

    using ScalarMap = std::unordered_map<std::string, AppStateValue>;
    using ArrayMap  = std::unordered_map<std::string, DataTable>;

    // Parse TOML text Content and store it under stem key Stem.
    // Returns true on success; OutError receives the parse description on failure.
    bool loadFile(const std::string& Stem,
                  const std::string& Content,
                  std::string&       OutError);

    std::unordered_map<std::string, ScalarMap> m_datasets;
    std::unordered_map<std::string, ArrayMap>  m_datasetArrays;
};

