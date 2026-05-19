#pragma once
#include "AppLayer.h"
#include <IEventHandler.h>
#include <IScriptableObject.h>
#include <LayerRegistry.h>
#include <sol/sol.hpp>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// ── LuaLayer ──────────────────────────────────────────────────────────────────
// Owns the sol::state (Lua VM) for the lifetime of the application.
// Push immediately after LoggingLayer so Lua is available to all subsequent
// layers during their initialisation.
//
// print() is redirected to LoggingLayer with a "[Lua] " prefix.
// Errors from execute() / executeFile() are also logged and returned as
// std::unexpected so callers can decide whether to abort.
//
// HandleEvent() intentionally does not consume SDL input; window/view-model
// input handling (including Lua console visibility) lives with those owners.
//
// Access anywhere via ServiceLocator::get<LuaLayer>().
//
// Load order: 5 — must follow SDLLayer (4).
class LuaLayer : public AppLayer, public IEventHandler
{
public:
    REGISTER_LAYER("lua", 5.0f, LuaLayer)

    [[nodiscard]] static std::expected<std::unique_ptr<LuaLayer>, std::string> Create();

    ~LuaLayer() override = default;

    LuaLayer(const LuaLayer&)            = delete;
    LuaLayer& operator=(const LuaLayer&) = delete;

    // Called every frame — calls all registered tick functions.
    void Update() override;

    // Execute a Lua snippet from a string.
    // Returns std::unexpected(error_message) and logs on failure.
    std::expected<void, std::string> Execute(std::string_view Code);

    // Read a script from the PhysFS VFS and execute it.
    // Requires PhysFSLayer to be registered with ServiceLocator.
    std::expected<void, std::string> ExecuteFile(const char* VirtualPath);

    // Direct access to the Lua state — use to register C++ types / functions.
    [[nodiscard]] sol::state& State() { return m_lua; }

    // LuaLayer does not consume SDL input by default.
    bool HandleEvent(SDL_Event& Event) override;

    // ── IScriptableObject registration ────────────────────────────────────────
    // Immediately invokes RegisterObject() on Obj and begins tracking it.
    void Register(IScriptableObject* Obj);

    // Immediately invokes UnregisterObject() on Obj and stops tracking it.
    // Safe no-op if Obj is not currently tracked.
    void Unregister(IScriptableObject* Obj);

    // ── Tickable functions ────────────────────────────────────────────────────
    // Register a Lua function already in the VM to be called every Update().
    // Name is used for display (e.g. UGE.ShowTicking()). Returns a tick ID.
    uint32_t AddTickFunction(sol::protected_function Fn,
                             const char* Name = "<anonymous>");

    // Remove a previously registered tick function by ID. Safe no-op if absent.
    void RemoveTick(uint32_t Id);

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void RegisterWithServiceLocator() override;

private:
    // Opens standard Lua libraries and wires print() to LoggingLayer.
    LuaLayer();


    struct TickEntry
    {
        uint32_t                id;
        std::string             name;
        sol::protected_function  fn;
    };

    void registerUGETable();

    sol::state                     m_lua;
    std::vector<IScriptableObject*> m_objects;
    std::vector<TickEntry>         m_tickEntries;
    uint32_t                       m_nextTickId = 1;
};
