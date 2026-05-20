#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>

// ── Material3D ────────────────────────────────────────────────────────────────
// Per-face Phong material coefficients loaded from a .mtl file.
// Channels in [0, 1] linear range.
struct Material3D
{
    glm::vec3 Ambient   { 0.1f,  0.1f,  0.1f  };
    glm::vec3 Diffuse   { 0.8f,  0.8f,  0.8f  };
    glm::vec3 Specular  { 0.5f,  0.5f,  0.5f  };
    float     Shininess { 32.0f                };
};

// ── Triangle3D ────────────────────────────────────────────────────────────────
// A single triangular face in model space, with per-vertex positions, shading
// normals, texture coordinates, and a material.
struct Triangle3D
{
    glm::vec3  V[3];   // positions in model space
    glm::vec3  N[3];   // shading normals in model space (may be face-normal if OBJ has no vn)
    glm::vec2  UV[3];  // texture coordinates (unused for un-textured rendering)
    Material3D Mat;
};

// ── Mesh3D ────────────────────────────────────────────────────────────────────
// A collection of triangles loaded from one .obj file.
// Owned by Render3DLayer; replaced on each LoadModel() call.
struct Mesh3D
{
    std::vector<Triangle3D> Triangles;
    std::string             SourcePath; // original VFS path
};

