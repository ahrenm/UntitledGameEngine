#pragma once
#include "Tag.h"
#include "DataValue.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// ── SubscriptionRegistry ──────────────────────────────────────────────────────
// Unifies the two subscription flavours:
//   - per-key subscriptions
//   - prefix subscriptions
//
// A single monotonically increasing token space is shared by both flavours, so
// Unsubscribe(token) works uniformly regardless of how the subscription was made.
// Notify() dispatches a change to every exact-key match and every prefix match.
class SubscriptionRegistry
{
public:
    using ChangeCallback    = std::function<void(const Tag&, const DataValue&)>;
    using SubscriptionToken = uint32_t;

    // Subscribe to changes on an exact key.
    [[nodiscard]] SubscriptionToken Subscribe(std::string Key, ChangeCallback Cb);

    // Subscribe to changes on any key beginning with Prefix.
    [[nodiscard]] SubscriptionToken SubscribePrefix(std::string Prefix, ChangeCallback Cb);

    // Remove a subscription (exact or prefix) by token.
    void Unsubscribe(SubscriptionToken Token);

    // Dispatch a change to all matching subscribers.
    void Notify(const Tag& Key, const DataValue& Val) const;

private:
    struct KeySubscription
    {
        std::string       key;
        ChangeCallback    callback;
        SubscriptionToken token = 0;
    };

    struct PrefixSubscription
    {
        std::string       prefix;
        ChangeCallback    callback;
        SubscriptionToken token = 0;
    };

    std::vector<KeySubscription>    m_keySubs;
    std::vector<PrefixSubscription> m_prefixSubs;
    SubscriptionToken               m_nextToken = 1;
};
