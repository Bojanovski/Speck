
#include "cpuGpuDefines.txt"

cbuffer cbSettings										: register(b0)
{
	uint gTextureMapsWidth;
	uint gTextureMapsHeight;
	uint gBackBufferWidth;
	uint gBackBufferHeight;
};

Texture2D gTextureMaps[POST_PROCESS_INPUT_TEXTURE_COUNT]	: register(t0);

// Samplers
SamplerState gsamPointClamp									: register(s0);
SamplerState gsamLinearWrap									: register(s1);

struct VertexOut
{
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

float4 ProcessPixel(float2 texCoord, float ambientOcclusion)
{
	float4 color = gTextureMaps[POST_PROCESS_INPUT_TEXTURE_DIFFUSE_MAP].SampleLevel(gsamPointClamp, texCoord, 0);
	float3 normal = gTextureMaps[POST_PROCESS_INPUT_TEXTURE_NORMAL_MAP].SampleLevel(gsamPointClamp, texCoord, 0).xyz;
	return color * ((ambientOcclusion * 0.5f + 0.5f) + max(0.0f, dot(normal, float3(0.0f, 1.0f, 0.0f))))*0.5f;
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

	float ambientOcclusion = gTextureMaps[POST_PROCESS_INPUT_SSAO_MAP].SampleLevel(gsamLinearWrap, pin.TexC, 0).r;

	// Super sampling part
	float4 color = 0.0f;
	color += ProcessPixel(coords[0], ambientOcclusion);
	color += ProcessPixel(coords[1], ambientOcclusion);
	color += ProcessPixel(coords[2], ambientOcclusion);
	color += ProcessPixel(coords[3], ambientOcclusion);
	color /= 4.0f;


	return color;
}
