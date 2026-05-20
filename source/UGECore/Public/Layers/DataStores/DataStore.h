#pragma once
#include "Tag.h"
#include "DataEntry.h"
#include "SubscriptionRegistry.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class DataStore;

// ── RowsView ──────────────────────────────────────────────────────────────────
// Lazy view over array-of-tables data flattened into indexed sub-tags.
// DatasetLoader stores each row as "<arrayPrefix>.<index>.<field>", e.g.:
//     platformerMap.tiles.0.x       = 3.0
//     platformerMap.tiles.0.sprite  = "tile-ground"
//     platformerMap.tiles.1.x       = 4.0
//
// A RowsView for "platformerMap.tiles" enumerates rows 0,1,2,… and each field
// access hits the store on demand — no snapshot is taken unless Materialize()
// is called.
class RowsView
{
public:
    // A single lazily-resolved row (e.g. "platformerMap.tiles.0").
    class Row
    {
    public:
        Row(const DataStore* Store, std::string RowPrefix)
            : m_store(Store), m_rowPrefix(std::move(RowPrefix)) {}

        // Pointer to the field's DataValue, or nullptr if absent.
        [[nodiscard]] const DataValue* Field(std::string_view Name) const;

        // Typed convenience read; std::nullopt if the field is absent or the
        // stored type does not match T.
        template <class T>
        [[nodiscard]] std::optional<T> Get(std::string_view Name) const
        {
            if (const DataValue* Val = Field(Name))
                if (const T* Ptr = Val->TryAs<T>())
                    return *Ptr;
            return std::nullopt;
        }

        [[nodiscard]] const std::string& Prefix() const noexcept { return m_rowPrefix; }

    private:
        const DataStore* m_store;
        std::string      m_rowPrefix;
    };

    // Forward iterator yielding Row values for indices 0,1,2,… up to Size().
    class Iterator
    {
    public:
        Iterator(const DataStore* Store, std::string ArrayPrefix, std::size_t Index)
            : m_store(Store), m_arrayPrefix(std::move(ArrayPrefix)), m_index(Index) {}

        Row  operator*()  const;
        Iterator& operator++() { ++m_index; return *this; }
        bool operator==(const Iterator& Other) const noexcept { return m_index == Other.m_index; }
        bool operator!=(const Iterator& Other) const noexcept { return m_index != Other.m_index; }

    private:
        const DataStore* m_store;
        std::string      m_arrayPrefix;
        std::size_t      m_index;
    };

    RowsView(const DataStore* Store, std::string ArrayPrefix)
        : m_store(Store), m_arrayPrefix(std::move(ArrayPrefix)) {}

    [[nodiscard]] Iterator    begin() const;
    [[nodiscard]] Iterator    end()   const;
    [[nodiscard]] std::size_t Size()  const;

    // Snapshot escape hatch — materialise into owned rows.
    using MaterialRow = std::unordered_map<std::string, DataValue>;
    [[nodiscard]] std::vector<MaterialRow> Materialize() const;

private:
    const DataStore* m_store;
    std::string      m_arrayPrefix;
};

// ── DataStore ─────────────────────────────────────────────────────────────────
// The single unified store: an unordered_map keyed by Tag (hash cached at
// construction) to DataEntry (DataValue + DataMeta).  Replaces the old
// three-store split (State / Transient / Data) with one container whose entries
// carry their own persistence policy via metadata.
//
// Threading note: all mutations funnel through Set/Remove so a future thread-safe
// build can wrap them (mutex or deferred queue) without touching call sites.
class DataStore
{
public:
    using ChangeCallback    = SubscriptionRegistry::ChangeCallback;
    using SubscriptionToken = SubscriptionRegistry::SubscriptionToken;

    // ── CRUD ──────────────────────────────────────────────────────────────────
    void                           Set(const Tag& Key, DataValue Val, DataMeta Meta = {});
    [[nodiscard]] const DataEntry* Find(const Tag& Key) const;   // nullptr if absent
    [[nodiscard]] const DataValue* Get(const Tag& Key) const;    // convenience
    [[nodiscard]] bool             Has(const Tag& Key) const;
    void                           Remove(const Tag& Key);
    DataEntry&                     operator[](const Tag& Key);    // create-if-absent

    // ── Iteration ─────────────────────────────────────────────────────────────
    void ForEach(const std::function<void(const Tag&, const DataEntry&)>& Fn) const;

    // ── Rows ──────────────────────────────────────────────────────────────────
    [[nodiscard]] RowsView RowsView(std::string_view ArrayPrefix) const;

    // ── Subscriptions (delegated to SubscriptionRegistry) ──────────────────────
    [[nodiscard]] SubscriptionToken Subscribe(const Tag& Key, ChangeCallback Cb);
    [[nodiscard]] SubscriptionToken SubscribePrefix(std::string_view Prefix, ChangeCallback Cb);
    void Unsubscribe(SubscriptionToken Token);

    // ── Maintenance ────────────────────────────────────────────────────────────
    // Manual defragmentation — rehash/shrink buckets after bulk Remove()s.
    // Call at natural boundaries (e.g. scene teardown).
    void Compact();

private:
    std::unordered_map<Tag, DataEntry, TagHash> m_entries;
    SubscriptionRegistry                        m_subs;
};
