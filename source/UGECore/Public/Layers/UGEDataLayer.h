#pragma once
#include "AppLayer.h"
#include "../ServiceLocator.h"
#include "../IScriptableObject.h"
#include "../LayerRegistry.h"
#include "DataStores/DataStore.h"
#include "DataStores/DataSerializer.h"

#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ── UGEDataLayer ──────────────────────────────────────────────────────────────
// Central data layer hosting a single unified DataStore.  Every entry carries its
// own DataMeta describing persistence policy and provenance, replacing the old
// three-store split (State / Transient / Data):
//
//   - Persistent entries   → meta { Serialize = true,  Source = Serialized }
//   - Transient entries    → default meta (never serialised, runtime-only)
//   - Dataset entries       → meta { Source = TomlDataset } (loaded from assets/DATA/)
//
// Access the store directly, e.g.:
//   auto& layer = ServiceLocator::Get<UGEDataLayer>();
//   layer.Store.Set("ui.visible", DataValue{1});
//   auto gravity = layer.Store.Get("platformerData.physics.gravity")->As<float>();
//   for (const auto& Row : layer.Store.RowsView("platformerMap.tiles")) { ... }
//
// Use DATA_BIND / DATA_BIND_LOCAL_FLOAT / DATA_BIND_LOCAL_STRING for reactive
// bindings; use Store.SubscribePrefix for prefix watching.
//
// Load order: 3 — must follow PhysFSLayer (2) as Create() scans assets/DATA/ via PhysFS.
class UGEDataLayer : public AppLayer, public IScriptableObject
{
public:
    REGISTER_LAYER("ude-data", 3.0f, UGEDataLayer)

    // ── Factory ───────────────────────────────────────────────────────────────
    // Creates the layer and immediately scans assets/DATA/ via PhysFSLayer,
    // loading all *.toml files into Store.  PhysFSLayer must be registered with
    // the ServiceLocator before calling Create().
    [[nodiscard]] static std::expected<std::unique_ptr<UGEDataLayer>, std::string> Create();

    // ── Public store ──────────────────────────────────────────────────────────
    DataStore Store;

    // ── Cross-store prefix subscription ───────────────────────────────────────
    // Fires Cb for every key starting with Prefix.  Delegates to Store.
    // e.g. SubscribePrefix("ui.", cb) watches every "ui.*" key.
    using SubscriptionToken = DataStore::SubscriptionToken;
    using ChangeCallback    = DataStore::ChangeCallback;

    [[nodiscard]] SubscriptionToken SubscribePrefix(const std::string& Prefix, ChangeCallback Cb);
    void UnsubscribePrefix(SubscriptionToken Token);

    // ── Serialisation ─────────────────────────────────────────────────────────
    // Persist / restore every entry whose Meta.Serialize == true to a TOML file
    // at a real OS path.  Dot-notation keys are written as a nested TOML hierarchy.
    // Returns false and logs on I/O or parse failure.
    [[nodiscard]] bool Save(const char* FilePath);
    [[nodiscard]] bool Load(const char* FilePath);

    // ── Deferred write-queue (threading seam, §8) ─────────────────────────────
    // Set / Remove requests from non-owner contexts enqueue a command that is
    // applied on the main thread when Update() drains the queue at the frame
    // sync point.  Direct Store.Set/Remove remain available for main-thread use.
    void QueueSet(Tag Key, DataValue Val, DataMeta Meta = {});
    void QueueRemove(Tag Key);

    // ── IScriptableObject ─────────────────────────────────────────────────────
    // Exposes Data.Set(key, value), Data.Get(key) and Data.Show() to Lua.
    void RegisterObject(sol::state& Lua) override;

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void Update() override;                 // drains the deferred write-queue
    void RegisterWithServiceLocator() override;

    ~UGEDataLayer() override;

private:
    UGEDataLayer();

    struct QueuedWrite
    {
        Tag       key;
        DataValue value;
        DataMeta  meta;
        bool      remove = false;
    };

    std::vector<QueuedWrite> m_writeQueue;
    DataSerializer           m_serializer;
};


// ── DataBinding ───────────────────────────────────────────────────────────────
// RAII handle returned by DATA_BIND / DATA_BIND_LOCAL_*.
// Automatically calls Unsubscribe() when destroyed or move-assigned.
// Always store as a member variable — never as a local variable.
//
// API:
//   binding.SetValue(DataValue{...})  — write a new value (fires subscribers)
//   binding.GetValue()                — read the current value (const DataValue*)
//   binding.Release()                 — unsubscribe early
//   binding.IsActive()                — true if still subscribed
class DataBinding
{
public:
    DataBinding() = default;

