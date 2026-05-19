#include <LayerRegistry.h>
#include <Layers/AppLayer.h>

LayerRegistry& LayerRegistry::Instance()
{
    static LayerRegistry Inst;
    return Inst;
}

void LayerRegistry::RegisterFactory(std::string_view LayerName, float LoadOrder, Factory FactoryFn)
{
    m_entries.insert_or_assign(std::string(LayerName),
                               Entry{ std::move(FactoryFn), LoadOrder });
}

bool LayerRegistry::HasFactory(std::string_view LayerName) const
{
    return m_entries.contains(std::string(LayerName));
}

float LayerRegistry::LoadOrderOf(std::string_view LayerName) const
{
    const auto It = m_entries.find(std::string(LayerName));
    return It != m_entries.end() ? It->second.loadOrder : 0.0f;
}

std::vector<std::string> LayerRegistry::Names() const
{
    std::vector<std::string> Result;
    Result.reserve(m_entries.size());
    for (const auto& [Name, _] : m_entries)
        Result.push_back(Name);

    std::sort(Result.begin(), Result.end(),
        [this](const std::string& A, const std::string& B) {
            return m_entries.at(A).loadOrder < m_entries.at(B).loadOrder;
        });
    return Result;
}

std::expected<std::unique_ptr<AppLayer>, std::string>
LayerRegistry::Create(std::string_view LayerName) const
{
    const auto It = m_entries.find(std::string(LayerName));
    if (It == m_entries.end())
        return std::unexpected("LayerRegistry: no factory registered for '" +
                               std::string(LayerName) + "'");
    return It->second.factory();
}
