#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

// ── Tag ───────────────────────────────────────────────────────────────────────
// Immutable dotted key used to address entries in a DataStore.
//
// The hash is computed once at construction from the key's string_view, so
// repeated lookups reuse the cached hash instead of re-hashing the string on
// every map access (see TagHash below).
//
// Keys remain plain dotted strings (e.g. "ui.console.visible"), so existing
// constexpr TAG_* / KEY_* constants need no changes — they convert implicitly.
class Tag
{
public:
    Tag(std::string_view Key)                       // NOLINT(google-explicit-constructor)
        : m_key(Key), m_hash(std::hash<std::string_view>{}(Key)) {}

    Tag(const char* Key) : Tag(std::string_view{Key}) {}          // NOLINT(google-explicit-constructor)
    Tag(const std::string& Key) : Tag(std::string_view{Key}) {}   // NOLINT(google-explicit-constructor)

    [[nodiscard]] std::size_t        Hash() const noexcept { return m_hash; }
    [[nodiscard]] const std::string& Str()  const noexcept { return m_key; }

    bool operator==(const Tag& Other) const noexcept
    {
        return m_hash == Other.m_hash && m_key == Other.m_key;
    }

private:
    std::string m_key;
    std::size_t m_hash;
};

// Hash functor for use as the Hash template argument of unordered_map<Tag, ...>.
struct TagHash
{
    std::size_t operator()(const Tag& Key) const noexcept { return Key.Hash(); }
};
