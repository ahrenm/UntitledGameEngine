#include <StateAssetsDemoScene.h>
#include "StateAssetsDemoKeys.h"
#include <cstddef>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

using namespace StateAssetsDemoKeys;   // KEY_* shared DataStore keys

StateAssetsDemoScene::StateAssetsDemoScene(Renderer2D* Renderer, SDL_Window* Window)
    : SceneObject(Renderer, Window)
{
    GetSDLLayer()->SetBackground("assets/dataBG.jpg");

    if (auto* Data = GetDataLayer())
    {
        // Save-file detection drives the Load button visibility through a
        // scene-tagged transient the ViewModel subscribes to.
        Data->Store.Set(std::string(KEY_LOAD_VISIBLE),
                        DataValue{ std::filesystem::exists("Game.sav") ? 1 : 0 });

        // UI → Scene triggers: the ViewModel bumps these transients on button
        // clicks; DataStore::Set always notifies, so a repeat click re-fires.
        m_saveRequestBinding = DATA_BIND(KEY_SAVE_REQUEST.data(), 0,
            [this](const Tag&, const DataValue&) { handleSaveRequest(); }, Transient);
        m_loadRequestBinding = DATA_BIND(KEY_LOAD_REQUEST.data(), 0,
            [this](const Tag&, const DataValue&) { handleLoadRequest(); }, Transient);
    }

    if (auto* UI = GetUILayer())
        UI->LoadDocument("assets/ui/state_assets_demo.rml");
}

// ── setStatus ───────────────────────────────────────────────────────────────────

void StateAssetsDemoScene::setStatus(std::string_view Msg)
{
    if (auto* Data = GetDataLayer())
        Data->Store.Set(std::string(KEY_STATUS), DataValue{ std::string(Msg) });
}

// ── handleSaveRequest ───────────────────────────────────────────────────────────

void StateAssetsDemoScene::handleSaveRequest()
{
    auto* Data = GetDataLayer();
    if (!Data) return;
    auto& Store = Data->Store;

    // The ViewModel has already pushed the character.* fields into the store;
    // the scene's job is to validate the numeric fields and serialise to disk.
    auto readStr = [&Store](std::string_view Key) -> std::string
    {
        if (const DataValue* V = Store.Get(Key))
            if (const auto* S = V->TryAs<std::string>()) return *S;
        return {};
    };

    auto isInteger = [](const std::string& S) -> bool
    {
        if (S.empty()) return false;
        try { std::size_t Pos = 0; (void)std::stoi(S, &Pos); return Pos == S.size(); }
        catch (const std::exception&) { return false; }
    };

    if (!isInteger(readStr(KEY_CHARACTER_STRENGTH)))
    { setStatus("Error: Strength must be a whole number."); return; }
    if (!isInteger(readStr(KEY_CHARACTER_MANA)))
    { setStatus("Error: Mana must be a whole number."); return; }

    // ── Serialise every persistent (character.*) entry to Game.sav ────────────
    if (!Data->Save("Game.sav")) { setStatus("Error: Could not write Game.sav."); return; }

    Store.Set(std::string(KEY_LOAD_VISIBLE), DataValue{1});
    setStatus("Saved to Game.sav.");
}

// ── handleLoadRequest ───────────────────────────────────────────────────────────

void StateAssetsDemoScene::handleLoadRequest()
{
    auto* Data = GetDataLayer();
    if (!Data) return;

    // Load repopulates the character.* / character.data.* entries via Store.Set,
    // which fires the ViewModel's data bindings — the UI refreshes itself, so no
    // manual form push is needed here.
    if (!Data->Load("Game.sav")) { setStatus("Error: Could not read Game.sav."); return; }

    setStatus("Loaded from Game.sav. (Mini-bonus: go back to platfomer level)");
}

