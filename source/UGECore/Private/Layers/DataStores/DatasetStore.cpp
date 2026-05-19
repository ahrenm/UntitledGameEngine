#include <Layers/DataStores/DatasetStore.h>
#include "../../../Public/toml.hpp"
#include <Layers/PhysFSLayer.h>
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>
#include <filesystem>

static void logDataset(const std::string& Msg)
{
    if (auto* L = ServiceLocator::TryGet<LoggingLayer>())
        L->Log(Msg);
}

// ── flattenTable ──────────────────────────────────────────────────────────────
// Recursively walks a toml::table, collecting:
//   - scalar leaves  (int, float, string, bool) into Scalars with a dotted path key
//   - arrays-of-tables                          into Arrays keyed by their top-level name

static void flattenTable(
    const toml::table&                                         Table,
    const std::string&                                         Prefix,
    std::unordered_map<std::string, AppStateValue>&            Scalars,
    std::unordered_map<std::string, DatasetStore::DataTable>&  Arrays)
{
    for (auto&& [K, V] : Table)
    {
        const std::string FullKey = Prefix.empty()
            ? std::string(K.str())
            : Prefix + "." + std::string(K.str());

        switch (V.type())
        {
        case toml::node_type::integer:
            Scalars[FullKey] = static_cast<int>(V.ref<int64_t>());
            break;
        case toml::node_type::floating_point:
            Scalars[FullKey] = static_cast<float>(V.ref<double>());
            break;
        case toml::node_type::string:
            Scalars[FullKey] = std::string(V.ref<std::string>());
            break;
        case toml::node_type::boolean:
            Scalars[FullKey] = V.ref<bool>() ? 1 : 0;
            break;
        case toml::node_type::table:
            flattenTable(*V.as_table(), FullKey, Scalars, Arrays);
            break;
        case toml::node_type::array:
        {
            const auto* Arr = V.as_array();
            if (Arr->is_array_of_tables())
            {
                DatasetStore::DataTable Rows;
                for (auto&& Elem : *Arr)
                {
                    if (const auto* RowTable = Elem.as_table())
                    {
                        DatasetStore::DataRow Row;
                        std::unordered_map<std::string, DatasetStore::DataTable> Ignored;
                        flattenTable(*RowTable, "", Row, Ignored);
                        Rows.push_back(std::move(Row));
                    }
                }
                Arrays[FullKey] = std::move(Rows);
            }
            // Non-table scalar arrays are not currently mapped.
            break;
        }
        default:
            break;
        }
    }
}

// ── DatasetStore ──────────────────────────────────────────────────────────────

bool DatasetStore::loadFile(const std::string& Stem,
                             const std::string& Content,
                             std::string&       OutError)
{
    try
    {
        auto Table = toml::parse(Content);

        ScalarMap Scalars;
        ArrayMap  Arrays;
        flattenTable(Table, "", Scalars, Arrays);

        m_datasets[Stem]      = std::move(Scalars);
        m_datasetArrays[Stem] = std::move(Arrays);
        return true;
    }
    catch (const toml::parse_error& E)
    {
        OutError = E.what();
        return false;
    }
}

bool DatasetStore::HasDataset(const std::string& Name) const
{
    return m_datasets.contains(Name);
}

std::optional<int> DatasetStore::GetInt(
    const std::string& Dataset, const std::string& Path) const
{
    auto It = m_datasets.find(Dataset);
    if (It == m_datasets.end()) return std::nullopt;
    auto Jt = It->second.find(Path);
    if (Jt == It->second.end()) return std::nullopt;
    if (const auto* I = std::get_if<int>(&Jt->second)) return *I;
    return std::nullopt;
}

std::optional<float> DatasetStore::GetFloat(
    const std::string& Dataset, const std::string& Path) const
{
    auto It = m_datasets.find(Dataset);
    if (It == m_datasets.end()) return std::nullopt;
    auto Jt = It->second.find(Path);
    if (Jt == It->second.end()) return std::nullopt;
    if (const auto* F = std::get_if<float>(&Jt->second)) return *F;
    return std::nullopt;
}

std::optional<std::string> DatasetStore::GetString(
    const std::string& Dataset, const std::string& Path) const
{
    auto It = m_datasets.find(Dataset);
    if (It == m_datasets.end()) return std::nullopt;
    auto Jt = It->second.find(Path);
    if (Jt == It->second.end()) return std::nullopt;
    if (const auto* S = std::get_if<std::string>(&Jt->second)) return *S;
    return std::nullopt;
}

const DatasetStore::DataTable*
DatasetStore::GetRows(const std::string& Dataset, const std::string& ArrayKey) const
{
    auto It = m_datasetArrays.find(Dataset);
    if (It == m_datasetArrays.end()) return nullptr;
    auto Jt = It->second.find(ArrayKey);
    if (Jt == It->second.end()) return nullptr;
    return &Jt->second;
}

void DatasetStore::ForEach(const std::string& Dataset, const ForEachCallback& Fn) const
{
    auto It = m_datasets.find(Dataset);
    if (It == m_datasets.end()) return;
    for (const auto& [Key, Val] : It->second)
        Fn(Key, Val);
}

bool DatasetStore::LoadFile(const std::string& VfsPath)
{
    auto* Physfs = ServiceLocator::TryGet<PhysFSLayer>();
    if (!Physfs)
    {
        logDataset("DatasetStore::LoadFile — PhysFSLayer not available");
        return false;
    }

    const auto Bytes = Physfs->ReadFile(VfsPath.c_str());
    if (Bytes.empty())
    {
        logDataset("DatasetStore::LoadFile — failed to read: " + VfsPath);
        return false;
    }

    const std::string Stem    = std::filesystem::path(VfsPath).stem().string();
    const std::string Content(reinterpret_cast<const char*>(Bytes.data()), Bytes.size());

    std::string ParseError;
    if (!loadFile(Stem, Content, ParseError))
    {
        logDataset("DatasetStore::LoadFile — TOML parse error in " + VfsPath + ": " + ParseError);
        return false;
    }

    return true;
}
