#include <Layers/PhysFSLayer.h>
#include <ServiceLocator.h>
#include <UGEApplication.h>
#include <filesystem>
#include <fstream>
#include <format>

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::expected<std::unique_ptr<PhysFSLayer>, std::string> PhysFSLayer::Create()
{
    auto Layer = std::unique_ptr<PhysFSLayer>(new PhysFSLayer());
    if (!PHYSFS_isInit())
        return std::unexpected(std::format("PhysFSLayer: init failed: {}", lastError()));

    // Mount all configured paths from LaunchSettings
    for (const auto& Mount : UGEApplication::Settings.MountPoints)
    {
        // Resolve relative paths against the PhysFS base dir (exe directory)
        std::filesystem::path RealPath(Mount.RealPath);
        if (RealPath.is_relative())
            RealPath = std::filesystem::path(PHYSFS_getBaseDir()) / RealPath;

        if (!std::filesystem::exists(RealPath))
        {
            Layer->Log(std::format("PhysFSLayer: skipping mount '{}' -> '{}' (path does not exist)",
                Mount.RealPath, Mount.VirtualPath));
            continue;
        }

        if (!Layer->Mount(Mount.RealPath.c_str(), Mount.VirtualPath.c_str(), Mount.Prepend))
        {
            return std::unexpected(std::format("PhysFSLayer: failed to mount '{}' -> '{}'",
                Mount.RealPath, Mount.VirtualPath));
        }

        Layer->Log(std::format("PhysFSLayer: mounted '{}' -> '{}' (prepend={})",
            Mount.RealPath, Mount.VirtualPath, Mount.Prepend));
    }

    return Layer;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

PhysFSLayer::PhysFSLayer()
{
    PHYSFS_init(UGEApplication::Settings.ExecutablePath);
}

PhysFSLayer::~PhysFSLayer()
{
    PHYSFS_deinit();
}

// ---------------------------------------------------------------------------
// Mounting
// ---------------------------------------------------------------------------

bool PhysFSLayer::Mount(const char* RealPath, const char* VirtualPath, bool Prepend)
{
    if (!PHYSFS_mount(RealPath, VirtualPath, Prepend ? 0 : 1))
    {
        Log(std::format("PhysFSLayer: mount('{}' -> '{}') failed: {}", RealPath, VirtualPath, lastError()));
        return false;
    }
    return true;
}

bool PhysFSLayer::Unmount(const char* RealPath)
{
    if (!PHYSFS_unmount(RealPath))
    {
        Log(std::format("PhysFSLayer: unmount('{}') failed: {}", RealPath, lastError()));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

SDL_IOStream* PhysFSLayer::OpenAsIOStream(const char* VirtualPath) const
{
    PHYSFS_File* File = PHYSFS_openRead(VirtualPath);
    if (!File)
    {
        Log(std::format("PhysFSLayer: openRead('{}') failed: {}", VirtualPath, lastError()));
        return nullptr;
    }

    const PHYSFS_sint64 Len = PHYSFS_fileLength(File);
    void* Buf = SDL_malloc(static_cast<size_t>(Len));
    if (!Buf)
    {
        PHYSFS_close(File);
        Log(std::format("PhysFSLayer: SDL_malloc failed for '{}'", VirtualPath));
        return nullptr;
    }

    PHYSFS_readBytes(File, Buf, static_cast<PHYSFS_uint64>(Len));
    PHYSFS_close(File);

    // SDL_IOFromConstMem does not take ownership; SDL will free when closed
    return SDL_IOFromConstMem(Buf, static_cast<size_t>(Len));
}

std::vector<std::byte> PhysFSLayer::ReadFile(const char* VirtualPath) const
{
    PHYSFS_File* File = PHYSFS_openRead(VirtualPath);
    if (!File)
    {
        Log(std::format("PhysFSLayer: readFile('{}') failed: {}", VirtualPath, lastError()));
        return {};
    }

    const PHYSFS_sint64 Len = PHYSFS_fileLength(File);
    std::vector<std::byte> Buffer(static_cast<size_t>(Len));
    PHYSFS_readBytes(File, Buffer.data(), static_cast<PHYSFS_uint64>(Len));
    PHYSFS_close(File);
    return Buffer;
}

// ---------------------------------------------------------------------------
// Querying
// ---------------------------------------------------------------------------

bool PhysFSLayer::Exists(const char* VirtualPath) const
{
    return PHYSFS_exists(VirtualPath) != 0;
}

std::vector<std::string> PhysFSLayer::ListFiles(const char* VirtualDir) const
{
    std::vector<std::string> Result;
    listFilesRecursive(VirtualDir, Result);
    return Result;
}

void PhysFSLayer::LogFiles(const char* VirtualDir) const
{
    for (const auto& Path : ListFiles(VirtualDir))
    {
        PHYSFS_Stat Stat{};
        PHYSFS_stat(Path.c_str(), &Stat);
        Log(std::format("Asset: {} ({} bytes)", Path, Stat.filesize));
    }
}

// ---------------------------------------------------------------------------
// DumpAsset
// ---------------------------------------------------------------------------

bool PhysFSLayer::DumpAsset(const char* VirtualPath) const
{
    // ── 1. Validate the path exists in the VFS ────────────────────────────────
    if (!Exists(VirtualPath))
    {
        Log(std::format("DumpAsset: '{}' not found in virtual filesystem.", VirtualPath));
        return false;
    }

    // ── 2. Build destination path: {exeDir}/OVERRIDE/{VirtualPath} ────────────
    // SDL_GetBasePath() uses GetModuleFileNameW internally and is reliable
    // regardless of how the application was launched (Explorer, CLI, IDE).
    // PHYSFS_getBaseDir() derives from argv[0] and can return a bare drive root
    // (e.g. "C:\") when the WIN32 subsystem is active and argv[0] has no path.
    const char* BaseDirCStr = SDL_GetBasePath();
    if (!BaseDirCStr)
    {
        Log("DumpAsset: SDL_GetBasePath() returned null.");
        return false;
    }

    // Strip leading separators from VirtualPath.  std::filesystem::path::operator/
    // treats a right-hand operand that starts with '/' as an absolute path and
    // silently discards everything accumulated on the left — killing both the base
    // directory and the OVERRIDE component.
    const char* RelVPath = VirtualPath;
    while (*RelVPath == '/' || *RelVPath == '\\')
        ++RelVPath;

    const std::filesystem::path DestPath =
        std::filesystem::path(BaseDirCStr) / "OVERRIDE" / RelVPath;

    // ── 3. Same-location guard ────────────────────────────────────────────────
    // PHYSFS_getRealDir returns the archive or directory root that contains the
    // file.  For a directory mount, realDir/VirtualPath IS the real file on disk.
    // For a zip/archive mount the composed path won't exist on disk, so
    // std::filesystem::exists returns false and the guard passes harmlessly.
    if (const char* RealDirCStr = PHYSFS_getRealDir(VirtualPath))
    {
        const std::filesystem::path CandidateSrc =
            std::filesystem::path(RealDirCStr) / RelVPath;

        std::error_code ExistsEc;
        if (std::filesystem::exists(CandidateSrc, ExistsEc))
        {
            std::error_code Ec1, Ec2;
            const auto CanonSrc  = std::filesystem::weakly_canonical(CandidateSrc, Ec1);
            const auto CanonDest = std::filesystem::weakly_canonical(DestPath,     Ec2);
            if (!Ec1 && !Ec2 && CanonSrc == CanonDest)
            {
                Log(std::format(
                    "DumpAsset: '{}' source and destination are the same file ({}), skipping.",
                    VirtualPath, CanonDest.string()));
                return false;
            }
        }
    }

    // ── 4. Read the asset into memory ─────────────────────────────────────────
    PHYSFS_Stat Stat{};
    PHYSFS_stat(VirtualPath, &Stat);
    const auto Bytes = ReadFile(VirtualPath);
    if (Bytes.empty() && Stat.filesize > 0)
        return false;   // ReadFile already logged the error

    // ── 5. Create intermediate directories ────────────────────────────────────
    std::error_code DirEc;
    std::filesystem::create_directories(DestPath.parent_path(), DirEc);
    if (DirEc)
    {
        Log(std::format("DumpAsset: failed to create directories for '{}': {}",
            DestPath.string(), DirEc.message()));
        return false;
    }

    // ── 6. Write to disk ──────────────────────────────────────────────────────
    std::ofstream Out(DestPath, std::ios::binary | std::ios::trunc);
    if (!Out)
    {
        Log(std::format("DumpAsset: failed to open '{}' for writing.", DestPath.string()));
        return false;
    }
    Out.write(reinterpret_cast<const char*>(Bytes.data()),
              static_cast<std::streamsize>(Bytes.size()));
    if (!Out)
    {
        Log(std::format("DumpAsset: write error on '{}'.", DestPath.string()));
        return false;
    }
    Out.close();

    Log(std::format("DumpAsset: '{}' → '{}' ({} bytes)",
        VirtualPath, DestPath.string(), Bytes.size()));
    return true;
}

// ---------------------------------------------------------------------------
// IScriptableObject
// ---------------------------------------------------------------------------

void PhysFSLayer::RegisterObject(sol::state& Lua)
{
    auto Uge = Lua["UGE"].get_or_create<sol::table>();

    Uge.set_function("DumpAsset", [this](const std::string& Path) -> bool
    {
        return DumpAsset(Path.c_str());
    });
}

void PhysFSLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void PhysFSLayer::listFilesRecursive(const char* Dir, std::vector<std::string>& Out) const
{
    char** Files = PHYSFS_enumerateFiles(Dir);
    for (char** F = Files; *F != nullptr; ++F)
    {
        std::string FullPath = std::string(Dir) + "/" + *F;
        // Normalise root-level paths (avoid leading "/")
        if (FullPath.starts_with("//"))
            FullPath = FullPath.substr(1);

        PHYSFS_Stat Stat{};
        PHYSFS_stat(FullPath.c_str(), &Stat);

        if (Stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
            listFilesRecursive(FullPath.c_str(), Out);
        else
            Out.push_back(FullPath);
    }
    PHYSFS_freeList(Files);
}

const char* PhysFSLayer::lastError()
{
    return PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
}
