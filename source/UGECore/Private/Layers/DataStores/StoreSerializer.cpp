#include <Layers/DataStores/StoreSerializer.h>
#include "../../../Public/toml.hpp"
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>

#include <fstream>
#include <string>
#include <variant>
#include <vector>

static void logSerializer(const std::string& Msg)
{
    if (auto* L = ServiceLocator::TryGet<LoggingLayer>())
        L->Log(Msg);
}

// ── Save helpers ──────────────────────────────────────────────────────────────

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

// ── Load helpers ──────────────────────────────────────────────────────────────

// Recursively flatten a toml::table into dot-notation Set() calls on Store.
static void flattenIntoStore(const toml::table& Table,
                             const std::string& Prefix,
                             RuntimeStore&      Store)
{
    for (auto&& [K, V] : Table)
    {
        const std::string FullKey = Prefix.empty()
            ? std::string(K.str())
            : Prefix + "." + std::string(K.str());

        switch (V.type())
        {
        case toml::node_type::integer:
            Store.Set(FullKey, static_cast<int>(V.ref<int64_t>()));
            break;
        case toml::node_type::floating_point:
            Store.Set(FullKey, static_cast<float>(V.ref<double>()));
            break;
        case toml::node_type::string:
            Store.Set(FullKey, std::string(V.ref<std::string>()));
            break;
        case toml::node_type::boolean:
            Store.Set(FullKey, V.ref<bool>() ? 1 : 0);
            break;
        case toml::node_type::table:
            flattenIntoStore(*V.as_table(), FullKey, Store);
            break;
        default:
            break;
        }
    }
}

// ── StoreSerializer ─────────────────────────────────────────────────

bool StoreSerializer::Save(const RuntimeStore& Store,
                                     const char*         FilePath) const
{
    toml::table Root;

    Store.ForEach([&](const std::string& Key, const AppStateValue& Val)
    {
        const auto Parts = splitKey(Key);

        // Navigate/create all intermediate tables.
        toml::table* Tbl = &Root;
        for (size_t I = 0; I + 1 < Parts.size(); ++I)
            Tbl = &ensureTable(*Tbl, Parts[I]);

        // Insert the leaf value, widening to toml native types.
        std::visit([&](const auto& V)
        {
            using T = std::decay_t<decltype(V)>;
            if constexpr (std::is_same_v<T, int>)
                Tbl->insert_or_assign(Parts.back(), static_cast<int64_t>(V));
            else if constexpr (std::is_same_v<T, float>)
                Tbl->insert_or_assign(Parts.back(), static_cast<double>(V));
            else
                Tbl->insert_or_assign(Parts.back(), V);
        }, Val);
    });

    std::ofstream Out(FilePath);
    if (!Out)
    {
        logSerializer(std::string("StoreSerializer::Save — could not open for writing: ")
                      + FilePath);
        return false;
    }

    Out << Root;
    return Out.good();
}

bool StoreSerializer::Load(RuntimeStore& Store,
                                     const char*   FilePath) const
{
    std::ifstream In(FilePath);
    if (!In)
    {
        logSerializer(std::string("StoreSerializer::Load — could not open file: ")
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
        logSerializer(std::string("StoreSerializer::Load — TOML parse error in ")
                      + FilePath + ": " + E.what());
        return false;
    }
}


