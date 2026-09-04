#include <Layers/UGEDataLayer.h>
#include <Layers/DataStores/DatasetLoader.h>
#include <Layers/DataStores/DataValue.h>
#include <Layers/PhysFSLayer.h>
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>
#include <format>
#include <string>
#include <utility>

// ── UGEDataLayer ──────────────────────────────────────────────────────────────

UGEDataLayer::UGEDataLayer() = default;

UGEDataLayer::~UGEDataLayer() = default;

std::expected<std::unique_ptr<UGEDataLayer>, std::string> UGEDataLayer::Create()
{
    auto Layer = std::unique_ptr<UGEDataLayer>(new UGEDataLayer());

    auto* Physfs = ServiceLocator::TryGet<PhysFSLayer>();
    if (!Physfs)
        return std::unexpected("UGEDataLayer::Create — PhysFSLayer is not registered");

    // Populate Store from every *.toml under assets/DATA/ (Source = TomlDataset).
    DatasetLoader::LoadAll(Layer->Store, "assets/DATA");

    return Layer;
}

// ── Prefix subscriptions ────────────────────────────────────────────────────────

UGEDataLayer::SubscriptionToken
UGEDataLayer::SubscribePrefix(const std::string& Prefix, ChangeCallback Cb)
{
    return Store.SubscribePrefix(Prefix, std::move(Cb));
}

void UGEDataLayer::UnsubscribePrefix(SubscriptionToken Token)
{
    Store.Unsubscribe(Token);
}

// ── Serialisation ───────────────────────────────────────────────────────────────

bool UGEDataLayer::Save(const char* FilePath)
{
    return m_serializer.Save(Store, FilePath);
}

bool UGEDataLayer::Load(const char* FilePath)
{
    return m_serializer.Load(Store, FilePath);
}

// ── Deferred write-queue (§8) ────────────────────────────────────────────────────

void UGEDataLayer::QueueSet(Tag Key, DataValue Val, DataMeta Meta)
{
    m_writeQueue.push_back(QueuedWrite{ std::move(Key), std::move(Val), std::move(Meta), false });
}

void UGEDataLayer::QueueRemove(Tag Key)
{
    m_writeQueue.push_back(QueuedWrite{ std::move(Key), DataValue{}, DataMeta{}, true });
}

void UGEDataLayer::Update()
{
    if (m_writeQueue.empty())
        return;

    // Drain on the main thread at the frame sync point.  Swap first so callbacks
    // fired by Set() that enqueue further writes are deferred to the next frame
    // rather than mutating the container mid-iteration.
    std::vector<QueuedWrite> Pending;
    Pending.swap(m_writeQueue);

    for (auto& Cmd : Pending)
    {
        if (Cmd.remove)
            Store.Remove(Cmd.key);
        else
            Store.Set(Cmd.key, std::move(Cmd.value), std::move(Cmd.meta));
    }
}

// ── ScriptableObject ─────────────────────────────────────────────────────────────

