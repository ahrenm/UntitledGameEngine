#include <StateAssetsDemoScene.h>
#include <StateAssetsDemoViewModel.h>
#include <filesystem>

StateAssetsDemoScene::StateAssetsDemoScene(SDL_Renderer* Renderer, SDL_Window* Window)
    : SceneObject(Renderer, Window)
{
    GetSDLLayer()->SetBackground("assets/dataBG.jpg");

    // If Game.sav already exists on disk, prime the transient so the ViewModel
    // shows the Load button as soon as the document is registered.
    if (std::filesystem::exists("Game.sav"))
        if (auto* Data = GetDataLayer())
            Data->Transient.Set(std::string(StateAssetsDemoViewModel::KEY_LOAD_VISIBLE),
                                AppStateValue{1});

    if (auto* UI = GetUILayer())
        UI->LoadDocument("assets/ui/state_assets_demo.rml");
}

