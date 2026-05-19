#pragma once
#include <algorithm>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class AppLayer;

// ── LayerRegistry ─────────────────────────────────────────────────────────────
// Singleton mapping layer name strings to AppLayer factory functions.
// Factories return std::expected so creation failures propagate cleanly to
// UGEApplication::Create(), matching the existing layer creation contract.
//
// Each entry carries a LoadOrder float. Names() returns names sorted ascending
// by LoadOrder so UGEApplication can push plugin layers in a deterministic,
// user-controlled sequence.
//
// Intended for plugin-provided layers that have no special construction
// arguments and can be pushed automatically during application startup.
// Core engine layers (SDLLayer, RmlUILayer, etc.) are wired explicitly in
// UGEApplication::Create() because they have inter-layer dependencies.
class LayerRegistry
{
public:
    using Factory = std::function<std::expected<std::unique_ptr<AppLayer>, std::string>()>;

    static LayerRegistry& Instance();

    void RegisterFactory(std::string_view LayerName, float LoadOrder, Factory FactoryFn);
    [[nodiscard]] bool  HasFactory(std::string_view LayerName)  const;
    [[nodiscard]] float LoadOrderOf(std::string_view LayerName) const;
    [[nodiscard]] std::expected<std::unique_ptr<AppLayer>, std::string>
                        Create(std::string_view LayerName)      const;

    // Returns all registered layer names sorted ascending by LoadOrder.
    [[nodiscard]] std::vector<std::string> Names() const;

private:
    struct Entry
    {
        Factory factory;
        float   loadOrder = 0.0f;
    };
    std::unordered_map<std::string, Entry> m_entries;
};

// ── REGISTER_LAYER ────────────────────────────────────────────────────────────
// Place once inside a public: section of the class body (header) to register
// against a layer name. Injects a static inline member whose constructor fires
// at DLL load time, before main() / UGEApplication::Create().
//
// REQUIREMENT: must be placed inside a public: access section so that the
// injected _LayerRegistrar struct and _s_layerReg member are publicly visible
// to the compiler's implicit instantiation rules. Placing it in a private:
// section causes linker issues on some compilers with static inline members.
//
// LoadOrder controls push sequence: lower values are pushed first.
// Use well-spaced values (e.g. 10.0, 20.0, 30.0) to leave room for insertion.
//
// The layer's static Create() must return:
//   std::expected<std::unique_ptr<YourLayer>, std::string>
//
// Example:
//   class MyLayer : public AppLayer {
//   public:
//       REGISTER_LAYER("my-layer", 10.0f, MyLayer)
//       static std::expected<std::unique_ptr<MyLayer>, std::string> Create();
//       ...
//   };
#define REGISTER_LAYER(LayerName, LoadOrder, Type)                               \
    struct _LayerRegistrar {                                                      \
        _LayerRegistrar() {                                                       \
            LayerRegistry::Instance().RegisterFactory(                            \
                LayerName,                                                        \
                LoadOrder,                                                        \
                []() -> std::expected<std::unique_ptr<AppLayer>, std::string> {  \
                    auto Result = Type::Create();                                 \
                    if (!Result) return std::unexpected(Result.error());          \
                    return std::move(*Result);                                    \
                });                                                               \
        }                                                                         \
    };                                                                            \
    static inline _LayerRegistrar _s_layerReg{};
