#pragma once
#include <any>
#include <functional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include <sol/sol.hpp>
#include "../../toml.hpp"

// ── DataValue ─────────────────────────────────────────────────────────────────
// Type-erased value holder (Option A) backing every DataStore entry.
//
// Scalars (int, float, std::string) and arbitrary user types (e.g. glm::vec3)
// are stored directly inside a std::any.  User types become first-class by
// registering a ValueConverter with the ValueConverterRegistry so TOML and Lua
// round-trips know how to marshal them.
//
// Cast contract:
//   operator T() / As<T>()  — returns the stored value; on a type mismatch it
//                             logs via LoggingLayer and throws std::bad_any_cast.
//   TryAs<T>()              — returns nullptr on mismatch and never throws.
class DataValue
{
public:
    DataValue() = default;

    template <class T>
    explicit DataValue(T Val) : m_data(std::move(Val)) {}

    // Wrap an already type-erased payload (e.g. produced by a ValueConverter's
    // FromToml / FromLua hook) into a DataValue without re-boxing it in a nested
    // std::any.  Used by DataSerializer / DatasetLoader / the Lua bridge.
    [[nodiscard]] static DataValue FromAny(std::any Any)
    {
        DataValue Val;
        Val.m_data = std::move(Any);
        return Val;
    }

    // Implicit cast back to the stored type — no GetValue() needed:
    //     glm::vec3 v = store.Get("enemy.spawn")->As<glm::vec3>();
    //     float g     = *store.Get("physics.gravity");   // operator float()
    template <class T>
    operator T() const { return As<T>(); }   // NOLINT(google-explicit-constructor)

    // Returns the stored value; logs and throws std::bad_any_cast on mismatch.
    template <class T>
    [[nodiscard]] T As() const
    {
        if (const T* Ptr = std::any_cast<T>(&m_data))
            return *Ptr;
        logBadCast(m_data.has_value() ? m_data.type().name() : "empty", typeid(T).name());
        throw std::bad_any_cast();
    }

    // Returns a pointer to the stored value, or nullptr on mismatch. Never throws.
    template <class T>
    [[nodiscard]] const T* TryAs() const noexcept { return std::any_cast<T>(&m_data); }

    [[nodiscard]] bool                  HasValue() const noexcept { return m_data.has_value(); }
    [[nodiscard]] const std::type_info& Type()     const noexcept { return m_data.type(); }
    [[nodiscard]] const std::any&       Any()      const noexcept { return m_data; }

private:
    // Non-template logging helper (implemented in DataValue.cpp) so the header
    // need not pull in LoggingLayer / ServiceLocator.
    static void logBadCast(const char* From, const char* To);

    std::any m_data;
};

// ── ValueConverter ────────────────────────────────────────────────────────────
// Marshalling hooks for a single registered type.  All four functions operate
// on the type-erased std::any payload of a DataValue.
struct ValueConverter
{
    // TOML round-trip — used by DataSerializer (save/load) and DatasetLoader.
    std::function<std::any(const toml::node&)>                         FromToml;
    std::function<void(toml::table&, const std::string&, const std::any&)> ToToml;

    // Lua round-trip — used by the UGEDataLayer Data.* bridge.
    std::function<std::any(const sol::object&)>                        FromLua;
    std::function<sol::object(sol::this_state, const std::any&)>       ToLua;
};

// ── ValueConverterRegistry ────────────────────────────────────────────────────
// Global registry mapping a C++ type to its ValueConverter.  Built-in converters
// for int, float and std::string are registered on first access.  User types are
// registered once (e.g. during plugin load or layer Create()):
//
//     ValueConverterRegistry::Instance().Register<glm::vec3>({ ... });
class ValueConverterRegistry
{
public:
    static ValueConverterRegistry& Instance();

    template <class T>
    void Register(ValueConverter Conv)
    {
        m_converters[std::type_index(typeid(T))] = std::move(Conv);
    }

    [[nodiscard]] const ValueConverter* Find(const std::type_info& Type) const;
    [[nodiscard]] const ValueConverter* Find(std::type_index Type) const;

private:
    ValueConverterRegistry();  // registers built-ins

    std::unordered_map<std::type_index, ValueConverter> m_converters;
};

// ── MakeDataValueFromToml ───────────────────────────────────────────────────────
// Build a DataValue from a TOML scalar node using the registered built-in
// converters (integer→int, floating_point→float, string→std::string) plus a
// boolean→int fallback.  Returns an empty DataValue for unsupported node types.
// Shared by DataSerializer (load) and DatasetLoader.
[[nodiscard]] DataValue MakeDataValueFromToml(const toml::node& Node);
