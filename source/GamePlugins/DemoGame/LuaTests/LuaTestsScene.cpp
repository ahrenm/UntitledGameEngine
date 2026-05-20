#include <LuaTestsScene.h>
#include <Layers/RmlUILayer.h>
#include <Layers/UGEDataLayer.h>
#include <ServiceLocator.h>

LuaTestsScene::LuaTestsScene(SDL_Renderer* Renderer, SDL_Window* Window)
    : SceneObject(Renderer, Window)
{
    // Initialise direction to Right (1) before the ViewModel is created.
    if (auto* Data = ServiceLocator::TryGet<UGEDataLayer>())
        Data->Transient.Set("luaTests.coinDirection", AppStateValue{1});

    GetSDLLayer()->SetBackground("assets/luaBG.jpg");
    if (auto* UI = GetUILayer())
        UI->LoadDocument("assets/ui/lua_tests.rml");

    GetLuaLayer()->ExecuteFile("assets/lua/tickingTest.lua");
}
