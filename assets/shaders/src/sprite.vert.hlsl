// sprite.vert.hlsl — 2D quad vertex shader for the Core Renderer2D (SDL_GPU).
//
// Authored in HLSL and cross-compiled to SPIR-V/DXIL/MSL via SDL_shadercross.
// See README.md in this folder for the exact (manual) compile commands and the
// SDL_GPU HLSL register-space conventions.
//
// Vertex layout (must match Renderer2D's SDL_GPUVertexAttribute setup):
//   location 0: float2 Position  (pixel-space, transformed by MvpMatrix)
//   location 1: float2 TexCoord
//   location 2: float4 Color     (RGBA, linear 0..1)

// Vertex uniform buffers live in space1 under the SDL_shadercross convention.
cbuffer VertexUniforms : register(b0, space1)
{
    float4x4 MvpMatrix; // letterbox-aware orthographic projection * camera view
};

struct Input
{
    float2 Position : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
    float4 Color    : TEXCOORD2;
};

struct Output
{
    float4 Color    : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
    float4 Position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.Position = mul(MvpMatrix, float4(input.Position, 0.0f, 1.0f));
    output.TexCoord = input.TexCoord;
    output.Color    = input.Color;
    return output;
}

