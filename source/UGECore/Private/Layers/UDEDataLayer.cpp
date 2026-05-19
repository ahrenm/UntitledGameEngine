#include <Layers/UDEDataLayer.h>
#include <Layers/PhysFSLayer.h>
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>
#include <algorithm>
#include <format>

// ── UDEDataLayer ──────────────────────────────────────────────────────────────

UDEDataLayer::UDEDataLayer()
{
    // Wire each store's m_onNotify to dispatch cross-store prefix subscriptions.
    auto prefixDispatch = [this](const std::string& Key, const AppStateValue& Val)
    {
        for (auto& Sub : m_prefixSubs)
            if (Key.starts_with(Sub.prefix))
                Sub.callback(Key, Val);
    };

    State.m_onNotify     = prefixDispatch;
    Transient.m_onNotify = prefixDispatch;
}

UDEDataLayer::~UDEDataLayer() = default;

std::expected<std::unique_ptr<UDEDataLayer>, std::string> UDEDataLayer::Create()
{
    auto Layer = std::unique_ptr<UDEDataLayer>(new UDEDataLayer());

    auto* Physfs = ServiceLocator::TryGet<PhysFSLayer>();
    if (!Physfs)
        return std::unexpected("UDEDataLayer::Create — PhysFSLayer is not registered");

    auto* Log = ServiceLocator::TryGet<LoggingLayer>();
    auto logMsg = [Log](const std::string& Msg) { if (Log) Log->Log(Msg); };

    const auto Files = Physfs->ListFiles("assets/DATA");
    int Loaded = 0, Skipped = 0;

    for (const auto& Path : Files)
    {
        if (!Path.ends_with(".toml")) { ++Skipped; continue; }

        if (Layer->Data.LoadFile(Path))
        {
            ++Loaded;
            logMsg("UDEDataLayer: loaded dataset from " + Path);
        }
        // LoadFile already logs parse/read errors internally.
    }

    logMsg("UDEDataLayer: " + std::to_string(Loaded) + " dataset(s) loaded, " +
           std::to_string(Skipped) + " non-toml file(s) skipped in assets/DATA/");

    return Layer;
}

UDEDataLayer::SubscriptionToken
UDEDataLayer::SubscribePrefix(const std::string& Prefix, ChangeCallback Cb)
{
    const auto Token = m_nextPrefixToken++;
    m_prefixSubs.push_back({ Prefix, std::move(Cb), Token });
    return Token;
}

void UDEDataLayer::UnsubscribePrefix(SubscriptionToken Token)
{
    std::erase_if(m_prefixSubs,
        [Token](const PrefixSubscription& S) { return S.token == Token; });
}

// ── Serialisation ─────────────────────────────────────────────────────────────

bool UDEDataLayer::SaveState(const char* FilePath)
{
    return m_serializer.Save(State, FilePath);
}

bool UDEDataLayer::LoadState(const char* FilePath)
{
    return m_serializer.Load(State, FilePath);
}

// ── ScriptableObject ──────────────────────────────────────────────────────────

void UDEDataLayer::RegisterObject(sol::state& Lua)
{
    auto dataTable = Lua.create_named_table("Data");

    // ── Data.SetTransient(key, value) ────────────────────────────────────────
    // If the key already exists the incoming Lua value is coerced to the
    // backing variant type (int / float / string); a mismatch is logged.
    // If the key is absent the type is inferred from the Lua value.
    dataTable.set_function("SetTransient",
        [this](const std::string& Key, sol::object Value)
        {
            const AppStateValue* Existing = Transient.Get(Key);

            auto logMismatch = [&](std::string_view Expected)
            {
                Log(std::format("Data.SetTransient: type mismatch for '{}' — expected {}, got {}",
                    Key, Expected, sol::type_name(Value.lua_state(), Value.get_type())));
            };

            if (Existing)
            {
                // Cast Lua value to the existing backing type.
                if (std::holds_alternative<int>(*Existing))
                {
                    if (Value.is<int>())
                        Transient.Set(Key, AppStateValue{Value.as<int>()});
                    else if (Value.is<double>())
                        Transient.Set(Key, AppStateValue{static_cast<int>(Value.as<double>())});
                    else
                        logMismatch("int");
                }
                else if (std::holds_alternative<float>(*Existing))
                {
                    if (Value.is<double>())
                        Transient.Set(Key, AppStateValue{static_cast<float>(Value.as<double>())});
                    else if (Value.is<int>())
                        Transient.Set(Key, AppStateValue{static_cast<float>(Value.as<int>())});
                    else
                        logMismatch("float");
                }
                else if (std::holds_alternative<std::string>(*Existing))
                {
                    if (Value.is<std::string>())
                        Transient.Set(Key, AppStateValue{Value.as<std::string>()});
                    else
                        logMismatch("string");
                }
            }
            else
            {
                // New key — infer type from Lua value.
                if (Value.is<std::string>())
                    Transient.Set(Key, AppStateValue{Value.as<std::string>()});
                else if (Value.is<int>())
                    Transient.Set(Key, AppStateValue{Value.as<int>()});
                else if (Value.is<double>())
                    Transient.Set(Key, AppStateValue{static_cast<float>(Value.as<double>())});
                else
                    Log(std::format("Data.SetTransient: unsupported value type for '{}' ({})",
                        Key, sol::type_name(Value.lua_state(), Value.get_type())));
            }
        });

    // ── Data.GetTransient(key) ───────────────────────────────────────────────
    // Returns the stored value as a Lua int, number, or string.
    // Returns nil if the key is absent.
    dataTable.set_function("GetTransient",
        [this](const std::string& Key, sol::this_state S) -> sol::object
        {
            const AppStateValue* Val = Transient.Get(Key);
            if (!Val)
                return sol::make_object(S, sol::nil);

            return std::visit([&](const auto& V) -> sol::object
            {
                return sol::make_object(S, V);
            }, *Val);
        });

    // ── Data.ShowTransient() ─────────────────────────────────────────────────
    // Iterates the transient store and logs every key/value pair.
    dataTable.set_function("ShowTransient",
        [this]()
        {
            // Collect entries via ForEach so we have the count up front.
            std::vector<std::pair<std::string, std::string>> Entries;
            Transient.ForEach([&](const std::string& Key, const AppStateValue& Val)
            {
                std::string ValStr = std::visit([](const auto& V) -> std::string
                {
                    using T = std::decay_t<decltype(V)>;
                    if constexpr (std::is_same_v<T, int>)
                        return std::format("(int) {}", V);
                    else if constexpr (std::is_same_v<T, float>)
                        return std::format("(float) {}", V);
                    else
                        return std::format("(string) \"{}\"", V);
                }, Val);
                Entries.emplace_back(Key, std::move(ValStr));
            });

            if (Entries.empty())
            {
                Log("[Data] Transient store is empty");
                return;
            }

            Log("[Data] Transient store (" +
                std::to_string(Entries.size()) + " entries):");

            for (const auto& [Key, ValStr] : Entries)
                Log(std::format("  {} = {}", Key, ValStr));
        });
}

void UDEDataLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}

