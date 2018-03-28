
#include "cpuGpuDefines.txt"

cbuffer cbSettings										: register(b0)
{
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gProjTex;

	uint gSSAOMapsWidth;
	uint gSSAOMapsHeight;
	uint gTextureMapsWidth;
	uint gTextureMapsHeight;

	float gOcclusionRadius;
	float gOcclusionFadeStart;
	float gOcclusionFadeEnd;
	float gSurfaceEpsilon;

	float4 gOffsetVectors[SSAO_RANDOM_SAMPLES_N];
	float4 gBlurWeights[SSAO_BLUR_WEIGHTS_N];
};

cbuffer cbPerPass										: register(b1)
{
	int gPassType;
};

Texture2D gPreviousResultMap							: register(t0, space0); // used for blurring
Texture2D gTextureMaps[SSAO_INPUT_TEXTURE_COUNT]		: register(t0, space1);

// Samplers
SamplerState gsamPointClamp								: register(s0);
SamplerState gsamLinearWrap								: register(s1);

struct VertexOut
{
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

float NDCDepthToViewDepth(float z_ndc)
{
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}
