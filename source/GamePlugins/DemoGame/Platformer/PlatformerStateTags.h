#pragma once
#include <Physics/PhysicsTypes.h>   // TAG_PHYSICS_GRAVITY_Y, TAG_PHYSICS_PPM
#include <string_view>

// ── Platformer-specific transient tags ────────────────────────────────────────
// Published by PlatformerScene from platformerData.toml.
// TAG_PHYSICS_GRAVITY_Y / TAG_PHYSICS_PPM come from PhysicsTypes.h and are
// managed by PhysicsLayer (engine-level); the scene overrides them from TOML
// before calling InitPhysics().

static constexpr std::string_view TAG_SPEED       = "plat2d.physics.speed";
static constexpr std::string_view TAG_JUMP_HEIGHT = "plat2d.physics.jump_height";

// ── Name plate anchor (screen-space px; written by PlatformerCharacter::Update) ──
static constexpr std::string_view TAG_NAMEPLATE_X = "plat2d.namePlate.posX";
static constexpr std::string_view TAG_NAMEPLATE_Y = "plat2d.namePlate.posY";
