#include "ConfigLoader.h"

#include "ModuleLoader.hpp"
#include <toml.hpp>
#include <filesystem>

namespace
{
    std::expected<MountPoint, std::string> parseMountPoint(const toml::table& table)
    {
        MountPoint mount;

        const auto realPath = table["RealPath"].value<std::string>();
        const auto virtualPath = table["VirtualPath"].value<std::string>();
        const auto prepend = table["Prepend"].value<bool>();

        if (!realPath)
            return std::unexpected("UGESettings.toml: each MountPoints entry requires string field 'RealPath'");
        if (!virtualPath)
            return std::unexpected("UGESettings.toml: each MountPoints entry requires string field 'VirtualPath'");
        if (!prepend)
            return std::unexpected("UGESettings.toml: each MountPoints entry requires bool field 'Prepend'");

        mount.RealPath = *realPath;
        mount.VirtualPath = *virtualPath;
        mount.Prepend = *prepend;

        return mount;
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

    if (const auto value = config["WindowTitle"].value<std::string>())
        settings.WindowTitle = *value;
    if (const auto value = config["WindowWidth"].value<int64_t>())
        settings.WindowWidth = static_cast<int>(*value);
    if (const auto value = config["WindowHeight"].value<int64_t>())
        settings.WindowHeight = static_cast<int>(*value);
    if (const auto value = config["InitScript"].value<std::string>())
        settings.InitScript = *value;

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
            if (!mount)
                return std::unexpected(mount.error());

            settings.MountPoints.push_back(std::move(*mount));
        }
    }

    if (const auto* modulesTable = config["Modules"].as_table())
    {
        if (const auto* loadArray = (*modulesTable)["Load"].as_array())
        {
            settings.Modules.clear();
            settings.Modules.reserve(loadArray->size());

            for (const auto& node : *loadArray)
            {
                if (const auto value = node.value<std::string>())
                {
                    settings.Modules.push_back(*value);
                }
                else
                {
                    return std::unexpected("UGESettings.toml: 'Modules.Load' must contain strings");
                }
            }
        }
    }

    return settings;
}