    DataBinding(DataStore* Store, DataStore::SubscriptionToken Token, std::string Key)
        : m_store(Store), m_token(Token), m_key(std::move(Key)) {}

    ~DataBinding() { release(); }

    DataBinding(const DataBinding&)            = delete;
    DataBinding& operator=(const DataBinding&) = delete;

    DataBinding(DataBinding&& Other) noexcept
        : m_store(std::exchange(Other.m_store, nullptr))
        , m_token(std::exchange(Other.m_token, 0))
        , m_key(std::move(Other.m_key)) {}

    DataBinding& operator=(DataBinding&& Other) noexcept
    {
        if (this != &Other)
        {
            release();
            m_store = std::exchange(Other.m_store, nullptr);
            m_token = std::exchange(Other.m_token, 0);
            m_key   = std::move(Other.m_key);
        }
        return *this;
    }

    // Explicitly unsubscribe before destruction.
    void Release() { release(); }

    [[nodiscard]] bool IsActive() const { return m_store != nullptr && m_token != 0; }

    // Write a new value into the bound key, firing all subscribers.
    // Preserves the existing entry's meta if the key is present, otherwise uses
    // default (transient) meta.  No-op if the binding is inactive or has no key.
    void SetValue(DataValue Val)
    {
        if (!m_store || m_key.empty()) return;
        DataMeta Meta;
        if (const DataEntry* Existing = m_store->Find(m_key))
            Meta = Existing->Meta;
        m_store->Set(m_key, std::move(Val), std::move(Meta));
    }

    // Read the current value from the bound key.
    // Returns nullptr if the binding is inactive, has no key, or the key is absent.
    [[nodiscard]] const DataValue* GetValue() const
    {
        if (!m_store || m_key.empty()) return nullptr;
        return m_store->Get(m_key);
    }

private:
    void release()
    {
        // Guard against accessing a destroyed store: ServiceLocator::Clear() runs
        // before m_layers tears down, so TryGet returns nullptr once the layer
        // is deregistered — any binding outliving the layer is a safe no-op.
        if (m_store && m_token && ServiceLocator::TryGet<UGEDataLayer>())
            m_store->Unsubscribe(m_token);
        m_store = nullptr;
        m_token = 0;
    }

    DataStore*                   m_store = nullptr;
    DataStore::SubscriptionToken m_token = 0;
    std::string                  m_key;
};


// ── DATA_BIND target selectors ────────────────────────────────────────────────
// The optional trailing Target argument of the DATA_BIND* macros expands to one
// of these helpers.  Persistent (default) tags entries for serialisation;
// Transient leaves default (runtime-only) meta.
namespace DataBindTarget
{
    inline DataMeta Persistent() { return DataMeta{ .Serialize = true, .Source = DataSource::Serialized }; }
    inline DataMeta Transient()  { return DataMeta{}; }
}


// ── DATA_BIND ─────────────────────────────────────────────────────────────────
// Injects Key_ with InitialValue_ if absent (using the Target_ meta), subscribes
// Callback_, and returns a DataBinding that auto-unsubscribes on destruction.
//
// Callback_ signature: void(const Tag& Key, const DataValue& Val)
//
// Target_ (optional, defaults to Persistent) selects the seed meta:
//   Persistent — { Serialize = true, Source = Serialized }
//   Transient  — default meta (runtime-only)
//
// Example:
//   DataBinding m_binding = DATA_BIND(
//       "ui.console.visible", 1,
//       [this](const Tag&, const DataValue& Val) {
//           m_visible = Val.As<int>() != 0;
//           m_model.DirtyVariable("panel_visible");
//       });
//   DataBinding m_dragBinding = DATA_BIND(
//       "ui.console.dragging", 0, cb, Transient);
//
// Returns an empty (no-op) DataBinding if UGEDataLayer is not yet registered.
#define DATA_BIND_IMPL(Key_, InitialValue_, Callback_, Target_)                    \
    [&]() -> DataBinding {                                                         \
        auto* _Layer = ServiceLocator::TryGet<UGEDataLayer>();                     \
        if (!_Layer) return {};                                                    \
        auto& _S = _Layer->Store;                                                  \
        if (!_S.Has(Key_))                                                         \
            _S.Set((Key_), DataValue{InitialValue_}, DataBindTarget::Target_());   \
        return DataBinding(&_S, _S.Subscribe((Key_), (Callback_)), std::string(Key_));        \
    }()

