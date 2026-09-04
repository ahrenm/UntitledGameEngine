#pragma once
#include <Physics/PhysicsTypes.h>   // TAG_PHYSICS_GRAVITY_Y, TAG_PHYSICS_PPM
#include <string_view>

// ── Platformer-specific transient tags ────────────────────────────────────────
// Published by PlatformerScene from platformerData.toml.
//
// World physics (gravity, pixels-per-metre) is owned by the engine-level
// PhysicsLayer under the "physics.*" namespace (TAG_PHYSICS_GRAVITY_Y /
// TAG_PHYSICS_PPM from PhysicsTypes.h); the scene overrides those from TOML
// before calling InitPhysics().  The tags below are game-side *movement*
// tunables consumed by PlatformerCharacter — they are not Box2D world params,
// hence the "plat2d.movement.*" namespace (never "plat2d.physics.*").

static constexpr std::string_view TAG_SPEED       = "plat2d.movement.speed";
static constexpr std::string_view TAG_JUMP_HEIGHT = "plat2d.movement.jump_height";

// ── Name plate anchor (screen-space px; written by PlatformerCharacter::Update) ──
// Single atomic Vec2 (top-centre of the sprite) so the nameplate ViewModel
// re-centres exactly once per frame instead of reacting to separate x/y events.
static constexpr std::string_view TAG_NAMEPLATE_POS = "plat2d.namePlate.pos";
