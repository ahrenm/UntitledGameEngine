// color.frag.hlsl — flat vertex-color fragment shader for the Core Renderer2D (SDL_GPU).
//
// Used for untextured fills (e.g. background clear rects, TeaPot colored path).
// Cross-compiled to SPIR-V/DXIL/MSL via SDL_shadercross (see README.md).

struct Input
{
    float4 Color    : TEXCOORD0;
    float2 TexCoord : TEXCOORD1; // present so this pairs with sprite.vert.hlsl output
};

float4 main(Input input) : SV_Target0
{
    return input.Color;
}

