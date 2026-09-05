# Shaders (SDL_GPU)

Shaders for the Core `Renderer2D` (and, later, the TeaPot3D GPU renderer) are authored once in
**HLSL** under [`src/`](./src) and cross-compiled to the three SDL_GPU byte-code formats:

| Format | File extension | Backend |
|--------|----------------|---------|
| SPIR-V | `.spv`         | Vulkan (primary on Windows/Linux) |
| DXIL   | `.dxil`        | Direct3D 12 |
| MSL    | `.msl`         | Metal |

**SPIR-V is the committed primary format.** DXIL/MSL variants are optional for now and tracked as a
deferred item in [`docs/sprints/SDL_GPU_Migration.md`](../../docs/sprints/SDL_GPU_Migration.md).

The compiled binaries are committed next to this README (e.g. `sprite.vert.spv`) and are packed into
`assets.pak` automatically by `pack_assets.cmake`. They load at runtime through the PhysFS VFS as
`assets/shaders/<name>.spv`.

> ⚠️ **Compilation is currently a manual step.** Build automation (a CMake custom command / CI step)
> is deferred. After editing any file in `src/`, re-run the commands below and commit the updated
> binaries.

## Prerequisites

Install **SDL_shadercross** (bundles DXC + SPIRV-Cross): <https://github.com/libsdl-org/SDL_shadercross>.
It cross-compiles HLSL to all three SDL_GPU formats and applies SDL_GPU's resource-binding rules.

## Compile commands

Run from this directory (`assets/shaders/`):

```powershell
# Vertex shader
shadercross src/sprite.vert.hlsl -o sprite.vert.spv
shadercross src/sprite.vert.hlsl -o sprite.vert.dxil   # optional
shadercross src/sprite.vert.hlsl -o sprite.vert.msl    # optional

# Fragment shaders
shadercross src/sprite.frag.hlsl -o sprite.frag.spv
shadercross src/color.frag.hlsl  -o color.frag.spv
```

`shadercross` infers stage and target format from the input/output file extensions. If your build
needs explicit flags, pass `--stage vertex|fragment` and `--dest SPIRV|DXIL|MSL`.

## SDL_GPU HLSL register-space conventions

`shadercross` maps HLSL registers to SDL_GPU binding slots by **register space**. Author shaders to
match, and keep `Renderer2D`'s pipeline layout in sync:

| Resource | Vertex stage | Fragment stage |
|----------|--------------|----------------|
| Textures (`t#`) / Samplers (`s#`) | `space0` | `space2` |
| Uniform buffers (`b#`)            | `space1` | `space3` |

## Current shaders

| Source | Purpose | Outputs |
|--------|---------|---------|
| `src/sprite.vert.hlsl` | 2D quad transform via `MvpMatrix` (b0, space1). | `sprite.vert.spv` |
| `src/sprite.frag.hlsl` | Sample texture (t0/s0, space2) × vertex color. | `sprite.frag.spv` |
| `src/color.frag.hlsl`  | Flat vertex-color fill (untextured). | `color.frag.spv` |
| `src/mesh.vert.hlsl`   | 3D mesh transform via `MvpMatrix` + `NormalMatrix` (b0, space1); passes world-space normal + per-vertex diffuse/ambient. | `mesh.vert.spv` |
| `src/mesh.frag.hlsl`   | Directional diffuse + ambient lighting (light uniforms b0, space3). | `mesh.frag.spv` |

The 2D vertex layout shared by these shaders is: `float2 Position, float2 TexCoord, float4 Color`.

