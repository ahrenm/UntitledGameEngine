// ── tinyobjloader implementation (single-TU define) ───────────────────────────
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <Render3DLayer/Render3DLayer.h>
#include <Layers/SDLLayer.h>
#include <Render3D/Mesh3D.h>
#include <Layers/PhysFSLayer.h>
#include <ServiceLocator.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <sol/sol.hpp>

#include <algorithm>
#include <format>
#include <sstream>
#include <map>

// ─────────────────────────────────────────────────────────────────────────────
// PhysFSMaterialReader
// Resolves .mtl paths via PhysFSLayer so tinyobjloader never touches the real
// filesystem.  baseDir is the virtual directory of the .obj file, e.g.
//   "assets/models/"
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
class PhysFSMaterialReader : public tinyobj::MaterialReader
{
public:
    PhysFSMaterialReader(PhysFSLayer& Physfs, std::string BaseDir)
        : m_physfs(Physfs), m_baseDir(std::move(BaseDir)) {}

    bool operator()(const std::string& MatId,
                    std::vector<tinyobj::material_t>* Materials,
                    std::map<std::string, int>*        MatMap,
                    std::string*                       Warn,
                    std::string*                       Err) override
    {
        std::string path = m_baseDir + MatId;
        auto bytes = m_physfs.ReadFile(path.c_str());
        if (bytes.empty())
        {
            if (Warn) *Warn += std::format("[Render3D] MTL not found in VFS: '{}'\n", path);
            return false;
        }

        std::string  text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::istringstream ss(text);
        tinyobj::LoadMtl(MatMap, Materials, &ss, Warn, Err);
        return true;
    }

private:
    PhysFSLayer& m_physfs;
    std::string  m_baseDir;
};
} // namespace

// ── Factory ───────────────────────────────────────────────────────────────────
std::expected<std::unique_ptr<Render3DLayer>, std::string>
Render3DLayer::Create()
{
    auto* SdlLayer = ServiceLocator::TryGet<SDLLayer>();
    if (!SdlLayer)
        return std::unexpected("Render3DLayer::Create: SDLLayer not found in ServiceLocator");

    SDL_Renderer* Renderer = SdlLayer->Renderer();
    if (!Renderer)
        return std::unexpected("Render3DLayer::Create: null SDL_Renderer");

    auto Layer = std::unique_ptr<Render3DLayer>(new Render3DLayer());
    Layer->m_renderer = Renderer;
    return Layer;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void Render3DLayer::Activate()
{
    m_active = true;

    // Auto-fill viewport to the full renderer output if not set explicitly.
    if (m_viewport.w <= 0.0f || m_viewport.h <= 0.0f)
    {
        int w = 0, h = 0;
        SDL_GetRenderOutputSize(m_renderer, &w, &h);
        m_viewport = { 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h) };
    }

    Log(std::format("[Render3D] Activated — viewport {:.0f}×{:.0f} @ ({:.0f},{:.0f})",
                    m_viewport.w, m_viewport.h, m_viewport.x, m_viewport.y));
}

void Render3DLayer::Deactivate()
{
    m_active = false;
    m_mesh.reset();
    m_pendingModelPath.reset();
    Log("[Render3D] Deactivated — resources released");
}

// ── Model loading ─────────────────────────────────────────────────────────────
void Render3DLayer::LoadModel(const char* VirtualPath)
{
    m_pendingModelPath = VirtualPath;
}

void Render3DLayer::UnloadModel()
{
    m_mesh.reset();
    m_pendingModelPath.reset();
}

// ── Viewport ──────────────────────────────────────────────────────────────────
void Render3DLayer::SetViewport(SDL_FRect Rect)
{
    m_viewport = Rect;
}

// ── Transform ─────────────────────────────────────────────────────────────────
void Render3DLayer::SetModelRotation(float PitchDeg, float YawDeg, float RollDeg)
{
    m_rotPitch = PitchDeg;
    m_rotYaw   = YawDeg;
    m_rotRoll  = RollDeg;
}

void Render3DLayer::SetModelScale(float Scale)
{
    m_scale = Scale;
}

void Render3DLayer::SetModelPosition(float X, float Y, float Z)
{
    m_posX = X;
    m_posY = Y;
    m_posZ = Z;
}

// ── Camera ────────────────────────────────────────────────────────────────────
void Render3DLayer::SetCamera(float EyeX,    float EyeY,    float EyeZ,
                               float TargetX, float TargetY, float TargetZ,
                               float FovDeg)
{
    m_camera.Eye    = { EyeX,    EyeY,    EyeZ    };
    m_camera.Target = { TargetX, TargetY, TargetZ };
    m_camera.FovDeg = FovDeg;
}

// ── Lighting ──────────────────────────────────────────────────────────────────
void Render3DLayer::SetLightDirection(float X, float Y, float Z)
{
    m_light.Direction = { X, Y, Z };
}

