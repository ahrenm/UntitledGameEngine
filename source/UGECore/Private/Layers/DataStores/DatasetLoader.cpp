#include <Layers/DataStores/DatasetLoader.h>
#include <Layers/DataStores/DataValue.h>
#include "../../../Public/toml.hpp"
#include <Layers/PhysFSLayer.h>
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>

#include <filesystem>
#include <string>

static void logLoader(const std::string& Msg)
{
    if (auto* L = ServiceLocator::TryGet<LoggingLayer>())
        L->Log(Msg);
}

// ── flattenTable ────────────────────────────────────────────────────────────────
// Recursively walks a toml::table, emitting DataStore entries:
//   - scalar leaves          → "<prefix>.<key>"                (Source = TomlDataset)
//   - nested tables          → recurse with extended prefix
//   - arrays-of-tables       → "<prefix>.<key>.<index>.<field>" per row
static void flattenTable(const toml::table& Table,
                         const std::string& Prefix,
                         const std::string& SourcePath,
                         DataStore&         Store)
{
    const DataMeta Meta{ .Serialize  = false,
                         .Source     = DataSource::TomlDataset,
                         .SourcePath = SourcePath };

    for (auto&& [K, V] : Table)
    {
        const std::string FullKey = Prefix.empty()
            ? std::string(K.str())
            : Prefix + "." + std::string(K.str());

        if (V.is_table())
        {
            flattenTable(*V.as_table(), FullKey, SourcePath, Store);
            continue;
        }

        if (V.is_array())
        {
            const auto* Arr = V.as_array();
            if (Arr && Arr->is_array_of_tables())
            {
                std::size_t Index = 0;
                for (auto&& Elem : *Arr)
                {
                    if (const auto* RowTable = Elem.as_table())
                    {
                        const std::string RowPrefix =
                            FullKey + "." + std::to_string(Index);
                        flattenTable(*RowTable, RowPrefix, SourcePath, Store);
                    }
                    ++Index;
                }
            }
            // Non-table scalar arrays are not currently mapped.
            continue;
        }

        DataValue Value = MakeDataValueFromToml(V);
        if (Value.HasValue())
            Store.Set(FullKey, std::move(Value), Meta);
    }
}

// ── DatasetLoader ───────────────────────────────────────────────────────────────

bool DatasetLoader::LoadFile(DataStore& Store, const std::string& VfsPath)
{
    auto* Physfs = ServiceLocator::TryGet<PhysFSLayer>();
    if (!Physfs)
    {
        logLoader("DatasetLoader::LoadFile — PhysFSLayer not available");
        return false;
    }

    const auto Bytes = Physfs->ReadFile(VfsPath.c_str());
    if (Bytes.empty())
    {
        logLoader("DatasetLoader::LoadFile — failed to read: " + VfsPath);
        return false;
    }

    const std::string Stem = std::filesystem::path(VfsPath).stem().string();
    const std::string Content(reinterpret_cast<const char*>(Bytes.data()), Bytes.size());

    try
    {
        auto Table = toml::parse(Content);
        flattenTable(Table, Stem, VfsPath, Store);
        return true;
    }
    catch (const toml::parse_error& E)
    {
        logLoader("DatasetLoader::LoadFile — TOML parse error in " + VfsPath + ": " + E.what());
        return false;
    }
}

int DatasetLoader::LoadAll(DataStore& Store, const std::string& DirVfsPath)
{
    auto* Physfs = ServiceLocator::TryGet<PhysFSLayer>();
    if (!Physfs)
    {
        logLoader("DatasetLoader::LoadAll — PhysFSLayer not available");
        return 0;
    }

    const auto Files = Physfs->ListFiles(DirVfsPath.c_str());
    int Loaded = 0, Skipped = 0;

    for (const auto& Path : Files)
    {
        if (!Path.ends_with(".toml")) { ++Skipped; continue; }

        if (LoadFile(Store, Path))
        {
            ++Loaded;
            logLoader("DatasetLoader: loaded dataset from " + Path);
        }
    }

    logLoader("DatasetLoader: " + std::to_string(Loaded) + " dataset(s) loaded, " +
              std::to_string(Skipped) + " non-toml file(s) skipped in " + DirVfsPath + "/");

    return Loaded;
}
