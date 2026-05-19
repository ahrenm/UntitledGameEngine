#pragma once
#include "AppLayer.h"
#include "../ServiceLocator.h"
#include "../IScriptableObject.h"
#include "../LayerRegistry.h"
#include "DataStores/RuntimeStore.h"
#include "DataStores/StoreSerializer.h"
#include "DataStores/DatasetStore.h"

#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ── UDEDataLayer ──────────────────────────────────────────────────────────────
// Central data layer hosting three public store objects:
//
//   State     — persistent key-value store (survives sessions; serialised)
//   Transient — runtime-only key-value store (never serialised; items may be removed)
//   Data      — read-only TOML dataset store (assets/DATA/**/*.toml)
//
// Access any store directly, e.g.:
//   auto& layer = ServiceLocator::Get<UDEDataLayer>();
//   layer.State.Set("ui.visible", 1);
//   layer.Transient.Set("game.score", 0);
//   auto gravity = layer.Data.GetFloat("platformerData", "physics.gravity");
//
// Use APPSTATE_BIND / APPSTATE_BIND_TRANSIENT for reactive bindings.
// Use SubscribePrefix for cross-store prefix watching.
//
// Load order: 3 — must follow PhysFSLayer (2) as Create() scans assets/DATA/ via PhysFS.
class UDEDataLayer : public AppLayer, public IScriptableObject
{
public:
    REGISTER_LAYER("ude-data", 3.0f, UDEDataLayer)

    // ── Factory ───────────────────────────────────────────────────────────────
    // Creates the layer and immediately scans assets/DATA/ via PhysFSLayer,
    // loading all *.toml files into Data.  PhysFSLayer must be registered with
    // the ServiceLocator before calling Create().
    [[nodiscard]] static std::expected<std::unique_ptr<UDEDataLayer>, std::string> Create();

    // ── Public store objects ──────────────────────────────────────────────────
    RuntimeStore State;      // persistent key-value store  (serialised via SaveState/LoadState)
    RuntimeStore Transient;  // runtime-only key-value store (never serialised)
    DatasetStore    Data;       // read-only TOML dataset store

    // ── Cross-store prefix subscription ───────────────────────────────────────
    // Fires Cb for every key starting with Prefix across both State and Transient.
    // e.g. SubscribePrefix("ui.", cb) watches "ui.console.visible" in both stores.
    using SubscriptionToken = StoreReadOnly::SubscriptionToken;
    using ChangeCallback    = StoreReadOnly::ChangeCallback;

    [[nodiscard]] SubscriptionToken SubscribePrefix(const std::string& Prefix, ChangeCallback Cb);
    void UnsubscribePrefix(SubscriptionToken Token);

    // ── Serialisation ─────────────────────────────────────────────────────────
    // Persist State to / restore State from a TOML file at a real OS path.
    // Dot-notation keys are written as a nested TOML hierarchy.
    // Returns false and logs on I/O or parse failure.
    [[nodiscard]] bool SaveState(const char* FilePath);
    [[nodiscard]] bool LoadState(const char* FilePath);

    // ── IScriptableObject ─────────────────────────────────────────────────────
    // Exposes Data.SetTransient(key, value) and Data.GetTransient(key) to Lua.
    void RegisterObject(sol::state& Lua) override;

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void RegisterWithServiceLocator() override;

    ~UDEDataLayer() override;

private:
    UDEDataLayer();

    struct PrefixSubscription
    {
        std::string       prefix;
        ChangeCallback    callback;
        SubscriptionToken token = 0;
    };

    std::vector<PrefixSubscription> m_prefixSubs;
    SubscriptionToken               m_nextPrefixToken = 1;
    StoreSerializer       m_serializer;
};


// ── AppStateBinding ───────────────────────────────────────────────────────────
// RAII handle returned by APPSTATE_BIND / APPSTATE_BIND_TRANSIENT.
// Automatically calls Unsubscribe() when destroyed or move-assigned.
// Always store as a member variable — never as a local variable.
//
// API:
//   binding.SetValue(AppStateValue{...})  — write a new value (fires subscribers)
//   binding.GetValue()                    — read the current value (const Value*)
//   binding.Release()                     — unsubscribe early
//   binding.IsActive()                    — true if still subscribed
class AppStateBinding
{
public:
    AppStateBinding() = default;

