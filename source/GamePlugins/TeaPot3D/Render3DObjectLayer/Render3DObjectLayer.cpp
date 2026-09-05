// ── tinyobjloader implementation (single-TU define) ───────────────────────────
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <Render3DObjectLayer/Render3DObjectLayer.h>
#include <Render3DObjectLayer/PhysFSMaterialReader.hpp>
#include <Layers/SDLLayer.h>
#include <Layers/PhysFSLayer.h>
#include <ServiceLocator.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <sol/sol.hpp>

#include <cstddef>
#include <cstring>
#include <format>
#include <sstream>
#include <vector>

namespace
{
    // Load a precompiled SPIR-V shader from the VFS and create an SDL_GPUShader.
    SDL_GPUShader* LoadShader(SDL_GPUDevice* Device, const char* VirtualPath,
                              SDL_GPUShaderStage Stage, Uint32 NumUniformBuffers)
    {
        auto* Physfs = ServiceLocator::TryGet<PhysFSLayer>();
        if (!Physfs) return nullptr;
        std::vector<std::byte> Code = Physfs->ReadFile(VirtualPath);
        if (Code.empty()) return nullptr;

        SDL_GPUShaderCreateInfo Info{};
        Info.code                = reinterpret_cast<const Uint8*>(Code.data());
        Info.code_size           = Code.size();
        Info.entrypoint          = "main";
        Info.format              = SDL_GPU_SHADERFORMAT_SPIRV;
        Info.stage               = Stage;
        Info.num_samplers        = 0;
        Info.num_uniform_buffers = NumUniformBuffers;
        return SDL_CreateGPUShader(Device, &Info);
    }
} // namespace

