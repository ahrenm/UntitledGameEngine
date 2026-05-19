#include <Layers/LuaLayer.h>
#include <Layers/PhysFSLayer.h>
#include <ServiceLocator.h>
#include <lua.h>
#include <lauxlib.h>
#include <algorithm>
#include <format>

// ── Factory ───────────────────────────────────────────────────────────────────

std::expected<std::unique_ptr<LuaLayer>, std::string> LuaLayer::Create()
{
    return std::unique_ptr<LuaLayer>(new LuaLayer());
}

// ── Construction ──────────────────────────────────────────────────────────────

LuaLayer::LuaLayer()
{
    m_lua.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table,
        sol::lib::package
    );

    // Redirect print() to LoggingLayer using luaL_tolstring so that all types
    // (numbers, booleans, userdata with __tostring) are correctly serialised.
    m_lua.set_function("print", [this](sol::variadic_args Va, sol::this_state Ts)
    {
        lua_State* L = Ts;
        std::string Line;
        for (size_t I = 0; I < Va.size(); ++I)
        {
            if (I > 0) Line += '\t';
            std::size_t Len = 0;
            const char* Str = luaL_tolstring(L, Va[I].stack_index(), &Len);
            if (Str) Line.append(Str, Len);
            lua_pop(L, 1); // pop the string pushed by luaL_tolstring
        }
        Log("[Lua] " + Line);
    });

    registerUGETable();
}

// ── UGE Lua table ─────────────────────────────────────────────────────────────

void LuaLayer::registerUGETable()
{
    auto ugeTable = m_lua.create_named_table("UGE");

    // UGE.AddTickFunction(fn [, name]) -> id
    ugeTable.set_function("AddTickFunction",
        [this](sol::protected_function Fn,
               sol::optional<std::string> Name) -> uint32_t
        {
            return AddTickFunction(std::move(Fn),
                Name.has_value() ? Name->c_str() : "<anonymous>");
        });

    // UGE.RemoveTick(id)
    ugeTable.set_function("RemoveTick",
        [this](uint32_t Id) { RemoveTick(Id); });

    // UGE.RunScript(virtualPath) — execute a VFS Lua file immediately
    ugeTable.set_function("RunScript",
        [this](const std::string& Path)
        {
            if (auto Res = ExecuteFile(Path.c_str()); !Res)
                Log("[UGE] RunScript error: " + Res.error());
        });

    // UGE.ShowTicking() — log all active tick entries
    ugeTable.set_function("ShowTicking",
        [this]()
        {
            if (m_tickEntries.empty())
            {
                Log("[UGE] No active tick entries");
                return;
            }
            Log(std::format("[UGE] Active tick entries ({}):", m_tickEntries.size()));
            for (const auto& Entry : m_tickEntries)
                Log(std::format("  id={} name={}", Entry.id, Entry.name));
        });

    // UGE.GetTickId(name) -> id or 0 if not found
    ugeTable.set_function("GetTickId",
        [this](const std::string& Name) -> uint32_t
        {
            const auto It = std::find_if(m_tickEntries.begin(), m_tickEntries.end(),
                [&Name](const TickEntry& E) { return E.name == Name; });
            return (It != m_tickEntries.end()) ? It->id : 0;
        });
}

// ── IScriptableObject registration ───────────────────────────────────────────

void LuaLayer::Register(IScriptableObject* Obj)
{
    m_objects.push_back(Obj);
    Obj->RegisterObject(m_lua);
}

void LuaLayer::Unregister(IScriptableObject* Obj)
{
    const auto It = std::find(m_objects.begin(), m_objects.end(), Obj);
    if (It == m_objects.end()) return;
    Obj->UnregisterObject(m_lua);
    m_objects.erase(It);
}

// ── Tickable functions ────────────────────────────────────────────────────────

uint32_t LuaLayer::AddTickFunction(sol::protected_function Fn, const char* Name)
{
    // Silently drop if a tick with the same name is already registered.
    const auto Existing = std::find_if(m_tickEntries.begin(), m_tickEntries.end(),
        [Name](const TickEntry& E) { return E.name == Name; });
    if (Existing != m_tickEntries.end())
        return Existing->id;

    const uint32_t Id = m_nextTickId++;
    m_tickEntries.push_back({ Id, std::string(Name), std::move(Fn) });
    Log(std::format("[UGE] Tick registered (fn): {} [id={}]", Name, Id));
    return Id;
}

void LuaLayer::RemoveTick(uint32_t Id)
{
    const auto It = std::find_if(m_tickEntries.begin(), m_tickEntries.end(),
        [Id](const TickEntry& E) { return E.id == Id; });
    if (It == m_tickEntries.end()) return;
    Log(std::format("[UGE] Tick unregistered: {} [id={}]", It->name, Id));
    m_tickEntries.erase(It);
}

// ── AppLayer::Update ──────────────────────────────────────────────────────────

void LuaLayer::Update()
{
    // Snapshot before iterating so a tick callback that calls RemoveTick()
    // mid-loop doesn't invalidate the iterator.
    const auto Snapshot = m_tickEntries;

    for (const auto& Entry : Snapshot)
    {
        const auto Result = Entry.fn();
        if (!Result.valid())
        {
            const sol::error Err = Result;
            Log(std::format("[UGE] Tick fn error (id={}, name={}): {}",
                Entry.id, Entry.name, Err.what()));
        }
    }
}

// ── IEventHandler ─────────────────────────────────────────────────────────────

bool LuaLayer::HandleEvent(SDL_Event& Event)
{
    (void)Event;
    return false;
}

// ── Script execution ──────────────────────────────────────────────────────────

std::expected<void, std::string> LuaLayer::Execute(std::string_view Code)
{
    const auto Result = m_lua.safe_script(Code, sol::script_pass_on_error);
    if (!Result.valid())
    {
        const sol::error Err = Result;
        std::string Msg = std::string("[Lua] Error: ") + Err.what();
        Log(Msg);
        return std::unexpected(std::move(Msg));
    }
    return {};
}

std::expected<void, std::string> LuaLayer::ExecuteFile(const char* VirtualPath)
{
    auto* Fs = ServiceLocator::TryGet<PhysFSLayer>();
    if (!Fs)
        return std::unexpected(
            std::string("LuaLayer::ExecuteFile — PhysFSLayer not available"));

    const auto Bytes = Fs->ReadFile(VirtualPath);
    if (Bytes.empty())
        return std::unexpected(
            std::string("LuaLayer::ExecuteFile — file not found: ") + VirtualPath);

    const char*  Ptr = reinterpret_cast<const char*>(Bytes.data());
    std::size_t  Len = Bytes.size();

    // Strip UTF-8 BOM (EF BB BF) so editors that save with BOM don't break Lua.
    if (Len >= 3 &&
        static_cast<unsigned char>(Ptr[0]) == 0xEF &&
        static_cast<unsigned char>(Ptr[1]) == 0xBB &&
        static_cast<unsigned char>(Ptr[2]) == 0xBF)
    {
        Ptr += 3;
        Len -= 3;
    }

    return Execute(std::string_view{ Ptr, Len });
}

void LuaLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}