void Render3DLayer::SetLightColor(float R, float G, float B)
{
    m_light.Color = { R, G, B };
}

void Render3DLayer::SetAmbientColor(float R, float G, float B)
{
    m_light.Ambient = { R, G, B };
}

// ── Viewport clear ────────────────────────────────────────────────────────────
void Render3DLayer::SetViewportClearColor(SDL_Color Color)
{
    m_viewportClear = Color;
}

void Render3DLayer::ClearViewportClear()
{
    m_viewportClear.reset();
}

// ── AppLayer::Update ─────────────────────────────────────────────────────────
void Render3DLayer::Update()
{
    if (!m_pendingModelPath) return;

    // Process a queued LoadModel() call.
    const std::string path = std::move(*m_pendingModelPath);
    m_pendingModelPath.reset();
    loadModelNow(path);
}

// ── AppLayer::Tick ────────────────────────────────────────────────────────────
void Render3DLayer::Tick(float /*deltaTime*/)
{
    if (!m_active) return;
    if (!m_mesh || m_mesh->Triangles.empty()) return;
    renderMesh();
}

// ── loadModelNow ──────────────────────────────────────────────────────────────
void Render3DLayer::loadModelNow(const std::string& VirtualPath)
{
    auto* physfs = ServiceLocator::TryGet<PhysFSLayer>();
    if (!physfs)
    {
        Log("[Render3D] LoadModel: PhysFSLayer not available");
        return;
    }

    // Derive the virtual directory for MTL resolution.
    std::string baseDir;
    if (auto pos = VirtualPath.rfind('/'); pos != std::string::npos)
        baseDir = VirtualPath.substr(0, pos + 1);

    // Read the .obj file from the VFS.
    auto bytes = physfs->ReadFile(VirtualPath.c_str());
    if (bytes.empty())
    {
        Log(std::format("[Render3D] LoadModel: failed to read '{}'", VirtualPath));
        return;
    }

    std::string        objText(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream objStream(objText);

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn, err;

    PhysFSMaterialReader matReader(*physfs, baseDir);
    const bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials,
                                     &warn, &err,
                                     &objStream, &matReader,
                                     /*triangulate=*/true);

    if (!warn.empty()) Log("[Render3D] OBJ warning: " + warn);
    if (!ok)
    {
        Log("[Render3D] OBJ parse error: " + err);
        return;
    }

    // ── Build Mesh3D ──────────────────────────────────────────────────────────
    auto mesh = std::make_unique<Mesh3D>();
    mesh->SourcePath = VirtualPath;

    for (const auto& shape : shapes)
    {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
        {
            const int fv = static_cast<int>(shape.mesh.num_face_vertices[f]);
            if (fv != 3) { indexOffset += static_cast<size_t>(fv); continue; }

            Triangle3D tri{};

            // Material
            const int matId = shape.mesh.material_ids[f];
            if (matId >= 0 && matId < static_cast<int>(materials.size()))
            {
                const auto& m     = materials[matId];
                tri.Mat.Ambient   = { m.ambient[0],  m.ambient[1],  m.ambient[2]  };
                tri.Mat.Diffuse   = { m.diffuse[0],  m.diffuse[1],  m.diffuse[2]  };
                tri.Mat.Specular  = { m.specular[0], m.specular[1], m.specular[2] };
                tri.Mat.Shininess = m.shininess;
            }

            for (int v = 0; v < 3; ++v)
            {
                const auto& idx = shape.mesh.indices[indexOffset + static_cast<size_t>(v)];

                tri.V[v] = {
                    attrib.vertices[3 * static_cast<size_t>(idx.vertex_index) + 0],
                    attrib.vertices[3 * static_cast<size_t>(idx.vertex_index) + 1],
                    attrib.vertices[3 * static_cast<size_t>(idx.vertex_index) + 2]
                };

                if (idx.normal_index >= 0)
                {
                    tri.N[v] = {
                        attrib.normals[3 * static_cast<size_t>(idx.normal_index) + 0],
                        attrib.normals[3 * static_cast<size_t>(idx.normal_index) + 1],
                        attrib.normals[3 * static_cast<size_t>(idx.normal_index) + 2]
                    };
                }

                if (idx.texcoord_index >= 0)
                {
                    tri.UV[v] = {
                        attrib.texcoords[2 * static_cast<size_t>(idx.texcoord_index) + 0],
                        attrib.texcoords[2 * static_cast<size_t>(idx.texcoord_index) + 1]
                    };
                }
            }

            // Generate face normal if the OBJ has none.
            if (tri.N[0] == glm::vec3(0.0f) &&
                tri.N[1] == glm::vec3(0.0f) &&
                tri.N[2] == glm::vec3(0.0f))
            {
                const glm::vec3 faceN = glm::normalize(
                    glm::cross(tri.V[1] - tri.V[0], tri.V[2] - tri.V[0]));
                tri.N[0] = tri.N[1] = tri.N[2] = faceN;
            }

            mesh->Triangles.push_back(tri);
            indexOffset += static_cast<size_t>(fv);
        }
    }

    m_mesh = std::move(mesh);
    Log(std::format("[Render3D] Model loaded: '{}' ({} triangles)",
                    VirtualPath, m_mesh->Triangles.size()));
}