// ── Factory ───────────────────────────────────────────────────────────────────
std::expected<std::unique_ptr<Render3DObjectLayer>, std::string>
Render3DObjectLayer::Create()
{
    auto* SdlLayer = ServiceLocator::TryGet<SDLLayer>();
    if (!SdlLayer)
        return std::unexpected("Render3DObjectLayer::Create: SDLLayer not found in ServiceLocator");

    SDL_GPUDevice* Device = SdlLayer->Device();
    SDL_Window*    Window = SdlLayer->Window();
    if (!Device || !Window)
        return std::unexpected("Render3DObjectLayer::Create: null GPU device or window");

    auto Layer = std::unique_ptr<Render3DObjectLayer>(new Render3DObjectLayer());
    Layer->m_device = Device;

    // ── Pick a supported depth format ──────────────────────────────────────────
    if (!SDL_GPUTextureSupportsFormat(Device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                                      SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
    {
        Layer->m_depthFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    }

    // ── Build the mesh graphics pipeline ───────────────────────────────────────
    SDL_GPUShader* Vert = LoadShader(Device, "assets/shaders/mesh.vert.spv",
                                     SDL_GPU_SHADERSTAGE_VERTEX,   /*uniforms=*/1);
    if (!Vert)
        return std::unexpected(std::format("Render3DObjectLayer::Create: failed to load mesh.vert.spv: {}",
                                           SDL_GetError()));

    SDL_GPUShader* Frag = LoadShader(Device, "assets/shaders/mesh.frag.spv",
                                     SDL_GPU_SHADERSTAGE_FRAGMENT, /*uniforms=*/1);
    if (!Frag)
    {
        SDL_ReleaseGPUShader(Device, Vert);
        return std::unexpected(std::format("Render3DObjectLayer::Create: failed to load mesh.frag.spv: {}",
                                           SDL_GetError()));
    }

    SDL_GPUVertexBufferDescription VbDesc{};
    VbDesc.slot       = 0;
    VbDesc.pitch      = sizeof(MeshVertex);
    VbDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute Attrs[4]{};
    Attrs[0].location = 0; Attrs[0].buffer_slot = 0;
    Attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; Attrs[0].offset = offsetof(MeshVertex, Px);
    Attrs[1].location = 1; Attrs[1].buffer_slot = 0;
    Attrs[1].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; Attrs[1].offset = offsetof(MeshVertex, Nx);
    Attrs[2].location = 2; Attrs[2].buffer_slot = 0;
    Attrs[2].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; Attrs[2].offset = offsetof(MeshVertex, Dr);
    Attrs[3].location = 3; Attrs[3].buffer_slot = 0;
    Attrs[3].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; Attrs[3].offset = offsetof(MeshVertex, Ar);

    SDL_GPUColorTargetDescription ColorTarget{};
    ColorTarget.format = SDL_GetGPUSwapchainTextureFormat(Device, Window);
    // Opaque 3D — blending disabled; depth resolves occlusion.

    SDL_GPUGraphicsPipelineCreateInfo Info{};
    Info.vertex_shader   = Vert;
    Info.fragment_shader = Frag;
    Info.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    Info.vertex_input_state.vertex_buffer_descriptions = &VbDesc;
    Info.vertex_input_state.num_vertex_buffers         = 1;
    Info.vertex_input_state.vertex_attributes          = Attrs;
    Info.vertex_input_state.num_vertex_attributes      = 4;
    // Draw both faces: the OBJ winding is not guaranteed and depth-testing makes
    // culling unnecessary for correctness.
    Info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    Info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    Info.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
    Info.depth_stencil_state.enable_depth_test  = true;
    Info.depth_stencil_state.enable_depth_write = true;
    Info.target_info.color_target_descriptions = &ColorTarget;
    Info.target_info.num_color_targets         = 1;
    Info.target_info.has_depth_stencil_target  = true;
    Info.target_info.depth_stencil_format      = Layer->m_depthFormat;

    Layer->m_pipeline = SDL_CreateGPUGraphicsPipeline(Device, &Info);

    SDL_ReleaseGPUShader(Device, Vert);
    SDL_ReleaseGPUShader(Device, Frag);

    if (!Layer->m_pipeline)
        return std::unexpected(std::format("Render3DObjectLayer::Create: SDL_CreateGPUGraphicsPipeline failed: {}",
                                           SDL_GetError()));

    return Layer;
}

// ── Destructor ────────────────────────────────────────────────────────────────
Render3DObjectLayer::~Render3DObjectLayer()
{
    releaseGpuResources();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void Render3DObjectLayer::Activate()
{
    m_active = true;

    // Viewport is resolved per-frame in renderMesh() (auto-filled to the full
    // swapchain when left unset), so no renderer query is needed here.
    if (m_viewport.w <= 0.0f || m_viewport.h <= 0.0f)
        Log("[Render3D] Activated — viewport auto-fills to full swapchain");
    else
        Log(std::format("[Render3D] Activated — viewport {:.0f}×{:.0f} @ ({:.0f},{:.0f})",
                        m_viewport.w, m_viewport.h, m_viewport.x, m_viewport.y));
}

void Render3DObjectLayer::Deactivate()
{
    m_active = false;
    m_mesh.reset();
    m_pendingModelPath.reset();

    // Release per-model GPU buffers and the depth texture; keep the pipeline so a
    // subsequent Activate()/LoadModel() can reuse it without a rebuild.
    if (m_device)
    {
        if (m_vertexBuffer) { SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer); m_vertexBuffer = nullptr; }
        if (m_indexBuffer)  { SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);  m_indexBuffer  = nullptr; }
        if (m_depthTexture) { SDL_ReleaseGPUTexture(m_device, m_depthTexture); m_depthTexture = nullptr; }
    }
    m_indexCount  = 0;
    m_depthWidth  = 0;
    m_depthHeight = 0;

    Log("[Render3D] Deactivated — resources released");
}

// ── Model loading ─────────────────────────────────────────────────────────────
void Render3DObjectLayer::LoadModel(const char* VirtualPath)
{
    m_pendingModelPath = VirtualPath;
}

void Render3DObjectLayer::UnloadModel()
{
    m_mesh.reset();
    m_pendingModelPath.reset();
    if (m_device)
    {
        if (m_vertexBuffer) { SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer); m_vertexBuffer = nullptr; }
        if (m_indexBuffer)  { SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);  m_indexBuffer  = nullptr; }
    }
    m_indexCount = 0;
}

// ── Viewport ──────────────────────────────────────────────────────────────────
void Render3DObjectLayer::SetViewport(SDL_FRect Rect)
{
    m_viewport = Rect;
}

// ── Transform ─────────────────────────────────────────────────────────────────
void Render3DObjectLayer::SetModelRotation(float PitchDeg, float YawDeg, float RollDeg)
{
    m_rotPitch = PitchDeg;
    m_rotYaw   = YawDeg;
    m_rotRoll  = RollDeg;
}

void Render3DObjectLayer::SetModelScale(float Scale)
{
    m_scale = Scale;
}

void Render3DObjectLayer::SetModelPosition(float X, float Y, float Z)
{
    m_posX = X;
    m_posY = Y;
    m_posZ = Z;
}

// ── Camera ────────────────────────────────────────────────────────────────────
void Render3DObjectLayer::SetCamera(float EyeX,    float EyeY,    float EyeZ,
                                     float TargetX, float TargetY, float TargetZ,
                                     float FovDeg)
{
    m_camera.Eye    = { EyeX,    EyeY,    EyeZ    };
    m_camera.Target = { TargetX, TargetY, TargetZ };
    m_camera.FovDeg = FovDeg;
}

