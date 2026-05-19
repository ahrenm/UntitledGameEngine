#include <Layers/DataStores/RuntimeStore.h>
#include <algorithm>

void RuntimeStore::Set(const std::string& Key, Value Val)
{
    m_data[Key] = std::move(Val);
    notify(Key, m_data[Key]);
}

void RuntimeStore::Remove(const std::string& Key)
{
    m_data.erase(Key);
}

void RuntimeStore::ForEach(ForEachCallback Fn) const
{
    for (const auto& [Key, Val] : m_data)
        Fn(Key, Val);
}

const RuntimeStore::Value* RuntimeStore::Get(const std::string& Key) const
{
    auto It = m_data.find(Key);
    return It != m_data.end() ? &It->second : nullptr;
}

bool RuntimeStore::Has(const std::string& Key) const
{
    return m_data.contains(Key);
}

RuntimeStore::SubscriptionToken
RuntimeStore::Subscribe(const std::string& Key, ChangeCallback Cb)
{
    const auto Token = m_nextToken++;
    m_subs.push_back({ Key, std::move(Cb), Token });
    return Token;
}

void RuntimeStore::Unsubscribe(SubscriptionToken Token)
{
    std::erase_if(m_subs, [Token](const Subscription& S) { return S.token == Token; });
}

void RuntimeStore::notify(const std::string& Key, const Value& Val)
{
    for (auto& Sub : m_subs)
        if (Sub.key == Key)
            Sub.callback(Key, Val);

    if (m_onNotify)
        m_onNotify(Key, Val);
}

