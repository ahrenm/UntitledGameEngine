# DataStore

`UGEDataLayer` hosts a single unified **`DataStore`**, pushed **by default** in
`UGEApplication::Create()` (immediately after `PhysFSLayer`, before `LuaLayer`). It replaces
the old three-store split (State / Transient / Data) with one container keyed by `Tag` to
`DataEntry` (a `DataValue` + `DataMeta`).

The authoritative design record is [`DataStore_Design.md`](../DataStore_Design.md).

Related: [LuaIntegration.md](LuaIntegration.md) · [AssetSystem.md](AssetSystem.md)
Key files:
[`UGEDataLayer.h`](../../source/UGECore/Public/Layers/UGEDataLayer.h),
[`DataStore.h`](../../source/UGECore/Public/Layers/DataStores/DataStore.h),
[`DataValue.h`](../../source/UGECore/Public/Layers/DataStores/DataValue.h),
[`Vec2.h`](../../source/UGECore/Public/Layers/DataStores/Vec2.h),
[`DataMeta.h`](../../source/UGECore/Public/Layers/DataStores/DataMeta.h)

---

## Entry Model & Metadata

Every entry carries its own `DataMeta` describing persistence policy and provenance:

- **Persistent** entries → meta `{ Serialize = true, Source = Serialized }`; round-tripped by
  `Save`/`Load`.
- **Transient** entries → default meta; runtime-only, never serialised.
- **Dataset** entries → meta `{ Source = TomlDataset }`; all `*.toml` files in `assets/DATA/`
  are parsed into the store at **layer creation time** (not lazily on first `Update()`).

`DataMeta` = `{ bool Serialize; DataSource Source; std::string SourcePath }`. Persistent writes
pass `DataMeta{ .Serialize = true, .Source = DataSource::Serialized }` (or the
`DataBindTarget::Persistent()` helper); transient writes use default meta.

---

## Store API (`layer.Store`)

**CRUD**

| Call | Result |
|---|---|
| `Set(key, DataValue{v}, meta = {})` | Insert/overwrite. |
| `Find(key)` | `const DataEntry*` |
| `Get(key)` | `const DataValue*` (convenience) |
| `Has(key)` | `bool` |
| `Remove(key)` | erase |
| `operator[](key)` | create-if-absent |

`key` is a `Tag` (implicitly built from `const char*` / `std::string` / `std::string_view`);
the hash is cached at construction. Keys use dot-notation by convention
(`"ui.console.visible"`); storage is flat.

**Iteration** — `ForEach([](const Tag& Key, const DataEntry& Entry){ ... })`.

**Rows** — `Store.RowsView("dataset.arrayKey")` returns a `RowsView` over indexed
array-of-tables entries (`dataset.arrayKey.0.field`, `.1.field`, …). Iterate rows and read
fields with `Row.Get<T>("field")` → `std::optional<T>` or `Row.Field("field")` →
`const DataValue*`. `Size()` gives the row count; `Materialize()` snapshots to owned rows.

**Subscriptions** — `Subscribe(key, cb)` / `SubscribePrefix(prefix, cb)` → `SubscriptionToken`;
`Unsubscribe(token)`. Callback signature is `void(const Tag&, const DataValue&)`.
`layer.SubscribePrefix("ui.", cb)` fires for every `"ui.*"` key.

**Maintenance** — `Compact()` rehashes/shrinks buckets after bulk `Remove()`s (call at scene
teardown).

---

## DataValue

Type-erased value. Read via `Val->TryAs<T>()` (→ `const T*`, null on type mismatch) or
`Val->As<T>()` (throwing). Construct with `DataValue{v}`. User types are supported by
registering a `ValueConverter` in `ValueConverterRegistry` (see `DataValue.h`).

Built-in types: `int`, `float`, `std::string`, and **`Vec2`**.

**`Vec2`** (`{ float X, Y }`, `DataStores/Vec2.h`) — use a single atomic `Vec2` entry for
paired quantities (positions, velocities, screen anchors) so updates notify **once** with both
components consistent, instead of two separate `"<key>.x"` / `"<key>.y"` writes. `Vec2`
round-trips to TOML as `key = { x, y }` and to Lua as `{ x = <n>, y = <n> }`.

---

## Serialisation

`layer.Save(FilePath)` / `layer.Load(FilePath)` (real OS paths) persist/restore every entry
whose `Meta.Serialize == true`; dot-notation keys map to nested TOML tables.

---

## Reactive Bindings

Reactive bindings use the `DATA_BIND` macro family and return a `DataBinding` RAII handle —
**always store it as a member variable**; it auto-unsubscribes on destruction.

| Operation | Behaviour |
|---|---|
| `binding.SetValue(DataValue{...})` | Writes a new value (preserving existing meta), fires subscribers. |
| `binding.GetValue()` | `const DataValue*` — current value (null if inactive/absent). |
| `binding.Release()` | Unsubscribe early. |
| `binding.IsActive()` | Test liveness. |

**`DATA_BIND*` macros** (defined in `UGEDataLayer.h`) — one consistent family with an optional
trailing `Target` selector (`Persistent` default, or `Transient`):

- `DATA_BIND(Key, Default, Callback [, Target])` — injects `Default` if absent, subscribes
  `Callback` (`void(const Tag&, const DataValue&)`), returns a `DataBinding`.
- `DATA_BIND_LOCAL_FLOAT(Binding, Key, FloatMember [, Target])` — binds a float key directly to
  a `float` member (subscribes, then seeds the member from the current value).
- `DATA_BIND_LOCAL_STRING(Binding, Key, StringMember [, Target])` — same for `std::string`.
- `DATA_BIND_LOCAL_VEC2(Binding, Key, Vec2Member [, Target])` — binds an atomic `Vec2` key to a
  `Vec2` member (fires once per update with both components consistent).

---

## Dataset Read Examples

```cpp
layer.Store.Get("platformerData.physics.gravity_y")->As<float>();
layer.Store.Get("platformerData.assets.background")->TryAs<std::string>();
for (const auto& Row : layer.Store.RowsView("platformerMap.tiles")) {
    auto x = Row.Get<float>("x");
}
```

For the Lua-facing `Data.*` table, see [LuaApiReference.md](../reference/LuaApiReference.md).