// ── Lighting ──────────────────────────────────────────────────────────────────
void Render3DObjectLayer::SetLightDirection(float X, float Y, float Z)
{
    m_light.Direction = { X, Y, Z };
}

void Render3DObjectLayer::SetLightColor(float R, float G, float B)
{
    m_light.Color = { R, G, B };
}

void Render3DObjectLayer::SetAmbientColor(float R, float G, float B)
{
    m_light.Ambient = { R, G, B };
}

// ── Viewport clear ────────────────────────────────────────────────────────────
void Render3DObjectLayer::SetViewportClearColor(SDL_Color Color)
{
    m_viewportClear = Color;
}

void Render3DObjectLayer::ClearViewportClear()
{
    m_viewportClear.reset();
}

// ── AppLayer::Update ─────────────────────────────────────────────────────────
void Render3DObjectLayer::Update()
{
    if (!m_pendingModelPath) return;

    // Process a queued LoadModel() call.
    const std::string path = std::move(*m_pendingModelPath);
    m_pendingModelPath.reset();
    loadModelNow(path);
}

// ── AppLayer::Draw ────────────────────────────────────────────────────────────
void Render3DObjectLayer::Draw(float /*deltaTime*/)
{
    if (!m_active) return;
    if (!m_mesh || m_mesh->Triangles.empty()) return;
    renderMesh();
}

// ── loadModelNow ──────────────────────────────────────────────────────────────
void Render3DObjectLayer::loadModelNow(const std::string& VirtualPath)
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
    uploadMesh();
    Log(std::format("[Render3D] Model loaded: '{}' ({} triangles)",
                    VirtualPath, m_mesh->Triangles.size()));
}

// ── uploadMesh ────────────────────────────────────────────────────────────────
// Flatten the CPU Mesh3D into an interleaved MeshVertex array (materials baked
// per-vertex) plus a trivial index buffer, then upload both to GPU buffers via a
// one-shot copy pass. Safe to call outside an active frame (during Update()).
void Render3DObjectLayer::uploadMesh()
{
    // Release any previous buffers.
    if (m_vertexBuffer) { SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer); m_vertexBuffer = nullptr; }
    if (m_indexBuffer)  { SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);  m_indexBuffer  = nullptr; }
    m_indexCount = 0;

    if (!m_mesh || m_mesh->Triangles.empty()) return;

    std::vector<MeshVertex> vertices;
    vertices.reserve(m_mesh->Triangles.size() * 3);

    for (const auto& tri : m_mesh->Triangles)
    {
        for (int v = 0; v < 3; ++v)
        {
            MeshVertex mv{};
            mv.Px = tri.V[v].x; mv.Py = tri.V[v].y; mv.Pz = tri.V[v].z;
            mv.Nx = tri.N[v].x; mv.Ny = tri.N[v].y; mv.Nz = tri.N[v].z;
            mv.Dr = tri.Mat.Diffuse.r; mv.Dg = tri.Mat.Diffuse.g; mv.Db = tri.Mat.Diffuse.b;
            mv.Ar = tri.Mat.Ambient.r; mv.Ag = tri.Mat.Ambient.g; mv.Ab = tri.Mat.Ambient.b;
            vertices.push_back(mv);
        }
    }

    // Vertices are per-face (normals/materials differ), so no dedup — indices are
    // sequential 0..N-1.
    std::vector<Uint32> indices(vertices.size());
    for (Uint32 i = 0; i < indices.size(); ++i) indices[i] = i;

    const Uint32 vbSize = static_cast<Uint32>(vertices.size() * sizeof(MeshVertex));
    const Uint32 ibSize = static_cast<Uint32>(indices.size()  * sizeof(Uint32));

    // ── Create GPU buffers ─────────────────────────────────────────────────────
    SDL_GPUBufferCreateInfo vbInfo{};
    vbInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbInfo.size  = vbSize;
    m_vertexBuffer = SDL_CreateGPUBuffer(m_device, &vbInfo);

    SDL_GPUBufferCreateInfo ibInfo{};
    ibInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ibInfo.size  = ibSize;
    m_indexBuffer = SDL_CreateGPUBuffer(m_device, &ibInfo);

    if (!m_vertexBuffer || !m_indexBuffer)
    {
        Log(std::format("[Render3D] uploadMesh: buffer creation failed: {}", SDL_GetError()));
        return;
    }

    // ── Transfer buffer (CPU-visible staging) ──────────────────────────────────
    SDL_GPUTransferBufferCreateInfo tbInfo{};
    tbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbInfo.size  = vbSize + ibSize;
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(m_device, &tbInfo);
    if (!transfer)
    {
        Log(std::format("[Render3D] uploadMesh: transfer buffer failed: {}", SDL_GetError()));
        return;
    }

    auto* mapped = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(m_device, transfer, false));
    std::memcpy(mapped,          vertices.data(), vbSize);
    std::memcpy(mapped + vbSize, indices.data(),  ibSize);
    SDL_UnmapGPUTransferBuffer(m_device, transfer);

    // ── One-shot copy pass ─────────────────────────────────────────────────────
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass*      copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation srcV{};
    srcV.transfer_buffer = transfer;
    srcV.offset          = 0;
    SDL_GPUBufferRegion dstV{};
    dstV.buffer = m_vertexBuffer;
    dstV.offset = 0;
    dstV.size   = vbSize;
    SDL_UploadToGPUBuffer(copy, &srcV, &dstV, false);

    SDL_GPUTransferBufferLocation srcI{};
    srcI.transfer_buffer = transfer;
    srcI.offset          = vbSize;
    SDL_GPUBufferRegion dstI{};
    dstI.buffer = m_indexBuffer;
    dstI.offset = 0;
    dstI.size   = ibSize;
    SDL_UploadToGPUBuffer(copy, &srcI, &dstI, false);

    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(m_device, transfer);

    m_indexCount = static_cast<Uint32>(indices.size());
}

