#include "ConfigLoader.h"

#include "ModuleLoader.hpp"
#include <toml.hpp>
#include <filesystem>

namespace
{
    // Read an optional scalar from a toml table into dest, casting as needed.
    // No-ops silently if the key is absent; the default in LaunchSettings is preserved.
    template<typename TomlT, typename DestT = TomlT>
    void readOpt(const toml::table& table, std::string_view key, DestT& dest)
    {
        if (const auto value = table[key].template value<TomlT>())
            dest = static_cast<DestT>(*value);
    }

    // Read a uniform array of strings from a toml table key.
    // Returns an empty vector (not an error) when the key is absent.
    std::expected<std::vector<std::string>, std::string> readStringArray(
        const toml::table& table, std::string_view key, std::string_view context)
    {
        std::vector<std::string> result;
        const auto* arr = table[key].as_array();
        if (!arr) return result;

        result.reserve(arr->size());
        for (const auto& node : *arr)
        {
            if (const auto value = node.value<std::string>())
                result.push_back(*value);
            else
                return std::unexpected(std::string(context) + ": '" + std::string(key) + "' must contain strings");
        }
        return result;
    }

    std::expected<MountPoint, std::string> parseMountPoint(const toml::table& table)
    {
        const auto realPath    = table["RealPath"].value<std::string>();
        const auto virtualPath = table["VirtualPath"].value<std::string>();
        const auto prepend     = table["Prepend"].value<bool>();

        if (!realPath)    return std::unexpected("UGESettings.toml: each MountPoints entry requires string field 'RealPath'");
        if (!virtualPath) return std::unexpected("UGESettings.toml: each MountPoints entry requires string field 'VirtualPath'");
        if (!prepend)     return std::unexpected("UGESettings.toml: each MountPoints entry requires bool field 'Prepend'");

        return MountPoint{ *realPath, *virtualPath, *prepend };
    }
}

std::expected<LaunchSettings, std::string> LoadLaunchSettingsFromAppDirectory()
{
    std::string exeDirError;
    const auto exeDirectory = ModuleLoader::detail::executableDirectory(exeDirError);
    if (!exeDirectory)
        return std::unexpected("Unable to locate executable directory: " + exeDirError);

    const std::filesystem::path settingsPath = (*exeDirectory / "UGESettings.toml").lexically_normal();

    toml::table config;
    try
    {
        config = toml::parse_file(settingsPath.string());
    }
    catch (const toml::parse_error& ex)
    {
        return std::unexpected("Failed to parse '" + settingsPath.string() + "': " + std::string(ex.description()));
    }

    LaunchSettings settings;

    // Scalar fields — preserved defaults when key is absent
    readOpt<std::string>  (config, "WindowTitle",  settings.WindowTitle);
    readOpt<int64_t, int> (config, "WindowWidth",  settings.WindowWidth);
    readOpt<int64_t, int> (config, "WindowHeight", settings.WindowHeight);
    readOpt<std::string>  (config, "InitScript",   settings.InitScript);

    // MountPoints array
    if (const auto* mountArray = config["MountPoints"].as_array())
    {
        settings.MountPoints.clear();
        settings.MountPoints.reserve(mountArray->size());
        for (const auto& node : *mountArray)
        {
            const auto* mountTable = node.as_table();
            if (!mountTable)
                return std::unexpected("UGESettings.toml: 'MountPoints' must contain tables");
            auto mount = parseMountPoint(*mountTable);
            if (!mount) return std::unexpected(mount.error());
            settings.MountPoints.push_back(std::move(*mount));
        }
    }

    // Modules.Load string array
    if (const auto* modulesTable = config["Modules"].as_table())
    {
        auto modules = readStringArray(*modulesTable, "Load", "UGESettings.toml [Modules]");
        if (!modules) return std::unexpected(modules.error());
        settings.Modules = std::move(*modules);
    }

    return settings;
}