// Overload-by-arg-count: allow an optional trailing Target argument.
#define DATA_BIND_GET(_1, _2, _3, _4, NAME, ...) NAME
#define DATA_BIND(...) \
    DATA_BIND_GET(__VA_ARGS__, DATA_BIND_4, DATA_BIND_3)(__VA_ARGS__)
#define DATA_BIND_3(Key_, InitialValue_, Callback_) \
    DATA_BIND_IMPL(Key_, InitialValue_, Callback_, Persistent)
#define DATA_BIND_4(Key_, InitialValue_, Callback_, Target_) \
    DATA_BIND_IMPL(Key_, InitialValue_, Callback_, Target_)


// ── DATA_BIND_LOCAL_FLOAT ─────────────────────────────────────────────────────
// Binds a float key directly to a local float member: subscribes, then seeds
// Member_ from the current stored value immediately so it is valid without a
// SetValue call.
//
// Binding_ : DataBinding lvalue to assign into
// Key_     : std::string_view / const char* / std::string tag
// Member_  : float lvalue to keep in sync
// Target_  : optional (Persistent default / Transient)
//
// Example:
//   DATA_BIND_LOCAL_FLOAT(m_gravityBinding, TAG_GRAVITY, m_gravity, Transient);
#define DATA_BIND_LOCAL_FLOAT_IMPL(Binding_, Key_, Member_, Target_)               \
    do {                                                                            \
        (Binding_) = DATA_BIND_IMPL((Key_), 0.0f,                                  \
            [&](const Tag&, const DataValue& lfVal)                               \
            {                                                                       \
                if (const float* lfF = lfVal.TryAs<float>()) (Member_) = *lfF;     \
            }, Target_);                                                            \
        if (const DataValue* lfV = (Binding_).GetValue())                         \
            if (const float* lfF = lfV->TryAs<float>()) (Member_) = *lfF;          \
    } while(0)

#define DATA_BIND_LOCAL_FLOAT_GET(_1, _2, _3, _4, NAME, ...) NAME
#define DATA_BIND_LOCAL_FLOAT(...) \
    DATA_BIND_LOCAL_FLOAT_GET(__VA_ARGS__, DATA_BIND_LOCAL_FLOAT_4, DATA_BIND_LOCAL_FLOAT_3)(__VA_ARGS__)
#define DATA_BIND_LOCAL_FLOAT_3(Binding_, Key_, Member_) \
    DATA_BIND_LOCAL_FLOAT_IMPL(Binding_, Key_, Member_, Persistent)
#define DATA_BIND_LOCAL_FLOAT_4(Binding_, Key_, Member_, Target_) \
    DATA_BIND_LOCAL_FLOAT_IMPL(Binding_, Key_, Member_, Target_)


// ── DATA_BIND_LOCAL_STRING ────────────────────────────────────────────────────
// Binds a string key directly to a local std::string member: subscribes, then
// seeds Member_ from the current stored value immediately.
//
// Binding_ : DataBinding lvalue to assign into
// Key_     : const char* / std::string tag
// Member_  : std::string lvalue to keep in sync
// Target_  : optional (Persistent default / Transient)
//
// Example:
//   DATA_BIND_LOCAL_STRING(m_labelBinding, "ui.label", m_label, Transient);
#define DATA_BIND_LOCAL_STRING_IMPL(Binding_, Key_, Member_, Target_)              \
    do {                                                                            \
        (Binding_) = DATA_BIND_IMPL((Key_), std::string{},                        \
            [&](const Tag&, const DataValue& lsVal)                               \
            {                                                                       \
                if (const std::string* lsS = lsVal.TryAs<std::string>()) (Member_) = *lsS; \
            }, Target_);                                                            \
        if (const DataValue* lsV = (Binding_).GetValue())                         \
            if (const std::string* lsS = lsV->TryAs<std::string>()) (Member_) = *lsS; \
    } while(0)

#define DATA_BIND_LOCAL_STRING_GET(_1, _2, _3, _4, NAME, ...) NAME
#define DATA_BIND_LOCAL_STRING(...) \
    DATA_BIND_LOCAL_STRING_GET(__VA_ARGS__, DATA_BIND_LOCAL_STRING_4, DATA_BIND_LOCAL_STRING_3)(__VA_ARGS__)
#define DATA_BIND_LOCAL_STRING_3(Binding_, Key_, Member_) \
    DATA_BIND_LOCAL_STRING_IMPL(Binding_, Key_, Member_, Persistent)
#define DATA_BIND_LOCAL_STRING_4(Binding_, Key_, Member_, Target_) \
    DATA_BIND_LOCAL_STRING_IMPL(Binding_, Key_, Member_, Target_)