// ── ensureDepthTexture ────────────────────────────────────────────────────────
// (Re)create the depth buffer when the target size changes.
bool Render3DObjectLayer::ensureDepthTexture(Uint32 Width, Uint32 Height)
{
    if (m_depthTexture && m_depthWidth == Width && m_depthHeight == Height)
        return true;

    if (m_depthTexture) { SDL_ReleaseGPUTexture(m_device, m_depthTexture); m_depthTexture = nullptr; }

    SDL_GPUTextureCreateInfo info{};
    info.type                 = SDL_GPU_TEXTURETYPE_2D;
    info.format               = m_depthFormat;
    info.usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width                = Width;
    info.height               = Height;
    info.layer_count_or_depth = 1;
    info.num_levels           = 1;
    info.sample_count         = SDL_GPU_SAMPLECOUNT_1;

    m_depthTexture = SDL_CreateGPUTexture(m_device, &info);
    if (!m_depthTexture)
    {
        Log(std::format("[Render3D] ensureDepthTexture: creation failed: {}", SDL_GetError()));
        return false;
    }
    m_depthWidth  = Width;
    m_depthHeight = Height;
    return true;
}

// ── releaseGpuResources ───────────────────────────────────────────────────────
// Release all owned GPU objects. During application shutdown the owning SDLLayer
// (load order 4.0) is destroyed before this layer (4.5), and its
// SDL_DestroyGPUDevice already frees every resource created from the device.
// ServiceLocator::Clear() runs before the layer vector tears down, so a null
// TryGet<SDLLayer>() means the device is gone — in that case we must NOT call the
// release APIs (use-after-free); just drop our handles.
void Render3DObjectLayer::releaseGpuResources()
{
    if (!m_device) return;

    if (ServiceLocator::TryGet<SDLLayer>())
    {
        if (m_pipeline)     SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
        if (m_vertexBuffer) SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
        if (m_indexBuffer)  SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
        if (m_depthTexture) SDL_ReleaseGPUTexture(m_device, m_depthTexture);
    }

    m_pipeline     = nullptr;
    m_vertexBuffer = nullptr;
    m_indexBuffer  = nullptr;
    m_depthTexture = nullptr;
    m_indexCount   = 0;
    m_depthWidth   = 0;
    m_depthHeight  = 0;
}

