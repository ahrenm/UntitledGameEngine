// mesh.vert.hlsl — 3D mesh vertex shader for the TeaPot3D GPU renderer (SDL_GPU).
//
// Authored in HLSL and cross-compiled to SPIR-V via SDL_shadercross.
// See README.md in this folder for the (manual) compile commands and the
// SDL_GPU HLSL register-space conventions.
//
// Vertex layout (must match Render3DLayer's SDL_GPUVertexAttribute setup):
//   location 0: float3 Position  (model space)
//   location 1: float3 Normal    (model space)
//   location 2: float3 Diffuse   (per-face material diffuse, linear 0..1)
//   location 3: float3 Ambient   (per-face material ambient, linear 0..1)

// Vertex uniform buffers live in space1 under the SDL_shadercross convention.
cbuffer VertexUniforms : register(b0, space1)
{
    float4x4 MvpMatrix;    // proj * view * model (Vulkan clip space, Y-flipped)
    float4x4 NormalMatrix; // inverse-transpose of the model matrix (world-space normals)
};

struct Input
{
    float3 Position : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float3 Diffuse  : TEXCOORD2;
    float3 Ambient  : TEXCOORD3;
};

struct Output
{
    float3 Normal   : TEXCOORD0;
    float3 Diffuse  : TEXCOORD1;
    float3 Ambient  : TEXCOORD2;
    float4 Position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.Position = mul(MvpMatrix, float4(input.Position, 1.0f));
    output.Normal   = mul((float3x3)NormalMatrix, input.Normal);
    output.Diffuse  = input.Diffuse;
    output.Ambient  = input.Ambient;
    return output;
}
