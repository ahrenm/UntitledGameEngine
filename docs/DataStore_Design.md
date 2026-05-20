# Unified Metadata-Driven DataStore — Design

> Status: **Design locked, pre-implementation**
> Scope: Refactor of `UGEDataLayer` and the `DataStores/` subsystem in `UGECore`.
> Out of scope: binary blob values, multi-threaded execution (design *readiness* only).

---

## 1. Motivation

The current [`UGEDataLayer`](../source/UGECore/Public/Layers/UGEDataLayer.h) hosts **three** separate stores:

| Slot        | Type                                                                 | Semantics                                  |
|-------------|----------------------------------------------------------------------|--------------------------------------------|
| `State`     | [`RuntimeStore`](../source/UGECore/Public/Layers/DataStores/RuntimeStore.h)   | persistent, serialised via TOML            |
| `Transient` | [`RuntimeStore`](../source/UGECore/Public/Layers/DataStores/RuntimeStore.h)   | runtime-only, never serialised             |
| `Data`      | [`DatasetStore`](../source/UGECore/Public/Layers/DataStores/DatasetStore.h)   | read-only TOML datasets (`assets/DATA/**`) |

Problems:

- **Type support is limited** to `std::variant<int, float, std::string>` (`AppStateValue`).
- **Persistence semantics are container-shaped**, not per-value — a datum's lifetime is decided by *which store* it lives in, not by its own metadata.
- **Two key shapes** exist (flat dotted keys vs. two-part `Dataset + Path`), which fragments the API and the mental model.
- **Cross-store concerns** (prefix subscriptions) are bolted on at the layer level and manually wired into each store's `m_onNotify`.

### Goals

1. **One store**, one key shape, one value model.
2. **Metadata per entry**: `Serialize` flag + `Source` provenance.
3. **Arbitrary user-defined types** via type-erasure (Option A), with **implicit cast** back to the value (no `GetValue()`).
4. **Arrays/tables expressed through the tag hierarchy** (sub-tags), not the type system.
5. **Clean, consistent, composable API** built from small focused classes.
6. **Performance-aware**: hashed tags, minimal hot-path allocation, manual compaction.
7. **Thread-safety as a future design goal** — not implemented, but not designed out.

---

## 2. Architecture Overview

The subsystem decomposes into small, single-responsibility units:

```
                         ┌──────────────────────────────┐
                         │        UGEDataLayer           │
                         │  (facade + Lua bridge +       │
                         │   deferred write drain)       │
                         └──────────────┬────────────────┘
                                        │ owns
                         ┌──────────────▼────────────────┐
                         │           DataStore            │
                         │  unordered_map<Tag, DataEntry> │
                         │  Set / Get / Has / Remove /    │
                         │  ForEach / operator[] / Compact│
                         └───┬───────────┬────────────┬───┘
              uses           │           │            │  uses
          ┌──────────────────▼──┐   ┌────▼────────┐   └──────────────┐
          │ SubscriptionRegistry │   │  DataEntry  │                  │
          │ key + prefix watch   │   │ value+meta  │                  │
          └──────────────────────┘   └──┬───────┬──┘                  │
                                        │       │                     │
                                 ┌──────▼──┐ ┌──▼────────┐    ┌────────▼────────┐
                                 │DataValue│ │ DataMeta  │    │    RowsView     │
                                 │ std::any│ │ Serialize │    │ lazy row proxy  │
                                 │ +cast   │ │ Source    │    │ over sub-tags   │
                                 └────┬────┘ └───────────┘    └─────────────────┘
                                      │ converters
                              ┌───────▼─────────┐
                              │ ValueConverter  │
                              │  registry       │
                              └─────────────────┘

  Loading / persistence (own files, no store-internal coupling):
     DatasetLoader   TOML (assets/DATA/**)      → entries (Source=TomlDataset)
     DataSerializer  TOML save/load             ← entries where Serialize==true
```

### Class responsibilities