    AppStateBinding(StoreReadWrite* Store, StoreReadOnly::SubscriptionToken Token,
                    std::string Key)
        : m_store(Store), m_token(Token), m_key(std::move(Key)) {}

    ~AppStateBinding() { release(); }

    AppStateBinding(const AppStateBinding&)            = delete;
    AppStateBinding& operator=(const AppStateBinding&) = delete;

    AppStateBinding(AppStateBinding&& Other) noexcept
        : m_store(std::exchange(Other.m_store, nullptr))
        , m_token(std::exchange(Other.m_token, 0))
        , m_key(std::move(Other.m_key)) {}

    AppStateBinding& operator=(AppStateBinding&& Other) noexcept
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

    // Write a new value into the bound key's store, firing all subscribers.
    // No-op if the binding is inactive or has no key.
    void SetValue(AppStateValue Val)
    {
        if (!m_store || m_key.empty()) return;
        m_store->Set(m_key, std::move(Val));
    }

    // Read the current value from the bound key's store.
    // Returns nullptr if the binding is inactive, has no key, or the key is absent.
    [[nodiscard]] const AppStateValue* GetValue() const
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
        if (m_store && m_token && ServiceLocator::TryGet<UDEDataLayer>())
            m_store->Unsubscribe(m_token);
        m_store = nullptr;
        m_token = 0;
    }

    StoreReadWrite*                  m_store = nullptr;
    StoreReadOnly::SubscriptionToken m_token = 0;
    std::string                      m_key;
};


// ── APPSTATE_BIND ─────────────────────────────────────────────────────────────
// Targets UDEDataLayer::State (persistent store).
// Injects Key_ with InitialValue_ if absent, subscribes Callback_, and returns
// an AppStateBinding that auto-unsubscribes on destruction.
//
// Callback_ signature: void(const std::string& Key, const AppStateValue& Val)
//
// Example:
//   AppStateBinding m_binding = APPSTATE_BIND(
//       "ui.console.visible", 1,
//       [this](const std::string&, const AppStateValue& Val) {
//           m_visible = std::get<int>(Val) != 0;
//           m_model.DirtyVariable("panel_visible");
//       });
//
// Returns an empty (no-op) AppStateBinding if UDEDataLayer is not yet registered.
#define APPSTATE_BIND(Key_, InitialValue_, Callback_)                              \
    [&]() -> AppStateBinding {                                                     \
        auto* _Layer = ServiceLocator::TryGet<UDEDataLayer>();                    \
        if (!_Layer) return {};                                                    \
        auto& _S = _Layer->State;                                                  \
        if (!_S.Has(Key_)) _S.Set((Key_), AppStateValue{InitialValue_});           \
        return AppStateBinding(&_S, _S.Subscribe((Key_), (Callback_)), (Key_));   \
    }()

// ── APPSTATE_BIND_TRANSIENT ───────────────────────────────────────────────────
// Like APPSTATE_BIND but targets UDEDataLayer::Transient (runtime-only store).
//
// Example:
//   AppStateBinding m_dragBinding = APPSTATE_BIND_TRANSIENT(
//       "ui.console.dragging", 0,
//       [this](const std::string&, const AppStateValue& Val) {
//           m_dragging = std::get<int>(Val) != 0;
//       });
#define APPSTATE_BIND_TRANSIENT(Key_, InitialValue_, Callback_)                    \
    [&]() -> AppStateBinding {                                                     \
        auto* _Layer = ServiceLocator::TryGet<UDEDataLayer>();                    \
        if (!_Layer) return {};                                                    \
        auto& _S = _Layer->Transient;                                              \
        if (!_S.Has(Key_)) _S.Set((Key_), AppStateValue{InitialValue_});           \
        return AppStateBinding(&_S, _S.Subscribe((Key_), (Callback_)), (Key_));   \
    }()

// ── APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT ───────────────────────────────────────
// Convenience wrapper for the common pattern of binding a transient float key
// directly to a local float variable: subscribes, then seeds the variable from
// the current stored value so it is immediately valid without a SetValue call.
//
// Binding_ : AppStateBinding lvalue to assign into
// Key_     : std::string_view or const char* tag (e.g. TAG_GRAVITY)
// Member_  : float lvalue to keep in sync
//
// Example:
//   APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT(m_gravityBinding, TAG_GRAVITY, m_gravity);
#define APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT(Binding_, Key_, Member_)               \
    do {                                                                            \
        (Binding_) = APPSTATE_BIND_TRANSIENT((Key_).data(), 0.0f,                 \
            [&](const std::string&, const AppStateValue& lfVal)                   \
            {                                                                       \
                if (const auto* lfF = std::get_if<float>(&lfVal)) (Member_) = *lfF; \
            });                                                                     \
        if (const auto* lfV = (Binding_).GetValue())                              \
            if (const auto* lfF = std::get_if<float>(lfV)) (Member_) = *lfF;     \
    } while(0)

// ── APPSTATE_BIND_LOCAL_FLOAT ─────────────────────────────────────────────────
// Persistent-store equivalent of APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT.
// Binds a persistent State key directly to a local float member: subscribes,
// then seeds Member_ from the current stored value immediately.
//
// Binding_ : AppStateBinding lvalue to assign into
// Key_     : std::string_view or const char* tag
// Member_  : float lvalue to keep in sync
//
// Example:
//   APPSTATE_BIND_LOCAL_FLOAT(m_volumeBinding, "audio.volume", m_volume);
#define APPSTATE_BIND_LOCAL_FLOAT(Binding_, Key_, Member_)                         \
    do {                                                                            \
        (Binding_) = APPSTATE_BIND((Key_), 0.0f,                                  \
            [&](const std::string&, const AppStateValue& lfVal)                   \
            {                                                                       \
                if (const auto* lfF = std::get_if<float>(&lfVal)) (Member_) = *lfF; \
            });                                                                     \
        if (const auto* lfV = (Binding_).GetValue())                              \
            if (const auto* lfF = std::get_if<float>(lfV)) (Member_) = *lfF;     \
    } while(0)

// ── APPSTATE_BIND_LOCAL_STRING ────────────────────────────────────────────────
// Binds a persistent State string key directly to a local std::string member.
// Subscribes and seeds Member_ from the current stored value immediately.
//
// Binding_ : AppStateBinding lvalue to assign into
// Key_     : const char* or std::string tag
// Member_  : std::string lvalue to keep in sync
//
// Example:
//   APPSTATE_BIND_LOCAL_STRING(m_nameBinding, "character.name", m_name);
#define APPSTATE_BIND_LOCAL_STRING(Binding_, Key_, Member_)                        \
    do {                                                                            \
        (Binding_) = APPSTATE_BIND((Key_), std::string{},                          \
            [&](const std::string&, const AppStateValue& lsVal)                   \
            {                                                                       \
                if (const auto* lsS = std::get_if<std::string>(&lsVal)) (Member_) = *lsS; \
            });                                                                     \
        if (const auto* lsV = (Binding_).GetValue())                              \
            if (const auto* lsS = std::get_if<std::string>(lsV)) (Member_) = *lsS; \
    } while(0)

// ── APPSTATE_BIND_TRANSIENT_LOCAL_STRING ──────────────────────────────────────
// Transient-store equivalent of APPSTATE_BIND_LOCAL_STRING.
// Binds a runtime-only Transient string key directly to a local std::string member.
//
// Example:
//   APPSTATE_BIND_TRANSIENT_LOCAL_STRING(m_labelBinding, "ui.label", m_label);
#define APPSTATE_BIND_TRANSIENT_LOCAL_STRING(Binding_, Key_, Member_)              \
    do {                                                                            \
        (Binding_) = APPSTATE_BIND_TRANSIENT((Key_), std::string{},                \
            [&](const std::string&, const AppStateValue& lsVal)                   \
            {                                                                       \
                if (const auto* lsS = std::get_if<std::string>(&lsVal)) (Member_) = *lsS; \
            });                                                                     \
        if (const auto* lsV = (Binding_).GetValue())                              \
            if (const auto* lsS = std::get_if<std::string>(lsV)) (Member_) = *lsS; \
    } while(0)


