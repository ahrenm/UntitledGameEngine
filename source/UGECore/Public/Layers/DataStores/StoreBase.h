#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>

// ── AppStateValue ─────────────────────────────────────────────────────────────
// The value type stored in the persistent and transient stores.
using AppStateValue = std::variant<int, float, std::string>;

// ── StoreReadOnly ─────────────────────────────────────────────────────────────
// Abstract read-only interface common to all key-value stores.
// Provides existence checks, typed reads, and change subscriptions.
class StoreReadOnly
{
public:
    using Value             = AppStateValue;
    using ChangeCallback    = std::function<void(const std::string& Key, const Value& Val)>;
    using SubscriptionToken = uint32_t;

    virtual ~StoreReadOnly() = default;

    // ── Existence and raw access ──────────────────────────────────────────────
    [[nodiscard]] virtual const Value* Get(const std::string& Key) const = 0;
    [[nodiscard]] virtual bool         Has(const std::string& Key) const = 0;

    // Typed convenience getters — return std::nullopt if the key is absent or
    // holds a different type.
    [[nodiscard]] std::optional<int>         GetInt(const std::string& Key)    const;
    [[nodiscard]] std::optional<float>       GetFloat(const std::string& Key)  const;
    [[nodiscard]] std::optional<std::string> GetString(const std::string& Key) const;

    // ── Iteration ─────────────────────────────────────────────────────────────
    // Visit every key/value pair in insertion-independent order.
    // Fn signature: void(const std::string& Key, const Value& Val)
    using ForEachCallback = std::function<void(const std::string& Key, const Value& Val)>;
    virtual void ForEach(ForEachCallback Fn) const = 0;

    // ── Change subscriptions ──────────────────────────────────────────────────
    // Returns a token that must be passed to Unsubscribe() when done.
    [[nodiscard]] virtual SubscriptionToken Subscribe(const std::string& Key, ChangeCallback Cb) = 0;
    virtual void Unsubscribe(SubscriptionToken Token) = 0;
};

// ── StoreReadWrite ────────────────────────────────────────────────────────────
// Extends StoreReadOnly with the ability to write values.
class StoreReadWrite : public StoreReadOnly
{
public:
    virtual void Set(const std::string& Key, Value Val) = 0;
};


