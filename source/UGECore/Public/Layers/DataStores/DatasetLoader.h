#pragma once
#include "DataStore.h"

#include <string>

// ── DatasetLoader ───────────────────────────────────────────────────────────────
// Populates a DataStore from the read-only TOML datasets under assets/DATA/.
// Each dataset is folded into single dotted tags rooted at the file stem:
//
//   assets/DATA/platformerData.toml
//     [physics]
//     gravity = 9.8                     →  "platformerData.physics.gravity"  (float)
//
//   assets/DATA/platformerMap.toml
//     [[tiles]]
//     x = 3                             →  "platformerMap.tiles.0.x"         (int)
//     sprite = "tile-ground"            →  "platformerMap.tiles.0.sprite"    (string)
//
// Scalars are stored with meta { Source = TomlDataset, SourcePath = <vfs path> }.
// Arrays-of-tables are flattened into indexed sub-tags so DataStore::RowsView can
// enumerate them lazily.  All entries are non-serialised (read-only origin).
class DatasetLoader
{
public:
    // Load every *.toml file under DirVfsPath (default "assets/DATA") into Store.
    // Uses PhysFSLayer via the ServiceLocator; logs progress and per-file errors.
    // Returns the number of dataset files successfully loaded.
    static int LoadAll(DataStore& Store, const std::string& DirVfsPath = "assets/DATA");

    // Load a single TOML file from the PhysFS VFS into Store, rooted at the file
    // stem.  Logs and returns false on read/parse failure.
    static bool LoadFile(DataStore& Store, const std::string& VfsPath);
};
