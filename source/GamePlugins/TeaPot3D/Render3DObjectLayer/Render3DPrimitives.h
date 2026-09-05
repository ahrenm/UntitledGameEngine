#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Render3DPrimitives.h
//
// Plain descriptor structs shared by Render3DObjectLayer and its clients.
// Consolidates the small POD types that used to live in Camera3D.h, Light3D.h,
// and Mesh3D.h into a single header. Also hosts the GPU-facing vertex/uniform
// layouts used to interface with the mesh shaders.
// ─────────────────────────────────────────────────────────────────────────────

// ── Camera3D ──────────────────────────────────────────────────────────────────
// Plain camera descriptor used by Render3DObjectLayer.
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

// ── Light3D ───────────────────────────────────────────────────────────────────
// Directional light descriptor used by Render3DObjectLayer.
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
// Owned by Render3DObjectLayer; replaced on each LoadModel() call.
struct Mesh3D
{
    std::vector<Triangle3D> Triangles;
    std::string             SourcePath; // original VFS path
};

// ── MeshVertex ────────────────────────────────────────────────────────────────
// Interleaved GPU vertex layout uploaded by Render3DObjectLayer. Materials are
// per-face in the OBJ, so diffuse/ambient are duplicated across a face's three
// vertices. Attribute offsets/formats must match the vertex-input state declared
// when creating the mesh pipeline.
struct MeshVertex
{
    float Px, Py, Pz;   // position (model space)
    float Nx, Ny, Nz;   // shading normal (model space)
    float Dr, Dg, Db;   // material diffuse
    float Ar, Ag, Ab;   // material ambient
};

// ── GPU uniform blocks (must match mesh.vert.hlsl / mesh.frag.hlsl) ───────────
struct VertexUniforms
{
    glm::mat4 Mvp;          // proj * view * model (Vulkan clip space)
    glm::mat4 NormalMatrix; // inverse-transpose of the model matrix (mat3 padded to mat4)
};

struct FragmentUniforms
{
    glm::vec4 LightDir;     // world-space light travel direction (xyz)
    glm::vec4 LightColor;   // diffuse colour (xyz)
    glm::vec4 LightAmbient; // ambient colour (xyz)
};
