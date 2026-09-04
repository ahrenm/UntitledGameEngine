#include <Layers/DataStores/DataValue.h>
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>

#include <string>

void DataValue::logBadCast(const char* From, const char* To)
{
    if (auto* L = ServiceLocator::TryGet<LoggingLayer>())
        L->Log(std::string("DataValue::As — type mismatch: stored '") + From
               + "', requested '" + To + "'");
}

// ── ValueConverterRegistry ────────────────────────────────────────────────────

ValueConverterRegistry& ValueConverterRegistry::Instance()
{
    static ValueConverterRegistry Registry;
    return Registry;
}

const ValueConverter* ValueConverterRegistry::Find(const std::type_info& Type) const
{
    return Find(std::type_index(Type));
}

const ValueConverter* ValueConverterRegistry::Find(std::type_index Type) const
{
    auto It = m_converters.find(Type);
    return It != m_converters.end() ? &It->second : nullptr;
}

ValueConverterRegistry::ValueConverterRegistry()
{
    // ── int ───────────────────────────────────────────────────────────────────
    Register<int>(ValueConverter{
        .FromToml = [](const toml::node& Node) -> std::any
        {
            if (auto V = Node.value<int64_t>()) return static_cast<int>(*V);
            return {};
        },
        .ToToml = [](toml::table& Table, const std::string& Key, const std::any& Val)
        {
            Table.insert_or_assign(Key, static_cast<int64_t>(std::any_cast<int>(Val)));
        },
        .FromLua = [](const sol::object& Obj) -> std::any
        {
            return Obj.as<int>();
        },
        .ToLua = [](sol::this_state State, const std::any& Val) -> sol::object
        {
            return sol::make_object(State, std::any_cast<int>(Val));
        },
    });

    // ── float ──────────────────────────────────────────────────────────────────
    Register<float>(ValueConverter{
        .FromToml = [](const toml::node& Node) -> std::any
        {
            if (auto V = Node.value<double>()) return static_cast<float>(*V);
            return {};
        },
        .ToToml = [](toml::table& Table, const std::string& Key, const std::any& Val)
        {
            Table.insert_or_assign(Key, static_cast<double>(std::any_cast<float>(Val)));
        },
        .FromLua = [](const sol::object& Obj) -> std::any
        {
            return Obj.as<float>();
        },
        .ToLua = [](sol::this_state State, const std::any& Val) -> sol::object
        {
            return sol::make_object(State, std::any_cast<float>(Val));
        },
    });

    // ── std::string ─────────────────────────────────────────────────────────────
    Register<std::string>(ValueConverter{
        .FromToml = [](const toml::node& Node) -> std::any
        {
            if (auto V = Node.value<std::string>()) return *V;
            return {};
        },
        .ToToml = [](toml::table& Table, const std::string& Key, const std::any& Val)
        {
            Table.insert_or_assign(Key, std::any_cast<std::string>(Val));
        },
        .FromLua = [](const sol::object& Obj) -> std::any
        {
            return Obj.as<std::string>();
        },
        .ToLua = [](sol::this_state State, const std::any& Val) -> sol::object
        {
            return sol::make_object(State, std::any_cast<std::string>(Val));
        },
    });

    // ── Vec2 ─────────────────────────────────────────────────────────────────────
    // Atomic two-component value.  TOML representation is an inline sub-table
    //   key = { x = <float>, y = <float> }
    // and the Lua representation is a table { x = <n>, y = <n> }.
    Register<Vec2>(ValueConverter{
        .FromToml = [](const toml::node& Node) -> std::any
        {
            if (const auto* Tbl = Node.as_table())
            {
                Vec2 V;
                V.X = static_cast<float>((*Tbl)["x"].value_or(0.0));
                V.Y = static_cast<float>((*Tbl)["y"].value_or(0.0));
                return V;
            }
            return {};
        },
        .ToToml = [](toml::table& Table, const std::string& Key, const std::any& Val)
        {
            const auto& V = std::any_cast<const Vec2&>(Val);
            toml::table Sub;
            Sub.insert_or_assign("x", static_cast<double>(V.X));
            Sub.insert_or_assign("y", static_cast<double>(V.Y));
            Table.insert_or_assign(Key, std::move(Sub));
        },
        .FromLua = [](const sol::object& Obj) -> std::any
        {
            if (Obj.is<sol::table>())
            {
                sol::table T = Obj.as<sol::table>();
                Vec2 V;
                V.X = T.get_or("x", T.get_or(1, 0.0f));
                V.Y = T.get_or("y", T.get_or(2, 0.0f));
                return V;
            }
            return {};
        },
        .ToLua = [](sol::this_state State, const std::any& Val) -> sol::object
        {
            const auto& V = std::any_cast<const Vec2&>(Val);
            sol::state_view Lua(State);
            sol::table T = Lua.create_table();
            T["x"] = V.X;
            T["y"] = V.Y;
            return T;
        },
    });
}

// ── MakeDataValueFromToml ───────────────────────────────────────────────────────

DataValue MakeDataValueFromToml(const toml::node& Node)
{
    auto& Registry = ValueConverterRegistry::Instance();

    switch (Node.type())
    {
    case toml::node_type::integer:
        if (const auto* Conv = Registry.Find(typeid(int)))
            return DataValue::FromAny(Conv->FromToml(Node));
        break;
    case toml::node_type::floating_point:
        if (const auto* Conv = Registry.Find(typeid(float)))
            return DataValue::FromAny(Conv->FromToml(Node));
        break;
    case toml::node_type::string:
        if (const auto* Conv = Registry.Find(typeid(std::string)))
            return DataValue::FromAny(Conv->FromToml(Node));
        break;
    case toml::node_type::boolean:
        // No dedicated bool converter — widen to int.
        return DataValue(Node.value_or<bool>(false) ? 1 : 0);
    default:
        break;
    }

    return {};
}