| Class                  | File                     | Responsibility                                                                 |
|------------------------|--------------------------|--------------------------------------------------------------------------------|
| `Tag`                  | `Tag.h`                  | Immutable dotted key; **hashes at construction**; cheap to copy/compare.       |
| `DataValue`            | `DataValue.h`            | Type-erased value (`std::any`); `template<class T> operator T() const`.        |
| `ValueConverter`       | `DataValue.h`            | Registry of type (de)serialisers + scalar coercions for user types.            |
| `DataMeta`             | `DataMeta.h`             | `bool Serialize`; `DataSource Source`; `std::string SourcePath`.               |
| `DataEntry`            | `DataEntry.h`            | Aggregates `DataValue` + `DataMeta`.                                            |
| `SubscriptionRegistry` | `SubscriptionRegistry.h` | Key + prefix subscriptions; token issue/erase; notify dispatch.                |
| `DataStore`            | `DataStore.h`            | The single map; CRUD; `ForEach`; `operator[]`; `Compact()`; `RowsView()`.      |
| `RowsView`             | `DataStore.h`            | Lazy proxy enumerating indexed sub-tag "rows"; `Materialize()` escape hatch.   |
| `DataSerializer`       | `DataSerializer.h`       | TOML save/load of entries filtered by `Serialize`.                             |
| `DatasetLoader`        | `DatasetLoader.h`        | Parse `assets/DATA/**.toml` → entries (scalars + flattened arrays-of-tables).  |
| `UGEDataLayer`         | `UGEDataLayer.h`         | Facade, Lua `Data.*` bridge, deferred write-queue drain in `Update()`.         |

---

## 3. Value Model (Option A — type erasure)

```cpp
// DataValue.h  (skeleton)
class DataValue
{
public:
    DataValue() = default;

    template <class T>
    explicit DataValue(T Val) : m_data(std::move(Val)) {}

    // Implicit cast back to the stored type — no GetValue() needed:
    //     Vector v = store["enemy.spawn"];      // calls operator Vector()
    template <class T>
    operator T() const { return As<T>(); }       // NOLINT(google-explicit-constructor)

    template <class T>
    [[nodiscard]] T As() const;                   // throws / logs on type mismatch

    template <class T>
    [[nodiscard]] const T* TryAs() const noexcept;// nullptr on mismatch

    [[nodiscard]] bool HasValue() const noexcept { return m_data.has_value(); }
    [[nodiscard]] const std::type_info& Type() const noexcept { return m_data.type(); }

private:
    std::any m_data;
};
```

- **Scalars** (`int`, `float`, `std::string`) are stored directly in the `std::any`.
- **User types** (e.g. `glm::vec3`, a game `Vector`) are stored directly too; they become
  fully first-class by registering a `ValueConverter` so TOML (de)serialisation and Lua
  marshalling know how to round-trip them.
- **Implicit cast**: `operator T()` allows `Vector v = entry.Value;` with no accessor call.
  A mismatching cast logs via `LoggingLayer` and throws `std::bad_any_cast` (or returns a
  default in the `TryAs` path) — final behaviour to be fixed in the design doc's contract
  section (§8) so consumers can rely on it.

### ValueConverter registry

```cpp
// DataValue.h  (skeleton)
struct ValueConverter
{
    // TOML round-trip for persistence (DataSerializer) and dataset loading.
    std::function<std::any(const toml::node&)>   FromToml;
    std::function<void(toml::table&, const char*, const std::any&)> ToToml;

    // Lua round-trip for the Data.* bridge.
    std::function<std::any(sol::object)>          FromLua;
    std::function<sol::object(sol::this_state, const std::any&)>    ToLua;
};

class ValueConverterRegistry
{
public:
    template <class T> void Register(ValueConverter Conv);
    [[nodiscard]] const ValueConverter* Find(const std::type_info& Type) const;
    // Built-ins registered on construction: int, float, std::string.
};
```

User types are registered once (e.g. during plugin load or layer `Create()`):

```cpp
registry.Register<Vector>({ /* FromToml */, /* ToToml */, /* FromLua */, /* ToLua */ });
```

---

## 4. Metadata Model

```cpp
// DataMeta.h  (skeleton)
enum class DataSource : uint8_t
{
    Unknown = 0,
    TomlDataset,       // loaded from assets/DATA/**.toml (read-only origin)
    Config,            // injected from launch/config
    RuntimeTransient,  // created at runtime, ephemeral
    Serialized,        // restored from a save file
};

struct DataMeta
{
    bool        Serialize = false;                 // included in DataSerializer output
    DataSource  Source    = DataSource::Unknown;
    std::string SourcePath;                        // e.g. "assets/DATA/platformerData.toml"
};
```