// ── renderMesh ────────────────────────────────────────────────────────────────
void Render3DLayer::renderMesh()
{
    if (m_viewport.w <= 0.0f || m_viewport.h <= 0.0f) return;

    const float vw = m_viewport.w;
    const float vh = m_viewport.h;

    // ── Build transform matrices ───────────────────────────────────────────────
    // Model: scale → pitch (X) → yaw (Y) → roll (Z) → translate
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, { m_posX, m_posY, m_posZ });
    model = glm::rotate(model, glm::radians(m_rotYaw),   glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotPitch), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotRoll),  glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(m_scale));

    const glm::mat4 view     = glm::lookAt(m_camera.Eye, m_camera.Target, m_camera.Up);
    const glm::mat4 proj     = glm::perspective(glm::radians(m_camera.FovDeg),
                                                 vw / vh,
                                                 m_camera.NearZ, m_camera.FarZ);
    const glm::mat4 mv       = view * model;
    // Normal matrix: inverse-transpose of the model-view upper-left 3×3
    const glm::mat3 normalMat = glm::inverseTranspose(glm::mat3(mv));

    // Light direction transformed to view space (direction vector → w=0)
    const glm::vec3 lightDirView = glm::normalize(
        glm::vec3(view * glm::vec4(glm::normalize(m_light.Direction), 0.0f)));

    // ── Build screen-space face batches ───────────────────────────────────────
    struct FaceBatch
    {
        SDL_Vertex verts[3];
        float      depth; // view-space centroid Z (negative = in front of camera)
    };

    std::vector<FaceBatch> batches;
    batches.reserve(m_mesh->Triangles.size());

    for (const auto& tri : m_mesh->Triangles)
    {
        // Transform vertices to view (camera) space.
        const glm::vec4 mv0 = mv * glm::vec4(tri.V[0], 1.0f);
        const glm::vec4 mv1 = mv * glm::vec4(tri.V[1], 1.0f);
        const glm::vec4 mv2 = mv * glm::vec4(tri.V[2], 1.0f);

        // ── Back-face culling ─────────────────────────────────────────────────
        // Geometric normal in view space from cross product.
        // In right-handed view space (camera at origin, looking toward -Z):
        //   dot(geomNormal, v0) >= 0  →  normal points away from camera  →  back-face
        const glm::vec3 e1       = glm::vec3(mv1) - glm::vec3(mv0);
        const glm::vec3 e2       = glm::vec3(mv2) - glm::vec3(mv0);
        const glm::vec3 geomNorm = glm::cross(e1, e2);
        if (glm::dot(geomNorm, glm::vec3(mv0)) >= 0.0f) continue;

        // ── Near-plane guard ──────────────────────────────────────────────────
        // Skip faces whose centroid is behind (more positive than) the near plane.
        const float centroidZ = (mv0.z + mv1.z + mv2.z) / 3.0f;
        if (centroidZ > -m_camera.NearZ) continue;

        // ── Project to clip space ─────────────────────────────────────────────
        const glm::vec4 clip0 = proj * mv0;
        const glm::vec4 clip1 = proj * mv1;
        const glm::vec4 clip2 = proj * mv2;

        // Discard faces where any vertex has non-positive clip.w (behind camera).
        if (clip0.w <= 0.0f || clip1.w <= 0.0f || clip2.w <= 0.0f) continue;

        // Perspective divide → NDC [-1, 1]
        const glm::vec3 ndc0 = glm::vec3(clip0) / clip0.w;
        const glm::vec3 ndc1 = glm::vec3(clip1) / clip1.w;
        const glm::vec3 ndc2 = glm::vec3(clip2) / clip2.w;

        // ── Phong shading (per vertex) ────────────────────────────────────────
        // Shading normals through the normal matrix.
        const glm::vec3 n0 = glm::normalize(normalMat * tri.N[0]);
        const glm::vec3 n1 = glm::normalize(normalMat * tri.N[1]);
        const glm::vec3 n2 = glm::normalize(normalMat * tri.N[2]);

        // Blinn-Phong diffuse-only (+ambient) for each vertex:
        //   L = Kd * (N · -LightDir) * LightColor + Ka * AmbientColor
        auto phong = [&](const glm::vec3& n) -> SDL_FColor
        {
            const float diff = glm::clamp(glm::dot(n, -lightDirView), 0.0f, 1.0f);
            glm::vec3 lit = tri.Mat.Diffuse  * m_light.Color   * diff
                          + tri.Mat.Ambient  * m_light.Ambient;
            lit = glm::clamp(lit, 0.0f, 1.0f);
            return { lit.r, lit.g, lit.b, 1.0f };
        };

        // ── NDC → viewport-local screen coords ───────────────────────────────
        // SDL_SetRenderViewport makes (0,0) the top-left of m_viewport on screen,
        // so all coordinates here are relative to the viewport, not the window.
        auto ndcToScreen = [&](const glm::vec3& ndc) -> SDL_FPoint
        {
            return {
                (ndc.x  * 0.5f + 0.5f)  * vw,          // X: [-1,1] → [0, vw]
                (1.0f - (ndc.y * 0.5f + 0.5f)) * vh    // Y: [-1,1] → [vh, 0] (flip)
            };
        };

        FaceBatch batch;
        batch.depth     = centroidZ;
        batch.verts[0]  = { ndcToScreen(ndc0), phong(n0), {} };
        batch.verts[1]  = { ndcToScreen(ndc1), phong(n1), {} };
        batch.verts[2]  = { ndcToScreen(ndc2), phong(n2), {} };
        batches.push_back(batch);
    }

    // ── Painter's algorithm: draw farthest faces first ────────────────────────
    // In right-handed view space, more negative Z = farther from camera.
    std::ranges::sort(batches, [](const FaceBatch& a, const FaceBatch& b)
    {
        return a.depth < b.depth; // most-negative (farthest) → first
    });

    // ── Activate the viewport clip and draw ───────────────────────────────────
    const SDL_Rect viewRect {
        static_cast<int>(m_viewport.x),
        static_cast<int>(m_viewport.y),
        static_cast<int>(m_viewport.w),
        static_cast<int>(m_viewport.h)
    };
    SDL_SetRenderViewport(m_renderer, &viewRect);

    // Optional solid-color clear (respects the viewport clip).
    if (m_viewportClear)
    {
        SDL_SetRenderDrawColor(m_renderer,
                               m_viewportClear->r,
                               m_viewportClear->g,
                               m_viewportClear->b,
                               m_viewportClear->a);
        const SDL_FRect clearRect { 0.0f, 0.0f, vw, vh };
        SDL_RenderFillRect(m_renderer, &clearRect);
    }

    // Stream all triangles; SDL batches internally.
    for (const auto& batch : batches)
        SDL_RenderGeometry(m_renderer, nullptr, batch.verts, 3, nullptr, 0);

    // Reset to full-window viewport so subsequent layers (RmlUILayer) are unaffected.
    SDL_SetRenderViewport(m_renderer, nullptr);
}

