# DataStore Refactor — Sub-Agent Task Briefs

> Companion to [`DataStore_Design.md`](./DataStore_Design.md). Each task below is a
> self-contained brief that can be handed to a coding sub-agent. Tasks are ordered by
> dependency; **T2 depends on T1**, **T3/T4 depend on T2**, **T5 is last**.
>
> Every task shares these **global constraints**:
> - Follow the coding conventions in `AGENTS.md` (PascalCase public / camelCase private+`m_`).
> - Build via the MinGW workflow: prepend `C:\Program Files\JetBrains\CLion 2026.1\bin\mingw\bin`
>   to `PATH`, then `cmake --build C:\Users\ahren\CLionProjects\untitled\cmake-build-debug --target all -j 62`.
> - Validate edited files with the error checker; do not leave dangling references to deleted symbols.
> - The header include root is `source/UGECore/Public/`; use `#include <Layers/DataStores/...>`.

---

## Dependency Graph

```
T1  Core value/store primitives
 └─► T2  UGEDataLayer facade + Lua bridge + macros
       ├─► T3  UGECore consumer migration
       └─► T4  Plugin consumer migration
             └─► T5  AGENTS.md documentation update
```

---

## T1 — Core Store Primitives

**Goal**: Implement the composable value/metadata/store classes from the design doc §3–§7.
Produce *no* consumer changes; this task delivers the new subsystem in isolation, compiling
into `UGECore` alongside (not yet replacing) the old stores.

**Create** (`source/UGECore/Public/Layers/DataStores/` + matching `Private/`):
- `Tag.h` — dotted key, hash-at-construction, `operator==`, `TagHash` functor (§5).
- `DataValue.h` / `.cpp` — `std::any` holder, `template<class T> operator T()`, `As<T>`,
  `TryAs<T>`; `ValueConverter` + `ValueConverterRegistry` with built-in `int`/`float`/`std::string`
  converters (TOML + Lua round-trips) (§3).
- `DataMeta.h` — `DataSource` enum + `DataMeta` struct (§4).
- `DataEntry.h` — aggregate of `DataValue` + `DataMeta`.
- `SubscriptionRegistry.h` / `.cpp` — unified key + prefix subscriptions, token issue/erase,
  notify dispatch (port logic from `RuntimeStore::notify` and `UGEDataLayer::SubscribePrefix`).
- `DataStore.h` / `.cpp` — `unordered_map<Tag, DataEntry, TagHash>`; `Set/Find/Get/Has/Remove/`
  `operator[]/ForEach/Subscribe/SubscribePrefix/Unsubscribe/Compact`; `RowsView` + lazy
  `RowsView::Row`/`Iterator` + `Materialize()` (§6, §7).

**Constraints**:
- Use the bundled TOML lib (`#include "../../../Public/toml.hpp"` pattern as in
  `DatasetStore.cpp`) inside converters/loaders only — keep `DataStore.h` TOML-free.
- Mismatched `As<T>()` logs via `LoggingLayer` (fetched through `ServiceLocator::TryGet`) and
  throws `std::bad_any_cast`; `TryAs<T>()` returns `nullptr` and never throws. Document this
  contract in the header.
- Add the new `.cpp` files to `source/UGECore/CMakeLists.txt` if sources are listed explicitly.

**Acceptance**: `UGECore` builds with both old and new subsystems present; a temporary unit
smoke-test (or a scratch `static_assert`/runtime check) confirms round-trip of `int`/`float`/
`std::string` and one user type (`glm::vec3`) through `DataValue` + converters. Remove scratch
tests before finishing.

---

## T2 — UGEDataLayer Facade, Lua Bridge & Macros

**Depends on**: T1.

**Goal**: Replace the three-store `UGEDataLayer` with the single-`Store` facade, port
serialisation and dataset loading, rewrite the Lua `Data.*` table, introduce the deferred
write-queue drain, and define the new `DATA_BIND*` macro family + `DataBinding`.

**Create**:
- `DataSerializer.h` / `.cpp` — TOML save/load; **save** walks `Store.ForEach` emitting only
  entries where `Meta.Serialize == true` (uses converter `ToToml`); **load** parses TOML,
  reconstructs values via converters, sets entries with `Source = Serialized`. Port dotted-key
  ↔ nested-table logic from `StoreSerializer.cpp`.
- `DatasetLoader.h` / `.cpp` — scan/parse `assets/DATA/**.toml`; scalars → entries
  (`Source = TomlDataset`, `SourcePath` set); arrays-of-tables → indexed sub-tag entries
  (`map.tiles.0.x`, …) per §6. Port `flattenTable` from `DatasetStore.cpp`.

**Rewrite**:
- [`UGEDataLayer.h`](../source/UGECore/Public/Layers/UGEDataLayer.h):
  - Single public `DataStore Store;` (drop `State` / `Transient` / `Data`).
  - `Save(path)` / `Load(path)` (rename from `SaveState`/`LoadState`).
  - `SubscribePrefix` delegates to `Store`.
  - `Update()` drains the deferred write-queue (§8).
  - Replace `AppStateBinding` with `DataBinding` (same RAII contract, targets `DataStore`).
  - Replace all `APPSTATE_BIND*` macros with the `DATA_BIND` / `DATA_BIND_LOCAL_FLOAT` /
    `DATA_BIND_LOCAL_STRING` family with optional `Target` selector (§9). Persistent =
    `{Serialize=true, Source=Serialized}`; `Transient` = default meta.
