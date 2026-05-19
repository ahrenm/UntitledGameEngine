#pragma once

#include <LaunchSettings.h>
#include <expected>
#include <string>

// Reads UGESettings.toml from the executable directory and maps it to LaunchSettings.
[[nodiscard]] std::expected<LaunchSettings, std::string> LoadLaunchSettingsFromAppDirectory();

