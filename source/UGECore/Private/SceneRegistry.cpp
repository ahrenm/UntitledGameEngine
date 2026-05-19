#include <SceneRegistry.h>
#include <GameClasses/SceneObject.h>

SceneRegistry& SceneRegistry::Instance()
{
    static SceneRegistry Inst;
    return Inst;
}

void SceneRegistry::RegisterFactory(std::string_view SceneName, Factory FactoryFn)
{
    m_factories.emplace(std::string(SceneName), std::move(FactoryFn));
}

bool SceneRegistry::HasFactory(std::string_view SceneName) const
{
    return m_factories.contains(std::string(SceneName));
}

std::unique_ptr<SceneObject> SceneRegistry::Create(std::string_view SceneName,
                                              SDL_Renderer*    Renderer,
                                              SDL_Window*      Window) const
{
    const auto It = m_factories.find(std::string(SceneName));
    if (It == m_factories.end()) return nullptr;
    return It->second(Renderer, Window);
}