// ── IScriptableObject ─────────────────────────────────────────────────────────
void Render3DLayer::RegisterObject(sol::state& Lua)
{
    auto r3d = Lua.create_named_table("Render3D");

    r3d.set_function("Activate",   [this]() { Activate(); });
    r3d.set_function("Deactivate", [this]() { Deactivate(); });
    r3d.set_function("IsActive",   [this]() { return IsActive(); });

    r3d.set_function("LoadModel", [this](const std::string& Path) {
        LoadModel(Path.c_str());
    });

    r3d.set_function("SetViewport", [this](float X, float Y, float W, float H) {
        SetViewport({ X, Y, W, H });
    });

    r3d.set_function("SetModelRotation", [this](float P, float Y, float R) {
        SetModelRotation(P, Y, R);
    });
    r3d.set_function("SetModelScale", [this](float S) { SetModelScale(S); });
    r3d.set_function("SetModelPosition", [this](float X, float Y, float Z) {
        SetModelPosition(X, Y, Z);
    });

    // SetCamera(eyeX, eyeY, eyeZ, targetX, targetY, targetZ [, fovDeg=60])
    r3d.set_function("SetCamera",
        [this](float Ex, float Ey, float Ez,
               float Tx, float Ty, float Tz,
               sol::optional<float> Fov)
        {
            SetCamera(Ex, Ey, Ez, Tx, Ty, Tz, Fov.value_or(60.0f));
        });

    r3d.set_function("SetLightDirection", [this](float X, float Y, float Z) {
        SetLightDirection(X, Y, Z);
    });
    r3d.set_function("SetLightColor", [this](float R, float G, float B) {
        SetLightColor(R, G, B);
    });
    r3d.set_function("SetAmbientColor", [this](float R, float G, float B) {
        SetAmbientColor(R, G, B);
    });

    // SetClearColor(r, g, b, a) with integer [0,255] channels; a=0 → disable
    r3d.set_function("SetClearColor",
        [this](Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
            if (A == 0)
                ClearViewportClear();
            else
                SetViewportClearColor({ R, G, B, A });
        });
}

void Render3DLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}