### Store-slot → metadata mapping

The old three slots become metadata presets, preserving all existing behaviour:

| Old usage                    | New entry metadata                                             |
|------------------------------|---------------------------------------------------------------|
| `State.Set(k, v)`            | `Serialize = true`,  `Source = Serialized` (or `Config`)      |
| `Transient.Set(k, v)`        | `Serialize = false`, `Source = RuntimeTransient`              |
| `Data` (dataset)             | `Serialize = false`, `Source = TomlDataset`, `SourcePath` set |

- **Persistence** = "walk all entries where `Serialize == true`" (was: "serialise the `State` map").
- **Transient lifetime** = entries with `Source == RuntimeTransient` (removable, never persisted).
- **Read-only datasets** are just entries with `Source == TomlDataset`; nothing enforces
  read-only at the type level, but consumers treat them as immutable by convention (matching
  today's behaviour). A future `bool ReadOnly` meta flag is a possible extension.

---

## 5. Tags & Hashing

```cpp
// Tag.h  (skeleton)
class Tag
{
public:
    Tag(std::string_view Key)                       // hashes at construction
        : m_key(Key), m_hash(std::hash<std::string_view>{}(Key)) {}
    Tag(const char* Key) : Tag(std::string_view{Key}) {}

    [[nodiscard]] std::size_t        Hash() const noexcept { return m_hash; }
    [[nodiscard]] const std::string& Str()  const noexcept { return m_key; }

    bool operator==(const Tag& O) const noexcept
    { return m_hash == O.m_hash && m_key == O.m_key; }

private:
    std::string m_key;
    std::size_t m_hash;
};

struct TagHash  { std::size_t operator()(const Tag& T) const noexcept { return T.Hash(); } };
```

- Keys remain **dotted strings** (`"ui.console.visible"`), so existing `constexpr TAG_*` /
  `KEY_*` constants (e.g. [`PlatformerStateTags.h`](../source/GamePlugins/DemoGame/Platformer/PlatformerStateTags.h),
  [`PhysicsTypes.h`](../source/UGECore/Public/Physics/PhysicsTypes.h)) need **no changes**.
- `DataStore` uses `unordered_map<Tag, DataEntry, TagHash>` so lookups reuse the cached hash
  instead of re-hashing the string each access.

---

## 6. Rows via Sub-Tags (lazy `RowsView`)

Arrays-of-tables are **not** a value type. `DatasetLoader` flattens each `[[tiles]]` row into
indexed sub-tags:

```
platformerMap.tiles.0.x       = 3.0
platformerMap.tiles.0.y       = 5.0
platformerMap.tiles.0.sprite  = "tile-ground"
platformerMap.tiles.1.x       = 4.0
...
```

`DataStore::RowsView(prefix)` returns a **lazy** proxy that enumerates rows by index; each
field access hits the store on demand. A `Materialize()` escape hatch produces the
old-style `vector<unordered_map<string, DataValue>>` when a snapshot is preferable.

```cpp
// DataStore.h  (skeleton)
class RowsView
{
public:
    class Row
    {
    public:
        [[nodiscard]] const DataValue* Field(std::string_view Name) const; // lazy lookup

        template <class T>
        [[nodiscard]] std::optional<T> Get(std::string_view Name) const;
    private:
        const DataStore* m_store;
        std::string      m_rowPrefix; // e.g. "platformerMap.tiles.0"
    };

    class Iterator { /* yields Row for 0,1,2,... until a gap */ };

    [[nodiscard]] Iterator begin() const;
    [[nodiscard]] Iterator end()   const;
    [[nodiscard]] std::size_t Size() const;

    // Snapshot escape hatch — materialise into owned rows.
    using MaterialRow = std::unordered_map<std::string, DataValue>;
    [[nodiscard]] std::vector<MaterialRow> Materialize() const;

private:
    const DataStore* m_store;
    std::string      m_arrayPrefix; // e.g. "platformerMap.tiles"
};
```

[`TileWorld`](../source/GamePlugins/DemoGame/Platformer/TileWorld.cpp) migrates from
`Data.GetRows("platformerMap", "tiles")` to
`store.RowsView("platformerMap.tiles")` and iterates `for (auto Row : view)`.

---

## 7. Store, Subscriptions & Compaction

```cpp
// DataStore.h  (skeleton)
class DataStore
{
public:
    using ChangeCallback    = std::function<void(const Tag&, const DataValue&)>;
    using SubscriptionToken = uint32_t;

    // ── CRUD ──────────────────────────────────────────────────────────────
    void                     Set(const Tag& Key, DataValue Val, DataMeta Meta = {});
    [[nodiscard]] const DataEntry* Find(const Tag& Key) const;   // nullptr if absent
    [[nodiscard]] const DataValue* Get(const Tag& Key) const;    // convenience
    [[nodiscard]] bool             Has(const Tag& Key) const;
    void                     Remove(const Tag& Key);
    DataEntry&               operator[](const Tag& Key);         // create-if-absent

    // ── Iteration ─────────────────────────────────────────────────────────
    void ForEach(const std::function<void(const Tag&, const DataEntry&)>& Fn) const;

    // ── Rows ──────────────────────────────────────────────────────────────
    [[nodiscard]] RowsView RowsView(std::string_view ArrayPrefix) const;

    // ── Subscriptions (delegated to SubscriptionRegistry) ───────────────────
    [[nodiscard]] SubscriptionToken Subscribe(const Tag& Key, ChangeCallback Cb);
    [[nodiscard]] SubscriptionToken SubscribePrefix(std::string_view Prefix, ChangeCallback Cb);
    void Unsubscribe(SubscriptionToken Token);

    // ── Maintenance ─────────────────────────────────────────────────────────
    // Manual defragmentation — rehash/shrink after bulk Remove()s.
    // Call at natural boundaries (e.g. scene teardown).
    void Compact();

private:
    std::unordered_map<Tag, DataEntry, TagHash> m_entries;
    SubscriptionRegistry                        m_subs;
};
```

- `SubscriptionRegistry` absorbs the per-key subscription list from
  [`RuntimeStore`](../source/UGECore/Private/Layers/DataStores/RuntimeStore.cpp) **and** the
  prefix subscription list from
  [`UGEDataLayer`](../source/UGECore/Private/Layers/UGEDataLayer.cpp), unifying notify dispatch
  in one place. No more `m_onNotify` cross-wiring.
- `Compact()` performs `rehash(0)` / bucket shrink and drops tombstoned rows so long-lived
  sessions with heavy transient churn don't fragment.

---

## 8. Threading Readiness (design goal, not implemented)

The engine is single-threaded today; `Run()` calls `Update()` on each layer once per frame.
To keep a future thread-safe store cheap to add **without touching call sites**:

- **Deferred write-queue**: `Set` / `Remove` from non-owner contexts (e.g. worker threads,
  Lua ticks) enqueue a command; `UGEDataLayer::Update()` **drains the queue** at the frame
  sync point and applies mutations + fires subscribers on the main thread.
- Reads remain direct (lock-free) in the single-threaded build; the queue is the seam where a
  mutex or lock-free MPSC queue slots in later.
- `DataStore`'s internal map is encapsulated so a `std::shared_mutex` can wrap CRUD with no
  API change.

> For this refactor we implement the **queue seam + main-thread drain** (cheap, single-threaded
> correct) but **not** any locking. This documents intent and avoids designing threading out.

---

## 9. Old → New API Equivalence

| Old API                                                     | New API                                                              |
|-------------------------------------------------------------|----------------------------------------------------------------------|
| `layer.State.Set(k, v)`                                     | `layer.Store.Set(k, DataValue{v}, {.Serialize=true, .Source=Serialized})` |
| `layer.Transient.Set(k, v)`                                 | `layer.Store.Set(k, DataValue{v})` (default meta = transient)        |
| `layer.State.Get(k)` / `Transient.Get(k)`                   | `layer.Store.Get(k)` → `const DataValue*`                            |
| `store.GetInt/GetFloat/GetString(k)`                        | `value.TryAs<T>()` / `value.As<T>()` / implicit cast                 |
| `layer.Transient.Remove(k)`                                 | `layer.Store.Remove(k)`                                              |
| `layer.Data.GetFloat("ds","a.b")`                           | `layer.Store.Get("ds.a.b")->As<float>()`                            |
| `layer.Data.GetRows("map","tiles")`                         | `layer.Store.RowsView("map.tiles")`                                  |
| `layer.SaveState(path)` / `LoadState(path)`                 | `layer.Save(path)` / `layer.Load(path)` (serialises `Serialize==true`)|
| `layer.SubscribePrefix(prefix, cb)`                         | `layer.Store.SubscribePrefix(prefix, cb)`                            |
| `APPSTATE_BIND(k, def, cb)`                                 | `DATA_BIND(k, def, cb)`  (persistent)                                |
| `APPSTATE_BIND_TRANSIENT(k, def, cb)`                       | `DATA_BIND(k, def, cb, Transient)`                                   |
| `APPSTATE_BIND_LOCAL_FLOAT(b, k, m)`                        | `DATA_BIND_LOCAL_FLOAT(b, k, m)`                                     |
| `APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT(b, k, m)`              | `DATA_BIND_LOCAL_FLOAT(b, k, m, Transient)`                          |
| `APPSTATE_BIND_LOCAL_STRING(b, k, m)`                       | `DATA_BIND_LOCAL_STRING(b, k, m)`                                    |
| `APPSTATE_BIND_TRANSIENT_LOCAL_STRING(b, k, m)`             | `DATA_BIND_LOCAL_STRING(b, k, m, Transient)`                         |
| Lua `Data.SetTransient(k, v)` / `GetTransient(k)` / `ShowTransient()` | Lua `Data.Set(k, v)` / `Data.Get(k)` / `Data.Show()` (transient default) |

> **Hard break**: no legacy aliases. All consumers migrate in tasks T3/T4.

### New macro shape

```cpp
// One consistent family. Target selector defaults to the persistent store;
// pass `Transient` as the trailing arg to target runtime-only entries.
DATA_BIND(Key, Default, Callback [, Target])
DATA_BIND_LOCAL_FLOAT(Binding, Key, Member [, Target])
DATA_BIND_LOCAL_STRING(Binding, Key, Member [, Target])
```

`DataBinding` replaces `AppStateBinding` (same RAII/auto-unsubscribe contract).

---

## 10. File Plan

**New** (`source/UGECore/Public/Layers/DataStores/`):
`Tag.h`, `DataValue.h`, `DataMeta.h`, `DataEntry.h`, `DataStore.h`,
`SubscriptionRegistry.h`, `DataSerializer.h`, `DatasetLoader.h`
(+ matching `.cpp` in `source/UGECore/Private/Layers/DataStores/`).

**Rewritten**:
[`UGEDataLayer.h`](../source/UGECore/Public/Layers/UGEDataLayer.h) /
[`UGEDataLayer.cpp`](../source/UGECore/Private/Layers/UGEDataLayer.cpp) — single `Store`,
`DataBinding`, `DATA_BIND*` macros, Lua bridge, `Update()` queue drain.

**Deleted**:
`StoreBase.h/.cpp`, `RuntimeStore.h/.cpp`, `DatasetStore.h/.cpp`, `StoreSerializer.h/.cpp`.

**Consumers migrated** (see task briefs): `AnimationControllerBase`, `TileWorld`,
`WindowViewModel`, Platformer (`PlatformerCharacter`, tags), LuaTests, TeaPot, StateAssets,
plus `assets/lua/tickingTest.lua` and `assets/ui/lua_tests.rml` snippet strings.

---

## 11. Acceptance Criteria (all tasks)

- Builds clean via the MinGW workflow in `AGENTS.md` (`cmake --build …\cmake-build-debug --target all`).
- **Behaviour parity**:
  - Save/Load round-trips all `Serialize==true` entries (was `State`).
  - Dataset scalar reads and tile rows produce identical world geometry.
  - Reactive bindings fire identically (window drag, physics tags, HUD).
  - Lua `Data.*` scripts (`tickingTest.lua`) behave identically.
- Tag-hash lookup benchmarked vs. old string-keyed map (note in PR/summary).
- No references to deleted symbols (`AppStateValue`, `RuntimeStore`, `DatasetStore`,
  `APPSTATE_BIND*`, `SaveState/LoadState`) remain.

