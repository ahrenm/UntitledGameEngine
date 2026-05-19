#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class SceneObject;
struct SDL_Renderer;
struct SDL_Window;

// ── SceneRegistry ─────────────────────────────────────────────────────────────
// Singleton mapping scene name strings to Scene factory functions.
// Factories receive the renderer and window at instantiation time.
class SceneRegistry
{
public:
    using Factory = std::function<std::unique_ptr<SceneObject>(SDL_Renderer*, SDL_Window*)>;

    static SceneRegistry& Instance();

    void RegisterFactory(std::string_view SceneName, Factory FactoryFn);
    [[nodiscard]] bool HasFactory(std::string_view SceneName) const;
    [[nodiscard]] std::unique_ptr<SceneObject> Create(std::string_view SceneName,
                                                SDL_Renderer*    Renderer,
                                                SDL_Window*      Window) const;

private:
    std::unordered_map<std::string, Factory> m_factories;
};

// ── REGISTER_SCENE ────────────────────────────────────────────────────────────
// Place once inside a public: section of the class body (header) to register
// against a scene name. Injects a static inline member whose constructor fires
// at DLL load time, before main() / SDLLayer::LoadScene().
//
// REQUIREMENT: must be in a public: section — see REGISTER_VIEWMODEL for the
// reasoning (static inline member visibility under implicit instantiation rules).
//
// Example:
//   class MyScene : public Scene {
//   public:
//       REGISTER_SCENE("my-scene", MyScene)
//       MyScene(SDL_Renderer* R, SDL_Window* W) : Scene(R, W) {}
//       void Tick(float DeltaTime) override;
//   };
#define REGISTER_SCENE(SceneName, Type)                                          \
    struct _SceneRegistrar {                                                      \
        _SceneRegistrar() {                                                       \
            SceneRegistry::Instance().RegisterFactory(                            \
                SceneName,                                                        \
                [](SDL_Renderer* R, SDL_Window* W) {                             \
                    return std::make_unique<Type>(R, W);                          \
                });                                                               \
        }                                                                         \
    };                                                                            \
    static inline _SceneRegistrar _s_sceneReg{};

