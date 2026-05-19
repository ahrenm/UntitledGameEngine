#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <cerrno>
#include <cstring>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
#endif

namespace ModuleLoader
{
    namespace detail
    {
        inline std::mutex g_loadedModulesMutex;

#if defined(_WIN32)
        using ModuleHandle = HMODULE;
#else
        using ModuleHandle = void*;
#endif

        inline std::unordered_map<std::string, ModuleHandle> g_loadedModules;

        inline std::string pathToKey(const std::filesystem::path& path)
        {
            const auto normalized = path.lexically_normal().u8string();
            return {normalized.begin(), normalized.end()};
        }

#if defined(_WIN32)
        inline std::string lastPlatformError()
        {
            const DWORD errorCode = ::GetLastError();
            if (errorCode == 0)
            {
                return "unknown error";
            }

            LPSTR messageBuffer = nullptr;
            const DWORD size = ::FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                errorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&messageBuffer),
                0,
                nullptr
            );

            std::string message = (size > 0 && messageBuffer != nullptr)
                ? std::string(messageBuffer, size)
                : std::string("unknown error");

            if (messageBuffer != nullptr)
            {
                ::LocalFree(messageBuffer);
            }

            while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
            {
                message.pop_back();
            }

            return message;
        }

        inline std::optional<std::filesystem::path> executableDirectory(std::string& outError)
        {
            std::vector<wchar_t> buffer(MAX_PATH, L'\0');

            for (;;)
            {
                const DWORD copied = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (copied == 0)
                {
                    outError = "GetModuleFileNameW failed: " + lastPlatformError();
                    return std::nullopt;
                }

                if (copied < buffer.size() - 1)
                {
                    return std::filesystem::path(std::wstring(buffer.data(), copied)).parent_path();
                }

                buffer.resize(buffer.size() * 2);
            }
        }
#else
        inline std::string lastPlatformError()
        {
            if (const char* error = ::dlerror(); error != nullptr)
            {
                return std::string(error);
            }

            return "unknown error";
        }

        inline std::optional<std::filesystem::path> executableDirectory(std::string& outError)
        {
#if defined(__APPLE__)
            uint32_t required = 0;
            (void)::_NSGetExecutablePath(nullptr, &required);
            if (required == 0)
            {
                outError = "_NSGetExecutablePath returned an empty size";
                return std::nullopt;
            }

            std::vector<char> buffer(required + 1, '\0');
            if (::_NSGetExecutablePath(buffer.data(), &required) != 0)
            {
                outError = "_NSGetExecutablePath failed";
                return std::nullopt;
            }

            return std::filesystem::path(buffer.data()).parent_path();
#else
            std::vector<char> buffer(1024, '\0');

            for (;;)
            {
                const ssize_t copied = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
                if (copied < 0)
                {
                    outError = std::string("readlink(/proc/self/exe) failed: ") + std::strerror(errno);
                    return std::nullopt;
                }

                if (static_cast<size_t>(copied) < buffer.size() - 1)
                {
                    buffer[static_cast<size_t>(copied)] = '\0';
                    return std::filesystem::path(buffer.data()).parent_path();
                }

                buffer.resize(buffer.size() * 2);
            }
#endif
        }
#endif

        inline bool loadAbsoluteModulePath(const std::filesystem::path& absolutePath, std::string& outError)
        {
            const std::string key = pathToKey(absolutePath);

            const std::lock_guard<std::mutex> lock(g_loadedModulesMutex);
            if (g_loadedModules.contains(key))
            {
                return true;
            }

#if defined(_WIN32)
            const std::wstring widePath = absolutePath.wstring();
            ModuleHandle handle = ::LoadLibraryW(widePath.c_str());
            if (handle == nullptr)
            {
                outError = "LoadLibraryW failed for '" + key + "': " + lastPlatformError();
                return false;
            }
#else
            ModuleHandle handle = ::dlopen(absolutePath.string().c_str(), RTLD_NOW | RTLD_LOCAL);
            if (handle == nullptr)
            {
                outError = "dlopen failed for '" + key + "': " + lastPlatformError();
                return false;
            }
#endif

            g_loadedModules.emplace(key, handle);
            return true;
        }
    }

    // Loads a module from a path relative to the running executable.
    // Repeated calls for the same normalized path are no-ops.
    inline bool LoadModuleRelativeToExecutable(const char* relativePath, std::string& outError)
    {
        outError.clear();

        if (relativePath == nullptr || relativePath[0] == '\0')
        {
            outError = "relative path is empty";
            return false;
        }

        const std::filesystem::path relative(relativePath);
        if (relative.is_absolute())
        {
            outError = "path must be relative to the executable";
            return false;
        }

        std::string directoryError;
        const auto exeDirectory = detail::executableDirectory(directoryError);
        if (!exeDirectory)
        {
            outError = directoryError;
            return false;
        }

        const auto absolutePath = (*exeDirectory / relative).lexically_normal();
        return detail::loadAbsoluteModulePath(absolutePath, outError);
    }

    inline bool LoadModuleRelativeToExecutable(const char* relativePath)
    {
        std::string ignoredError;
        return LoadModuleRelativeToExecutable(relativePath, ignoredError);
    }

    // Converts a platform-independent module name to a platform-specific filename.
    // E.g., "UIWindows" -> "UIWindows.dll" (Windows), "UIWindows.dylib" (macOS), "UIWindows.so" (Linux)
    inline std::string GetPlatformSpecificModuleName(const std::string& baseName)
    {
#if defined(_WIN32)
        return baseName + ".dll";
#elif defined(__APPLE__)
        return baseName + ".dylib";
#else
        return baseName + ".so";
#endif
    }

    // Loads multiple modules from the application directory.
    // Each name in moduleNames is converted to platform-specific filename and loaded.
    // Returns false if any module fails to load; outError contains the first error encountered.
    inline bool LoadModulesRelativeToExecutable(const std::vector<std::string>& moduleNames, std::string& outError)
    {
        outError.clear();

        for (const auto& baseName : moduleNames)
        {
            const auto platformName = GetPlatformSpecificModuleName(baseName);
            if (!LoadModuleRelativeToExecutable(platformName.c_str(), outError))
            {
                return false;
            }
        }

        return true;
    }
}

