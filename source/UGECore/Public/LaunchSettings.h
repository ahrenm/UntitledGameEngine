#pragma once
#include <string>
#include <vector>

// ── MountPoint ────────────────────────────────────────────────────────────────
// Describes a real filesystem path and its virtual filesystem mount point.
struct MountPoint
{
    std::string RealPath;       // Path on disk (relative to executable directory or absolute)
    std::string VirtualPath;    // Virtual path in PhysFS mount hierarchy (typically "/" or "/assets")
    bool        Prepend = false; // If true, prepends to VFS search path (override priority)
};

// ── LaunchSettings ────────────────────────────────────────────────────────────
// Plain settings struct populated before UGEApplication::Create() is called.
// Holds the window configuration, command-line parameters, and virtual filesystem
// mount configuration used at startup.
//
// Usage — configure in main() before calling UGEApplication::Create():
//
//   UGEApplication::Settings.WindowTitle   = "My Game";
//   UGEApplication::Settings.WindowWidth   = 1600;
//   UGEApplication::Settings.WindowHeight  = 1200;
//   UGEApplication::Settings.ExecutablePath = argv[0];
//   // Optionally customize MountPoints (see defaults in UGEApplication::Create)
//   auto app = UGEApplication::Create(argc, argv);
struct LaunchSettings
{
    std::string WindowTitle             = "Untitled Game Engine";
    int         WindowWidth             = 1600;
    int         WindowHeight            = 1200;
    // Logical render reference resolution.  0 = inherit from WindowWidth/WindowHeight.
    // SDL_SetRenderLogicalPresentation is configured with these values so all scene
    // render calls work in reference pixels regardless of the actual window size.
    int         RefWidth                = 0;
    int         RefHeight               = 0;
    // ── Physics defaults (used to seed physics.* transient tag branch) ────────
    // PhysicsLayer reads these on startup and writes them to transient state.
    // Scenes can override individual keys before calling InitPhysics().
    float       PhysicsPixelsPerMeter   = 100.0f;  // world pixels per Box2D metre
    float       PhysicsDefaultGravityX  = 0.0f;   // m/s²
    float       PhysicsDefaultGravityY  = -9.81f; // m/s²  (negative = downward in Y-up)
    const char* ExecutablePath          = nullptr;  // argv[0] — required by PhysFSLayer
    std::string InitScript              = "assets/lua/initApp.lua"; // Executed at end of Create()
    std::vector<MountPoint> MountPoints;            // Virtual filesystem mount configuration
    std::vector<std::string> Modules;               // Platform-independent module names to load at startup
};

