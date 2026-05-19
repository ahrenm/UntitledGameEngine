#include <ViewModelRegistry.h>
#include <GameClasses/ViewModel.h>

ViewModelRegistry& ViewModelRegistry::Instance()
{
    static ViewModelRegistry Inst;
    return Inst;
}

void ViewModelRegistry::RegisterFactory(std::string_view ModelName, Factory FactoryFn)
{
    m_factories.emplace(std::string(ModelName), std::move(FactoryFn));
}

bool ViewModelRegistry::HasFactory(std::string_view ModelName) const
{
    return m_factories.contains(std::string(ModelName));
}

std::unique_ptr<ViewModel> ViewModelRegistry::Create(std::string_view ModelName) const
{
    const auto It = m_factories.find(std::string(ModelName));
    if (It == m_factories.end()) return nullptr;
    return It->second();
}

