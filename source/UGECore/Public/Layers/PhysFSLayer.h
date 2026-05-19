#pragma once
#include "AppLayer.h"
#include "../IScriptableObject.h"
#include "../LayerRegistry.h"
#include <physfs.h>
#include <SDL3/SDL.h>
#include <expected>
#include <memory>
#include <string>
#include <vector>

// ── PhysFSLayer ───────────────────────────────────────────────────────────────
// AppLayer that owns the PhysicsFS virtual filesystem lifetime.
// Construct and push into Application::layers_ immediately after LoggingLayer.
// Access anywhere via ServiceLocator::get<PhysFSLayer>().
//
// Load order: 2 — must follow LoggingLayer (1).
// Reads ExecutablePath from UGEApplication::Settings at initialization time.
class PhysFSLayer : public AppLayer, public IScriptableObject
{
public:
    REGISTER_LAYER("physfs", 2.0f, PhysFSLayer)

    [[nodiscard]] static std::expected<std::unique_ptr<PhysFSLayer>, std::string>
    Create();

    // Deinitialises PhysicsFS on destruction.
    ~PhysFSLayer() override;

    // Non-copyable
    PhysFSLayer(const PhysFSLayer&)            = delete;
    PhysFSLayer& operator=(const PhysFSLayer&) = delete;

    // Mount an archive or directory into the virtual filesystem.
    // virtualPath : destination path in the VFS (e.g. "/assets")
    // prepend     : if true the mount takes priority over existing mounts at the same path
    bool Mount(const char* RealPath, const char* VirtualPath, bool Prepend = false);

    // Unmount a previously mounted archive or directory.
    bool Unmount(const char* RealPath);

    // Read an entire file from the VFS into an SDL_IOStream (caller does NOT need to free the buffer).
    // Returns nullptr on failure.
    SDL_IOStream*          OpenAsIOStream(const char* VirtualPath) const;

    // Read an entire file from the VFS into a byte vector.
    // Returns an empty vector on failure.
    std::vector<std::byte> ReadFile(const char* VirtualPath) const;

    // Returns true if the given virtual path exists in the VFS.
    bool                   Exists(const char* VirtualPath) const;

    // Recursively enumerate all files under a virtual directory.
    // Pass "" or "/" for the root.
    std::vector<std::string> ListFiles(const char* VirtualDir) const;

    // Log all files under virtualDir to LoggingLayer (recursive).
    void                   LogFiles(const char* VirtualDir = "") const;

    // Extract a virtual asset to ApplicationDirectory/OVERRIDE/{VirtualPath} on disk.
    // Uses PHYSFS_getRealDir to detect when the source and destination resolve to the
    // same real file (e.g. a previously-dumped asset that was remounted) and cancels
    // gracefully in that case.  All outcomes including failure and the same-path skip
    // are written to the log.  Returns true on success, false otherwise.
    bool                   DumpAsset(const char* VirtualPath) const;

    // ── AppLayer ──────────────────────────────────────────────────────────
    void RegisterWithServiceLocator() override;

    // ── IScriptableObject ─────────────────────────────────────────────────────
    // Extends the UGE Lua table with:
    //   UGE.DumpAsset(virtualPath: string) -> bool
    void RegisterObject(sol::state& Lua) override;

private:
    PhysFSLayer();

    static const char* lastError();
    void listFilesRecursive(const char* Dir, std::vector<std::string>& Out) const;
};
