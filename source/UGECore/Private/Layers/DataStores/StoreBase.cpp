#include <Layers/DataStores/StoreBase.h>

std::optional<int> StoreReadOnly::GetInt(const std::string& Key) const
{
    if (const auto* V = Get(Key))
        if (const auto* I = std::get_if<int>(V)) return *I;
    return std::nullopt;
}

std::optional<float> StoreReadOnly::GetFloat(const std::string& Key) const
{
    if (const auto* V = Get(Key))
        if (const auto* F = std::get_if<float>(V)) return *F;
    return std::nullopt;
}

std::optional<std::string> StoreReadOnly::GetString(const std::string& Key) const
{
    if (const auto* V = Get(Key))
        if (const auto* S = std::get_if<std::string>(V)) return *S;
    return std::nullopt;
}

