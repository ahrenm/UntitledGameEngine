// mesh.frag.hlsl — 3D mesh fragment shader for the TeaPot3D GPU renderer (SDL_GPU).
//
// Directional-light diffuse + ambient shading in world space, matching the
// previous CPU rasteriser (Kd·(N·-L)·LightColor + Ka·LightAmbient).
// Cross-compiled to SPIR-V via SDL_shadercross (see README.md).
//
// SDL_shadercross HLSL register spaces (fragment stage):
//   uniform buffers -> space3.

cbuffer FragmentUniforms : register(b0, space3)
{
    float4 LightDir;     // world-space direction the light travels (xyz); normalised on CPU
    float4 LightColor;   // RGB diffuse colour (xyz)
    float4 LightAmbient; // RGB ambient colour (xyz)
};

struct Input
{
    float3 Normal  : TEXCOORD0;
    float3 Diffuse : TEXCOORD1;
    float3 Ambient : TEXCOORD2;
};

float4 main(Input input) : SV_Target0
{
    float3 N = normalize(input.Normal);
    float3 L = normalize(LightDir.xyz);

    // Surface toward light is -L; clamp the Lambert term to the lit hemisphere.
    float diff = saturate(dot(N, -L));

    float3 lit = input.Diffuse * LightColor.xyz * diff
               + input.Ambient * LightAmbient.xyz;

    return float4(saturate(lit), 1.0f);
}
