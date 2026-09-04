# Lua Integration

`LuaLayer` (load order 5) owns a **sol2** `sol::state` VM and the registry of scriptable
objects. It redirects Lua's `print()` to `LoggingLayer` (with a `[Lua] ` prefix) and runs
per-tick Lua functions.

Related: [LayerSystem.md](../architecture/LayerSystem.md) ·
[LuaApiReference.md](../reference/LuaApiReference.md)
Key files:
[`LuaLayer.h`](../../source/UGECore/Public/Layers/LuaLayer.h),
[`IScriptableObject.h`](../../source/UGECore/Public/IScriptableObject.h)

---

## IScriptableObject Mixin

Any class that should expose itself to Lua inherits `IScriptableObject` (include
`<IScriptableObject.h>`) and implements `RegisterObject(sol::state& Lua)`. Register the C++ API
using `Lua.new_usertype<T>(...)` and expose the instance via `Lua["Name"] = this`.

For objects created and destroyed at runtime (e.g. scene-lifetime services), also override
`UnregisterObject(sol::state& Lua)` to nil out the globals: `Lua["Name"] = sol::nil`.

Examples: `SDLLayer`, `RmlUILayer`. (`ScriptableObject.h` is a backward-compat shim; prefer
`<IScriptableObject.h>` in new code.)

---

## Registration Paths

`LuaLayer` owns `IScriptableObject` registration. Two paths:

- **Startup** — `UGEApplication::Create()` iterates all pushed layers via
  `dynamic_cast<IScriptableObject*>` and calls `luaLayer->Register(Scriptable)` on each. No
  manual wiring is needed for layers pushed before the init script runs.
- **Runtime (dynamic)** — call `ServiceLocator::Get<LuaLayer>().Register(obj)` to immediately
  invoke `RegisterObject()` and begin tracking; call `Unregister(obj)` to invoke
  `UnregisterObject()` and stop tracking. Use this for `IScriptableObject`s whose lifetime is
  shorter than the application (e.g. scene objects). `SceneManagerLayer` calls these
  automatically when loading/unloading `IScriptableObject` scenes.

---

## Tickable Functions

- `AddTickFunction(sol::protected_function Fn, const char* Name)` → `uint32_t` registers a Lua
  function to be called every `Update()` (returns a tick ID).
- `RemoveTick(uint32_t Id)` unregisters by ID.
- Duplicate name registrations are silently ignored (returns the existing ID).

Exposed to Lua via the `UGE` table — see [LuaApiReference.md](../reference/LuaApiReference.md).

---

## Error Handling & print()

`LuaLayer` overrides Lua's `print()` to forward output to `LoggingLayer` with a `[Lua] `
prefix. Errors from `Execute()` / `ExecuteFile()` are logged and returned as `std::unexpected`
so callers can decide whether to abort.

The startup init script is configured via `InitScript` in `UGESettings.toml` (default
`"assets/lua/initApp.lua"`; set to `""` to skip) — see
[Configuration.md](../reference/Configuration.md).

