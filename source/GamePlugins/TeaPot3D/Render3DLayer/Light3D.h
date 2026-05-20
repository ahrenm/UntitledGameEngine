#pragma once
#include <glm/glm.hpp>

// ── Light3D ───────────────────────────────────────────────────────────────────
// Directional light descriptor used by Render3DLayer.
// Direction is a world-space vector pointing FROM the surface TOWARD the light.
// Color channels are in [0, 1] linear range.
//
//   auto& light = render3d->Light();
//   light.Direction = glm::normalize({ 1.0f, -1.0f, -0.5f });
//   light.Color     = { 1.0f, 0.9f, 0.8f };
//   light.Ambient   = { 0.1f, 0.1f, 0.15f };
//
struct Light3D
{
    glm::vec3 Direction { 0.5f, -1.0f, -0.5f }; // world space; normalised internally
    glm::vec3 Color     { 1.0f,  1.0f,  1.0f }; // RGB diffuse, [0,1]
    glm::vec3 Ambient   { 0.15f, 0.15f, 0.20f }; // RGB ambient, [0,1]
};

