#pragma once
#include <string_view>

static constexpr std::string_view TAG_GRAVITY        = "plat2d.physics.gravity";
static constexpr std::string_view TAG_GROUND_EPSILON = "plat2d.physics.ground_epsilon";
static constexpr std::string_view TAG_SPEED          = "plat2d.physics.speed";
static constexpr std::string_view TAG_JUMP_HEIGHT    = "plat2d.physics.jump_height";

// ── Name plate anchor (reference-space px; written by PlatformerCharacter::Update) ──
static constexpr std::string_view TAG_NAMEPLATE_X    = "plat2d.namePlate.posX";
static constexpr std::string_view TAG_NAMEPLATE_Y    = "plat2d.namePlate.posY";