// ── renderMesh ────────────────────────────────────────────────────────────────
// Record a GPU render pass for the current frame: bind the mesh pipeline, push
// MVP/normal + light uniforms, and issue an indexed draw against the depth buffer.
void Render3DObjectLayer::renderMesh()
{
    auto* SdlLayer = ServiceLocator::TryGet<SDLLayer>();
    if (!SdlLayer || !SdlLayer->IsFrameActive()) return;
    if (!m_pipeline || !m_vertexBuffer || !m_indexBuffer || m_indexCount == 0) return;

    const GpuFrameContext frame = SdlLayer->Frame();
    if (!frame.CommandBuffer || !frame.SwapchainTexture) return;

    // ── Resolve the effective viewport (auto-fill to full swapchain) ───────────
    float vx = m_viewport.x, vy = m_viewport.y, vw = m_viewport.w, vh = m_viewport.h;
    if (vw <= 0.0f || vh <= 0.0f)
    {
        vx = 0.0f; vy = 0.0f;
        vw = static_cast<float>(frame.Width);
        vh = static_cast<float>(frame.Height);
    }
    if (vw <= 0.0f || vh <= 0.0f) return;

    if (!ensureDepthTexture(frame.Width, frame.Height)) return;

    // ── Transforms ─────────────────────────────────────────────────────────────
    // Model: translate → yaw (Y) → pitch (X) → roll (Z) → scale
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, { m_posX, m_posY, m_posZ });
    model = glm::rotate(model, glm::radians(m_rotYaw),   glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotPitch), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_rotRoll),  glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(m_scale));

    const glm::mat4 view = glm::lookAt(m_camera.Eye, m_camera.Target, m_camera.Up);
    // Vulkan-style clip space depth (0..1) via RH_ZO. No Y flip: SDL_GPU's clip
    // space (as used by Renderer2D, whose ortho maps screen-top to clip +Y) already
    // renders clip +Y at the top of the framebuffer, so world-up maps to screen-up.
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(m_camera.FovDeg),
                                           vw / vh, m_camera.NearZ, m_camera.FarZ);

    VertexUniforms vu{};
    vu.Mvp          = proj * view * model;
    // World-space normal matrix (lighting is done in world space in the shader).
    vu.NormalMatrix = glm::mat4(glm::inverseTranspose(glm::mat3(model)));

    FragmentUniforms fu{};
    fu.LightDir     = glm::vec4(glm::normalize(m_light.Direction), 0.0f);
    fu.LightColor   = glm::vec4(m_light.Color,   1.0f);
    fu.LightAmbient = glm::vec4(m_light.Ambient, 1.0f);

    // ── Color + depth targets ──────────────────────────────────────────────────
    SDL_GPUColorTargetInfo colorInfo{};
    colorInfo.texture     = frame.SwapchainTexture;
    colorInfo.store_op    = SDL_GPU_STOREOP_STORE;
    if (m_viewportClear)
    {
        colorInfo.load_op        = SDL_GPU_LOADOP_CLEAR;
        colorInfo.clear_color.r  = m_viewportClear->r / 255.0f;
        colorInfo.clear_color.g  = m_viewportClear->g / 255.0f;
        colorInfo.clear_color.b  = m_viewportClear->b / 255.0f;
        colorInfo.clear_color.a  = m_viewportClear->a / 255.0f;
    }
    else
    {
        // Composite the mesh over whatever the 2D layers rendered.
        colorInfo.load_op = SDL_GPU_LOADOP_LOAD;
    }

    SDL_GPUDepthStencilTargetInfo depthInfo{};
    depthInfo.texture     = m_depthTexture;
    depthInfo.clear_depth = 1.0f;
    depthInfo.load_op     = SDL_GPU_LOADOP_CLEAR;
    depthInfo.store_op    = SDL_GPU_STOREOP_DONT_CARE;
    depthInfo.stencil_load_op  = SDL_GPU_LOADOP_DONT_CARE;
    depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(frame.CommandBuffer,
                                                     &colorInfo, 1, &depthInfo);
    if (!pass) return;

    const SDL_GPUViewport vp { vx, vy, vw, vh, 0.0f, 1.0f };
    SDL_SetGPUViewport(pass, &vp);
    const SDL_Rect scissor {
        static_cast<int>(vx), static_cast<int>(vy),
        static_cast<int>(vw), static_cast<int>(vh)
    };
    SDL_SetGPUScissor(pass, &scissor);

    SDL_BindGPUGraphicsPipeline(pass, m_pipeline);

    SDL_PushGPUVertexUniformData(frame.CommandBuffer,   0, &vu, sizeof(vu));
    SDL_PushGPUFragmentUniformData(frame.CommandBuffer, 0, &fu, sizeof(fu));

    SDL_GPUBufferBinding vbind{};
    vbind.buffer = m_vertexBuffer;
    vbind.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vbind, 1);

    SDL_GPUBufferBinding ibind{};
    ibind.buffer = m_indexBuffer;
    ibind.offset = 0;
    SDL_BindGPUIndexBuffer(pass, &ibind, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_DrawGPUIndexedPrimitives(pass, m_indexCount, 1, 0, 0, 0);

    SDL_EndGPURenderPass(pass);
}

// ── IScriptableObject ─────────────────────────────────────────────────────────
void Render3DObjectLayer::RegisterObject(sol::state& Lua)
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

void Render3DObjectLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}

