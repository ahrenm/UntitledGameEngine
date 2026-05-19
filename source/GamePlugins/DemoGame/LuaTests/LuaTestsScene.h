#pragma once
#include <GameClasses/SceneObject.h>
#include <SceneRegistry.h>

// ── LuaTestsScene ─────────────────────────────────────────────────────────────
// Loads the Lua test runner UI document (assets/ui/lua_tests.rml).
// Activate via SDL.LoadScene("lua-tests") from Lua or
// SDLLayer::LoadScene("lua-tests") from C++.
//
// The test runner UI is driven entirely by LuaTestsViewModel (data-model="lua-tests").
// Press the Run button in the UI to execute assets/lua/lua_tests.lua and
// view per-test pass/fail results.
class LuaTestsScene : public SceneObject
{
public:
    REGISTER_SCENE("lua-tests", LuaTestsScene)

    LuaTestsScene(SDL_Renderer* Renderer, SDL_Window* Window);
};

