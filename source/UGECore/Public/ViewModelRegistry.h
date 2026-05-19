#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class ViewModel;

// ── ViewModelRegistry ─────────────────────────────────────────────────────────
// Singleton mapping data-model name strings to ViewModel factory functions.
class ViewModelRegistry
{
public:
    using Factory = std::function<std::unique_ptr<ViewModel>()>;

    static ViewModelRegistry& Instance();

    void RegisterFactory(std::string_view ModelName, Factory FactoryFn);
    [[nodiscard]] bool                       HasFactory(std::string_view ModelName) const;
    [[nodiscard]] std::unique_ptr<ViewModel> Create(std::string_view ModelName)     const;

private:
    std::unordered_map<std::string, Factory> m_factories;
};

// ── REGISTER_VIEWMODEL ────────────────────────────────────────────────────────
// Place once inside a public: section of the class body (header) to register
// against a data-model name. Injects a static inline member whose constructor
// fires at DLL load time, before main() / RmlUILayer::LoadDocument().
//
// REQUIREMENT: must be placed inside a public: access section so that the
// injected _VmRegistrar struct and _s_vmReg member are publicly visible to the
// compiler's implicit instantiation rules. Placing it in a private: section
// causes linker issues on some compilers with static inline members.
//
// Example:
//   class MyViewModel : public ViewModel {
//   public:
//       REGISTER_VIEWMODEL("my_model", MyViewModel)
//       ...
//   };
#define REGISTER_VIEWMODEL(ModelName, Type)                                      \
    struct _VmRegistrar {                                                        \
        _VmRegistrar() {                                                         \
            ViewModelRegistry::Instance().RegisterFactory(                       \
                ModelName, [] { return std::make_unique<Type>(); });             \
        }                                                                        \
    };                                                                           \
    static inline _VmRegistrar _s_vmReg{};