void UGEDataLayer::RegisterObject(sol::state& Lua)
{
    auto dataTable = Lua.create_named_table("Data");

    // ── Data.Set(key, value) ─────────────────────────────────────────────────
    // If the key already exists the incoming Lua value is coerced to the current
    // backing type (int / float / string) and the entry's meta is preserved.
    // If the key is absent the type is inferred and default (transient) meta used.
    dataTable.set_function("Set",
        [this](const std::string& Key, sol::object Value)
        {
            const DataEntry* Existing = Store.Find(Key);

            auto logMismatch = [&](std::string_view Expected)
            {
                Log(std::format("Data.Set: type mismatch for '{}' — expected {}, got {}",
                    Key, Expected, sol::type_name(Value.lua_state(), Value.get_type())));
            };

            // Preserve existing meta on overwrite; default (transient) for new keys.
            const DataMeta Meta = Existing ? Existing->Meta : DataMeta{};

            if (Existing)
            {
                const DataValue& Cur = Existing->Value;

                if (Cur.TryAs<int>())
                {
                    if (Value.is<int>())
                        Store.Set(Key, DataValue{Value.as<int>()}, Meta);
                    else if (Value.is<double>())
                        Store.Set(Key, DataValue{static_cast<int>(Value.as<double>())}, Meta);
                    else
                        logMismatch("int");
                }
                else if (Cur.TryAs<float>())
                {
                    if (Value.is<double>())
                        Store.Set(Key, DataValue{static_cast<float>(Value.as<double>())}, Meta);
                    else if (Value.is<int>())
                        Store.Set(Key, DataValue{static_cast<float>(Value.as<int>())}, Meta);
                    else
                        logMismatch("float");
                }
                else if (Cur.TryAs<std::string>())
                {
                    if (Value.is<std::string>())
                        Store.Set(Key, DataValue{Value.as<std::string>()}, Meta);
                    else
                        logMismatch("string");
                }
                else if (Cur.TryAs<Vec2>())
                {
                    // Marshal a Lua table { x, y } into a Vec2 via its converter.
                    const ValueConverter* Conv =
                        ValueConverterRegistry::Instance().Find(typeid(Vec2));
                    if (Value.is<sol::table>() && Conv && Conv->FromLua)
                    {
                        std::any Any = Conv->FromLua(Value);
                        if (Any.has_value())
                            Store.Set(Key, DataValue::FromAny(std::move(Any)), Meta);
                        else
                            logMismatch("vec2");
                    }
                    else
                        logMismatch("vec2");
                }
                else
                {
                    Log(std::format("Data.Set: '{}' holds a non-scalar type ({}); "
                        "cannot coerce from Lua", Key, Cur.Type().name()));
                }
            }
            else
            {
                // New key — infer type from the Lua value.
                if (Value.is<std::string>())
                    Store.Set(Key, DataValue{Value.as<std::string>()}, Meta);
                else if (Value.is<int>())
                    Store.Set(Key, DataValue{Value.as<int>()}, Meta);
                else if (Value.is<double>())
                    Store.Set(Key, DataValue{static_cast<float>(Value.as<double>())}, Meta);
                else if (Value.is<sol::table>())
                {
                    // A table with x/y fields infers to a Vec2.
                    const ValueConverter* Conv =
                        ValueConverterRegistry::Instance().Find(typeid(Vec2));
                    std::any Any = (Conv && Conv->FromLua) ? Conv->FromLua(Value) : std::any{};
                    if (Any.has_value())
                        Store.Set(Key, DataValue::FromAny(std::move(Any)), Meta);
                    else
                        Log(std::format("Data.Set: table value for '{}' is not a Vec2 "
                            "(expected {{ x, y }})", Key));
                }
                else
                    Log(std::format("Data.Set: unsupported value type for '{}' ({})",
                        Key, sol::type_name(Value.lua_state(), Value.get_type())));
            }
        });

    // ── Data.Get(key) ────────────────────────────────────────────────────────
    // Returns the stored value marshalled to Lua via the type's ValueConverter.
    // Returns nil if the key is absent or its type has no registered converter.
    dataTable.set_function("Get",
        [this](const std::string& Key, sol::this_state S) -> sol::object
        {
            const DataValue* Val = Store.Get(Key);
            if (!Val || !Val->HasValue())
                return sol::make_object(S, sol::nil);

            const ValueConverter* Conv =
                ValueConverterRegistry::Instance().Find(Val->Type());
            if (!Conv || !Conv->ToLua)
                return sol::make_object(S, sol::nil);

            return Conv->ToLua(S, Val->Any());
        });

    // ── Data.Show() ──────────────────────────────────────────────────────────
    // Logs every entry in the store with its value and provenance.
    dataTable.set_function("Show",
        [this]()
        {
            auto sourceLabel = [](DataSource Src) -> const char*
            {
                switch (Src)
                {
                case DataSource::TomlDataset:      return "dataset";
                case DataSource::Config:           return "config";
                case DataSource::RuntimeTransient: return "transient";
                case DataSource::Serialized:       return "serialized";
                default:                           return "unknown";
                }
            };

            std::vector<std::string> Lines;
            Store.ForEach([&](const Tag& Key, const DataEntry& Entry)
            {
                const DataValue& V = Entry.Value;
                std::string ValStr;
                if (const int* I = V.TryAs<int>())
                    ValStr = std::format("(int) {}", *I);
                else if (const float* F = V.TryAs<float>())
                    ValStr = std::format("(float) {}", *F);
                else if (const std::string* Str = V.TryAs<std::string>())
                    ValStr = std::format("(string) \"{}\"", *Str);
                else if (const Vec2* Vec = V.TryAs<Vec2>())
                    ValStr = std::format("(vec2) ({}, {})", Vec->X, Vec->Y);
                else
                    ValStr = std::format("({})", V.HasValue() ? V.Type().name() : "empty");

                Lines.push_back(std::format("  {} = {}  [{}]",
                    Key.Str(), ValStr, sourceLabel(Entry.Meta.Source)));
            });

            if (Lines.empty())
            {
                Log("[Data] store is empty");
                return;
            }

            Log("[Data] store (" + std::to_string(Lines.size()) + " entries):");
            for (const auto& Line : Lines)
                Log(Line);
        });
}

void UGEDataLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}
