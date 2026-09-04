#pragma once
#include <optional>
#include <string_view>

// ── Physics transient tag branch ──────────────────────────────────────────────
// These keys live in UGEDataLayer under the "physics.*" namespace (transient
// meta).  PhysicsLayer seeds them from LaunchSettings on startup; scenes can
// override individual keys before calling InitPhysics() to customise the world.
//
// Runtime tunability contract
// ---------------------------
//   LIVE  — physics.gravity.x / physics.gravity.y  (m/s²)
//           Changing these while a world is active updates the running Box2D
//           simulation immediately: PhysicsLayer's DataBinding fires and calls
//           b2World_SetGravity().  Tune from Lua via either the raw store key
//           (Data.Set('physics.gravity.y', -20)) or the sugar Physics.SetGravity(x, y).
//
//   INIT  — physics.pixels_per_meter  (px/m)
//           Box2D bakes body/shape sizes (in metres) at CreateBody() time, so
//           PPM cannot change while a world is live.  Writing it is honoured on
//           the NEXT InitPhysics(); Physics.SetPixelsPerMeter() refuses while a
//           world exists and logs a warning.
static constexpr std::string_view TAG_PHYSICS_GRAVITY_X = "physics.gravity.x";
static constexpr std::string_view TAG_PHYSICS_GRAVITY_Y = "physics.gravity.y";
static constexpr std::string_view TAG_PHYSICS_PPM       = "physics.pixels_per_meter";

// ── Debug transient tag ────────────────────────────────────────────────────────
// Shared key used by both PhysicsLayer and SDLLayer so that a single toggle
// activates the physics body debug overlay.  int value: 1 = visible, 0 = hidden.
// Write via Lua:  Physics.DebugDraw(true/false)  or  SDL.ShowCollision()
static constexpr std::string_view TAG_DEBUG_SHOW_COLLISION = "debug.show_collision";

// ── BodyType ──────────────────────────────────────────────────────────────────
enum class BodyType
{
    Static,     // immovable; used for terrain and walls
    Dynamic,    // gravity + collision response; used for player and projectiles
    Kinematic,  // velocity-driven, no gravity; used for moving platforms
    Sensor      // static sensor shape; generates contact events, no collision response
};

// ── BodyDef ───────────────────────────────────────────────────────────────────
// Parameters for PhysicsLayer::CreateBody().
// X, Y = bottom-left corner in Y-up world pixels.  W, H = size in world pixels.
// UserData is stored on the Box2D shape so contact events can identify the owner.
// LIFETIME CONTRACT: The object pointed to by UserData must outlive the body.
struct BodyDef
{
    float    X             = 0.0f;
    float    Y             = 0.0f;
    float    W             = 0.0f;
    float    H             = 0.0f;
    BodyType Type          = BodyType::Static;
    float    Density       = 1.0f;
    float    Friction      = 0.3f;
    float    Restitution   = 0.0f;
    void*    UserData      = nullptr;
    bool     FixedRotation = true;   // prevents the body from rotating (recommended for characters)
};

// ── PhysicsWorldCreateParams ──────────────────────────────────────────────────
// Optional parameters for InitPhysics().  Gravity and pixels-per-metre are NOT
// here — they are always read from the physics.* transient tag branch, which is
// seeded from LaunchSettings and may be overridden by the scene.
struct PhysicsWorldCreateParams
{
    int  SubSteps       = 8;     // Box2D velocity sub-steps per tick (higher = more accurate)
    bool EnableSleeping = true;  // allow static bodies to sleep when undisturbed
};

// ── ContactPhase ──────────────────────────────────────────────────────────────
enum class ContactPhase { Begin, End };

// ── ContactEvent ──────────────────────────────────────────────────────────────
// Delivered to the callback registered with PhysicsLayer::SetContactCallback().
// Sensor contacts only (Box2D 3.x raises begin/end events for sensor shapes).
// UserDataA/B are the BodyDef.UserData values from each shape's parent body.
struct ContactEvent
{
    void*        UserDataA = nullptr;
    void*        UserDataB = nullptr;
    ContactPhase Phase     = ContactPhase::Begin;
};
