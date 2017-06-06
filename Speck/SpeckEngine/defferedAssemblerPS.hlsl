
#include "cpuGpuDefines.txt"

cbuffer cbSettings										: register(b0)
{
	uint gTextureMapsWidth;
	uint gTextureMapsHeight;
	uint gBackBufferWidth;
	uint gBackBufferHeight;
};

Texture2D gTextureMaps[DEFERRED_RENDER_TARGETS_COUNT]	: register(t0);
// Samplers
SamplerState gsamPointClamp								: register(s0);

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

float3 TextelToNormalInWorld(float4 normalT)
{
	// Decompress each component from [0,1] to [-1,1].
	return (2.0f*normalT - 1.0f).xyz;
}

float4 ProcessPixel(float2 texCoord)
{
	float4 color = gTextureMaps[0].SampleLevel(gsamPointClamp, texCoord, 0);
	float3 normal = TextelToNormalInWorld(gTextureMaps[1].SampleLevel(gsamPointClamp, texCoord, 0));
	return color * (1.0f + max(0.0f, dot(normal, float3(0.0f, 1.0f, 0.0f))))*0.5f;
}

float4 main(VertexOut pin) : SV_Target
{
	float2 textelDimensions = float2(1.0f / gTextureMapsWidth, 1.0f / gTextureMapsHeight);
	float2 pixelDimensions = float2(1.0f / gBackBufferWidth, 1.0f / gBackBufferHeight);

	float2 coords[4];
	coords[0] = pin.TexC + float2(0.0f, 0.0f);
	coords[1] = pin.TexC + float2(textelDimensions.x, 0.0f);
	coords[2] = pin.TexC + float2(0.0f, textelDimensions.y);
	coords[3] = pin.TexC + float2(textelDimensions.x, textelDimensions.y);

	// Super sampling part
	float4 color = 0.0f;
	color += ProcessPixel(coords[0]);
	color += ProcessPixel(coords[1]);
	color += ProcessPixel(coords[2]);
	color += ProcessPixel(coords[3]);
	color /= 4.0f;

	return color;
}
