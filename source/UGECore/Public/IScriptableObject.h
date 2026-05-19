#pragma once
#include <sol/sol.hpp>

// ── IScriptableObject ─────────────────────────────────────────────────────────
// Mixin interface for any C++ object that exposes itself to the Lua VM.
//
// Implement RegisterObject() to bind member functions / properties onto the
// provided sol::state using new_usertype, set_function, create_named_table,
// etc. The implementation should be self-contained — do not rely on global
// state or call order beyond what the sol::state already holds.
//
// For objects that need to be unregistered at runtime (e.g. scene-lifetime
// objects), also override UnregisterObject() to nil out the Lua globals that
// RegisterObject() created, e.g.:
//   lua["Audio"] = sol::nil;
//
// Typical usage (startup / permanent object):
//
//   class AudioService : public IScriptableObject {
//   public:
//       void RegisterObject(sol::state& Lua) override {
//           Lua.new_usertype<AudioService>("AudioService",
//               "play",     &AudioService::play,
//               "stop_all", &AudioService::stopAll
//           );
//           Lua["Audio"] = this;
//       }
//       ...
//   };
//
// Typical usage (dynamic / scene-lifetime object):
//
//   class MySceneService : public IScriptableObject {
//   public:
//       void RegisterObject(sol::state& Lua) override {
//           Lua["Scene"] = this;
//       }
//       void UnregisterObject(sol::state& Lua) override {
//           Lua["Scene"] = sol::nil;
//       }
//       ...
//   };
//
// Startup objects are collected by Application::Create() and batch-registered
// via LuaLayer::Register().  Dynamic objects call LuaLayer::Register(this) /
// Unregister(this) themselves.
//
// UnregisterObject() is intentionally non-pure — objects that live for the
// full application lifetime (layers) have nothing to unregister and should not
// be burdened with an empty override.  Only scene-scoped objects need it.
class IScriptableObject
{
public:
    virtual ~IScriptableObject() = default;

    // Bind this object's API surface into the provided Lua state.
    // Called by LuaLayer::Register() at startup, or for objects created at runtime.
    virtual void RegisterObject(sol::state& Lua) = 0;

    // Remove this object's API surface from the Lua state.
    // Called by LuaLayer::Unregister().  Default is a no-op — only override if
    // RegisterObject() exposes Lua globals that should be cleared when the object
    // is destroyed (set them to sol::nil here).
    virtual void UnregisterObject(sol::state& /*Lua*/) {}
};

