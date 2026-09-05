// sprite.frag.hlsl — textured 2D quad fragment shader for the Core Renderer2D (SDL_GPU).
//
// Samples a single texture and modulates it by the interpolated vertex color.
// Cross-compiled to SPIR-V/DXIL/MSL via SDL_shadercross (see README.md).
//
// SDL_shadercross HLSL register spaces (fragment stage):
//   textures/samplers -> space2, uniform buffers -> space3.

Texture2D<float4> Texture : register(t0, space2);
SamplerState      Sampler : register(s0, space2);

struct Input
{
    float4 Color    : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
};

float4 main(Input input) : SV_Target0
{
    return Texture.Sample(Sampler, input.TexCoord) * input.Color;
}

