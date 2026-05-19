#pragma once
#include <glm/glm.hpp>

// ── Camera3D ──────────────────────────────────────────────────────────────────
// Plain camera descriptor used by Render3DLayer.
// Mutate Eye / Target / Up / Fov directly:
//
//   auto& cam = render3d->Camera();
//   cam.Eye    = { 0.0f, 1.0f, 5.0f };
//   cam.Target = { 0.0f, 0.0f, 0.0f };
//
struct Camera3D
{
    glm::vec3 Eye    { 0.0f, 0.0f,   5.0f };
    glm::vec3 Target { 0.0f, 0.0f,   0.0f };
    glm::vec3 Up     { 0.0f, 1.0f,   0.0f };
    float     FovDeg { 60.0f         };
    float     NearZ  {  0.1f         };
    float     FarZ   { 1000.0f       };
};

