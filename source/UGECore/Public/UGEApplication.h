#pragma once
#include "LaunchSettings.h"
#include "Layers/AppLayer.h"
#include "Layers/SDLLayer.h"
#include "ServiceLocator.h"
#include <expected>
#include <memory>
#include <string>
#include <vector>

class UGEApplication
{
public:
    // ── Launch configuration ──────────────────────────────────────────────────
    // Set these fields before calling Create() to configure the window.
    // Defaults are defined in LaunchSettings.
    static LaunchSettings Settings;

    [[nodiscard]] static std::expected<std::unique_ptr<UGEApplication>, std::string>
    Create(int Argc, char* Argv[], LaunchSettings LaunchConfig);

    ~UGEApplication() { ServiceLocator::Clear(); }

    void Run();

private:
    UGEApplication() = default;

    SDLLayer* m_sdlLayer = nullptr;

    std::vector<std::unique_ptr<AppLayer>> m_layers;

    template<typename T>
    T* pushLayer(std::unique_ptr<T> Layer)
    {
        T* Ptr = Layer.get();
        m_layers.push_back(std::move(Layer));
        // Delegate all ServiceLocator registration to the layer itself.
        Ptr->RegisterWithServiceLocator();
        return Ptr;
    }

    // Dispatch an SDL event to all layers in reverse push order.
    // Stops at the first layer that returns true from HandleEvent().
    void dispatchEvent(SDL_Event& Event);
};