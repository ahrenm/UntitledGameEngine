#include <Layers/DataStores/SubscriptionRegistry.h>
#include <algorithm>

SubscriptionRegistry::SubscriptionToken
SubscriptionRegistry::Subscribe(std::string Key, ChangeCallback Cb)
{
    const auto Token = m_nextToken++;
    m_keySubs.push_back({ std::move(Key), std::move(Cb), Token });
    return Token;
}

SubscriptionRegistry::SubscriptionToken
SubscriptionRegistry::SubscribePrefix(std::string Prefix, ChangeCallback Cb)
{
    const auto Token = m_nextToken++;
    m_prefixSubs.push_back({ std::move(Prefix), std::move(Cb), Token });
    return Token;
}

void SubscriptionRegistry::Unsubscribe(SubscriptionToken Token)
{
    std::erase_if(m_keySubs,
        [Token](const KeySubscription& S) { return S.token == Token; });
    std::erase_if(m_prefixSubs,
        [Token](const PrefixSubscription& S) { return S.token == Token; });
}

void SubscriptionRegistry::Notify(const Tag& Key, const DataValue& Val) const
{
    const std::string& KeyStr = Key.Str();

    for (const auto& Sub : m_keySubs)
        if (Sub.key == KeyStr)
            Sub.callback(Key, Val);

    for (const auto& Sub : m_prefixSubs)
        if (KeyStr.starts_with(Sub.prefix))
            Sub.callback(Key, Val);
}