- [`UGEDataLayer.cpp`](../source/UGECore/Private/Layers/UGEDataLayer.cpp):
  - `Create()` uses `DatasetLoader` to populate `Store` from `assets/DATA/`.
  - Lua bridge: `Data.Set(k, v)` / `Data.Get(k)` / `Data.Show()` — reuse the type-coercion
    behaviour of the old `SetTransient` (coerce to existing backing type, infer for new keys),
    routing through `ValueConverter` `FromLua`/`ToLua`.

**Delete**: `StoreBase.h/.cpp`, `RuntimeStore.h/.cpp`, `DatasetStore.h/.cpp`,
`StoreSerializer.h/.cpp` (and their CMake entries).

**Acceptance**: `UGECore` builds; layer registers; datasets load; Lua `Data.*` responds.
Consumers will not yet compile — that is expected and handled in T3/T4. Note in the summary
that downstream targets are temporarily broken pending T3/T4.

---

## T3 — UGECore Consumer Migration

**Depends on**: T2.

**Goal**: Migrate all `UGECore`-internal consumers to the new API.

**Files**:
- [`AnimationControllerBase.cpp`](../source/UGECore/Private/Animation/AnimationControllerBase.cpp)
  — replace `store.GetString(DatasetKey, key("sprite"))` etc. with
  `Store.Get(DatasetKey + "." + key(...))->TryAs<std::string>()` (or implicit cast) per §9.
- [`TileWorld.cpp`](../source/GamePlugins/DemoGame/Platformer/TileWorld.cpp) — replace
  `Data.GetRows(MapKey, "tiles")` + row `.find()` loops with
  `Store.RowsView(std::string(MapKey) + ".tiles")` and `Row.Get<float>("x")` etc.
- Audit `GameObjectBase.h` comment/example (`Data->Data.GetFloat(...)`) and update to new API.

**Acceptance**: `UGECore` + `DemoGame` compile for these files; tile map produces identical
geometry (same wall/coin counts logged by `TileWorld`).

---

## T4 — Plugin Consumer Migration

**Depends on**: T2 (parallelisable with T3).

**Goal**: Migrate all game-plugin consumers and the Lua/RML asset strings.

**Files**:
- [`WindowViewModel.h`/`.cpp`](../source/GamePlugins/UIWindows/WindowViewModel.cpp) —
  `Transient.Get/Set` → `Store.Get/Set`; `AppStateBinding` members → `DataBinding`;
  `APPSTATE_BIND_TRANSIENT` → `DATA_BIND(..., Transient)`; `setTransient` helper updated.
- [`PlatformerCharacter.cpp`](../source/GamePlugins/DemoGame/Platformer/PlatformerCharacter.cpp)
  — `APPSTATE_BIND_LOCAL_STRING` / `APPSTATE_BIND_TRANSIENT_LOCAL_FLOAT` → `DATA_BIND_LOCAL_*`;
  `m_dataLayer->Transient.Set(...)` → `Store.Set(...)`.
- [`LuaTestsViewModel.cpp`](../source/GamePlugins/DemoGame/LuaTests/LuaTestsViewModel.cpp),
  [`LuaTestsScene.cpp`](../source/GamePlugins/DemoGame/LuaTests/LuaTestsScene.cpp) — `Data->Transient.Set` → `Store.Set`.
- [`TeaPotDemoViewModel.cpp`](../source/GamePlugins/TeaPot3D/TeaPotDemo/TeaPotDemoViewModel.cpp)
  — `Data->Transient.Set(KEY_LIGHT_COLOR, ...)`; if `m_lightColor` is a vector type, register a
  `ValueConverter` and store it directly (showcases user-type support).
- [`StateAssetsDemoViewModel.cpp`](../source/GamePlugins/DemoGame/StateAssetsDemo/StateAssetsDemoViewModel.cpp),
  [`StateAssetsDemoScene.cpp`](../source/GamePlugins/DemoGame/StateAssetsDemo/StateAssetsDemoScene.cpp)
  — `SaveState`/`LoadState` → `Save`/`Load`; `Store.GetString/GetInt` reads via new API;
  `Transient.Set` → `Store.Set`.
- Asset strings: [`assets/lua/tickingTest.lua`](../assets/lua/tickingTest.lua) and
  [`assets/ui/lua_tests.rml`](../assets/ui/lua_tests.rml) — `Data.SetTransient`/`GetTransient`
  → `Data.Set`/`Data.Get`.

**Acceptance**: all plugin DLLs build; window drag/z-order, platformer physics tags, HUD
score, TeaPot light, and StateAssets save/load behave identically at runtime.

---

## T5 — Documentation Update

**Depends on**: T3 + T4 complete and verified.

**Goal**: Update `AGENTS.md` to describe the unified `Store`, `DataValue`/`DataMeta`,
`RowsView`, `DataBinding`, `DATA_BIND*` macros, and the new Lua `Data.*` surface. Remove all
references to `State`/`Transient`/`Data` slots, `AppStateValue`, `APPSTATE_BIND*`,
`SaveState`/`LoadState`, and `GetRows`. Keep [`DataStore_Design.md`](./DataStore_Design.md)
cross-linked as the authoritative design record.

**Acceptance**: `AGENTS.md` contains no stale API names; the "Asset & Data Layers" and
"Lua API Surfaces" sections match the shipped API.

