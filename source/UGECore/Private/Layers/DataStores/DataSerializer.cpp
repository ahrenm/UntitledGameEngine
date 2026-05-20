#include <Layers/DataStores/DataSerializer.h>
#include <Layers/DataStores/DataValue.h>
#include "../../../Public/toml.hpp"
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

static void logSerializer(const std::string& Msg)
{
    if (auto* L = ServiceLocator::TryGet<LoggingLayer>())
        L->Log(Msg);
}

// ── Save helpers ────────────────────────────────────────────────────────────────

// Navigate to (or create) a toml::table child under Parent keyed by Key.
static toml::table& ensureTable(toml::table& Parent, const std::string& Key)
{
    if (!Parent.contains(Key) || !Parent.at(Key).is_table())
        Parent.insert_or_assign(Key, toml::table{});
    return *Parent.at(Key).as_table();
}

// Split a dot-notation key into its segment parts.
static std::vector<std::string> splitKey(const std::string& Key)
{
    std::vector<std::string> Parts;
    std::string_view         Sv = Key;
    while (true)
    {
        const auto Pos = Sv.find('.');
        if (Pos == std::string_view::npos)
        {
            Parts.emplace_back(Sv);
            break;
        }
        Parts.emplace_back(Sv.substr(0, Pos));
        Sv = Sv.substr(Pos + 1);
    }
    return Parts;
}

// ── Load helpers ────────────────────────────────────────────────────────────────

// Recursively flatten a toml::table into dot-notation Set() calls on Store.
static void flattenIntoStore(const toml::table& Table,
                             const std::string& Prefix,
                             DataStore&         Store)
{
    for (auto&& [K, V] : Table)
    {
        const std::string FullKey = Prefix.empty()
            ? std::string(K.str())
            : Prefix + "." + std::string(K.str());

        if (V.is_table())
        {
            flattenIntoStore(*V.as_table(), FullKey, Store);
            continue;
        }

        DataValue Value = MakeDataValueFromToml(V);
        if (Value.HasValue())
            Store.Set(FullKey, std::move(Value),
                      DataMeta{ .Serialize = true, .Source = DataSource::Serialized });
    }
}

// ── DataSerializer ──────────────────────────────────────────────────────────────

bool DataSerializer::Save(const DataStore& Store, const char* FilePath) const
{
    auto& Registry = ValueConverterRegistry::Instance();
    toml::table Root;

    Store.ForEach([&](const Tag& Key, const DataEntry& Entry)
    {
        if (!Entry.Meta.Serialize)
            return;

        const ValueConverter* Conv = Registry.Find(Entry.Value.Type());
        if (!Conv || !Conv->ToToml)
        {
            logSerializer("DataSerializer::Save — no ToToml converter for key '"
                          + Key.Str() + "' (type " + Entry.Value.Type().name() + "), skipped");
            return;
        }

        const auto Parts = splitKey(Key.Str());

        // Navigate/create all intermediate tables.
        toml::table* Tbl = &Root;
        for (size_t I = 0; I + 1 < Parts.size(); ++I)
            Tbl = &ensureTable(*Tbl, Parts[I]);

        Conv->ToToml(*Tbl, Parts.back(), Entry.Value.Any());
    });

    std::ofstream Out(FilePath);
    if (!Out)
    {
        logSerializer(std::string("DataSerializer::Save — could not open for writing: ")
                      + FilePath);
        return false;
    }

    Out << Root;
    return Out.good();
}

bool DataSerializer::Load(DataStore& Store, const char* FilePath) const
{
    std::ifstream In(FilePath);
    if (!In)
    {
        logSerializer(std::string("DataSerializer::Load — could not open file: ")
                      + FilePath);
        return false;
    }

    const std::string Content(std::istreambuf_iterator<char>(In),
                               std::istreambuf_iterator<char>{});

    try
    {
        auto Table = toml::parse(Content);
        flattenIntoStore(Table, "", Store);
        return true;
    }
    catch (const toml::parse_error& E)
    {
        logSerializer(std::string("DataSerializer::Load — TOML parse error in ")
                      + FilePath + ": " + E.what());
        return false;
    }
}
