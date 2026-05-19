#pragma once
#include "StoreBase.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class UDEDataLayer;
class StoreSerializer;

// ── RuntimeStore ──────────────────────────────────────────────────────────────
// Concrete key-value store used for both the persistent (State) and transient
// (Transient) slots in UDEDataLayer.
//
// Both slots expose identical API — including Remove(). The distinction between
// persistence semantics (serialisation, session lifetime) is enforced entirely
// at the UDEDataLayer level; RuntimeStore itself is policy-free.
//
// StoreSerializer is a friend so it can read m_data during Save();
// it is only ever invoked on the State slot by UDEDataLayer::SaveState /
// LoadState — never on Transient.
class RuntimeStore final : public StoreReadWrite
{
public:
    // ── StoreReadOnly ─────────────────────────────────────────────────────────
    [[nodiscard]] const Value* Get(const std::string& Key) const override;
    [[nodiscard]] bool         Has(const std::string& Key) const override;
    [[nodiscard]] SubscriptionToken Subscribe(const std::string& Key, ChangeCallback Cb) override;
    void Unsubscribe(SubscriptionToken Token) override;
    void ForEach(ForEachCallback Fn) const override;

    // ── StoreReadWrite ────────────────────────────────────────────────────────
    void Set(const std::string& Key, Value Val) override;

    // Remove a key entirely.
    void Remove(const std::string& Key);

private:
    friend class UDEDataLayer; // wires m_onNotify for cross-store prefix dispatch

    struct Subscription
    {
        std::string       key;
        ChangeCallback    callback;
        SubscriptionToken token = 0;
    };

    std::unordered_map<std::string, Value> m_data;
    std::vector<Subscription>              m_subs;
    SubscriptionToken                      m_nextToken = 1;

    // Wired by UDEDataLayer to forward all Set() notifications to the
    // cross-store prefix subscription dispatcher.
    std::function<void(const std::string&, const Value&)> m_onNotify;

    void notify(const std::string& Key, const Value& Val);
};

