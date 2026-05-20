#include <Layers/DataStores/DataStore.h>

#include <charconv>

// ── RowsView helpers ──────────────────────────────────────────────────────────

// Parse the row index immediately following "<ArrayPrefix>." in Key.
// Returns true and sets OutIndex + OutRemainder (the text after the index dot)
// when Key belongs to the array; false otherwise.
static bool parseRowIndex(std::string_view Key,
                          std::string_view ArrayPrefix,
                          std::size_t&     OutIndex,
                          std::string_view& OutRemainder)
{
    if (Key.size() <= ArrayPrefix.size() + 1) return false;
    if (!Key.starts_with(ArrayPrefix))        return false;
    if (Key[ArrayPrefix.size()] != '.')       return false;

    std::string_view Rest = Key.substr(ArrayPrefix.size() + 1);
    const auto Dot = Rest.find('.');
    const std::string_view IndexPart =
        Dot == std::string_view::npos ? Rest : Rest.substr(0, Dot);
    if (IndexPart.empty()) return false;

    std::size_t Index = 0;
    const auto* First = IndexPart.data();
    const auto* Last  = IndexPart.data() + IndexPart.size();
    const auto  Res   = std::from_chars(First, Last, Index);
    if (Res.ec != std::errc{} || Res.ptr != Last) return false;

    OutIndex     = Index;
    OutRemainder = Dot == std::string_view::npos
                       ? std::string_view{}
                       : Rest.substr(Dot + 1);
    return true;
}

const DataValue* RowsView::Row::Field(std::string_view Name) const
{
    std::string Key = m_rowPrefix;
    Key += '.';
    Key.append(Name);
    return m_store->Get(Key);
}

RowsView::Row RowsView::Iterator::operator*() const
{
    return Row(m_store, m_arrayPrefix + "." + std::to_string(m_index));
}

RowsView::Iterator RowsView::begin() const
{
    return Iterator(m_store, m_arrayPrefix, 0);
}

RowsView::Iterator RowsView::end() const
{
    return Iterator(m_store, m_arrayPrefix, Size());
}

std::size_t RowsView::Size() const
{
    bool        Any = false;
    std::size_t Max = 0;

    m_store->ForEach([&](const Tag& Key, const DataEntry&)
    {
        std::size_t      Index = 0;
        std::string_view Remainder;
        if (parseRowIndex(Key.Str(), m_arrayPrefix, Index, Remainder))
        {
            Any = true;
            if (Index > Max) Max = Index;
        }
    });

    return Any ? Max + 1 : 0;
}

std::vector<RowsView::MaterialRow> RowsView::Materialize() const
{
    std::vector<MaterialRow> Rows(Size());

    m_store->ForEach([&](const Tag& Key, const DataEntry& Entry)
    {
        std::size_t      Index = 0;
        std::string_view Remainder;
        if (parseRowIndex(Key.Str(), m_arrayPrefix, Index, Remainder) &&
            Index < Rows.size() && !Remainder.empty())
        {
            Rows[Index].emplace(std::string(Remainder), Entry.Value);
        }
    });

    return Rows;
}

// ── DataStore ─────────────────────────────────────────────────────────────────

void DataStore::Set(const Tag& Key, DataValue Val, DataMeta Meta)
{
    auto& Entry = m_entries[Key];
    Entry.Value = std::move(Val);
    Entry.Meta  = std::move(Meta);
    m_subs.Notify(Key, Entry.Value);
}

const DataEntry* DataStore::Find(const Tag& Key) const
{
    auto It = m_entries.find(Key);
    return It != m_entries.end() ? &It->second : nullptr;
}

const DataValue* DataStore::Get(const Tag& Key) const
{
    auto It = m_entries.find(Key);
    return It != m_entries.end() ? &It->second.Value : nullptr;
}

bool DataStore::Has(const Tag& Key) const
{
    return m_entries.contains(Key);
}

void DataStore::Remove(const Tag& Key)
{
    m_entries.erase(Key);
}

DataEntry& DataStore::operator[](const Tag& Key)
{
    return m_entries[Key];
}

void DataStore::ForEach(const std::function<void(const Tag&, const DataEntry&)>& Fn) const
{
    for (const auto& [Key, Entry] : m_entries)
        Fn(Key, Entry);
}

RowsView DataStore::RowsView(std::string_view ArrayPrefix) const
{
    return ::RowsView(this, std::string(ArrayPrefix));
}

DataStore::SubscriptionToken DataStore::Subscribe(const Tag& Key, ChangeCallback Cb)
{
    return m_subs.Subscribe(Key.Str(), std::move(Cb));
}

DataStore::SubscriptionToken DataStore::SubscribePrefix(std::string_view Prefix, ChangeCallback Cb)
{
    return m_subs.SubscribePrefix(std::string(Prefix), std::move(Cb));
}

void DataStore::Unsubscribe(SubscriptionToken Token)
{
    m_subs.Unsubscribe(Token);
}

void DataStore::Compact()
{
    m_entries.rehash(0);
}
