
#ifndef SHADER_ROOT_H
#define SHADER_ROOT_H

#include "cpuGpuDefines.txt"
#include "LightingUtil.hlsl"
#include "MathHelper.hlsl"
#include "sharedStructures.hlsl"

// Render item data that varies per item.
cbuffer cbRenderItem : register(b0)
{
	// Local or global, depending on the type of the render item.
	float4x4 gTransform;
	// Local or global, depending on the type of the render item.
	float4x4 gInvTransposeTransform;
	float4x4 gTexTransform;

	uint gRenderItemType;
	uint gMaterialIndex;
	uint2 gPadding;
	uint4 gParam[RENDER_ITEM_SPECIAL_PARAM_N];
};

// Constant data that varies per pass.
cbuffer cbPass : register(b1)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
};

// An array of textures, which is only supported in shader model 5.1+.  Unlike Texture2DArray, the textures
// in this array can be different sizes and formats, making it more flexible than texture arrays.
Texture2D gTextureMaps[MATERIAL_TEXTURES_COUNT] : register(t0);

// Put in space1, so the texture array does not overlap with these resources.  
// The texture array will occupy registers t0, t1, ..., t6 in space0. 
// Data for instances
StructuredBuffer<InstanceData> gInstanceData	: register(t0, space1);
// Materials data
StructuredBuffer<MaterialData> gMaterialData	: register(t1, space1);
// Rigid body data
StructuredBuffer<RigidBodyData> gRigidBodies	: register(t2, space1);

// Samplers
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

//---------------------------------------------------------------------------------------
// Build tangent-bitangent-normal matrix.
//---------------------------------------------------------------------------------------
float3x3 GetTBN(float3 normalW, float3 tangentW)
{
	// Build orthonormal basis.
	float3 N = normalize(normalW);
	float3 T = normalize(tangentW - dot(tangentW, N)*N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);
	return TBN;
}

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3x3 TBN)
{
	// Uncompress each component from [0,1] to [-1,1].
	float3 normalT = 2.0f*normalMapSample - 1.0f;

	// Transform from tangent space to world space.
	float3 bumpedNormalW = mul(normalT, TBN);

	return bumpedNormalW;
}

//---------------------------------------------------------------------------------------
// Transforms a normal in world space to a normal map textel.
//---------------------------------------------------------------------------------------
float4 NormalInWorldToTextel(float3 normalW)
{
	// Compress each component from [-1,1] to [0,1].
	float3 normalTextel = 0.5f*normalW + 0.5f;
	return float4(normalTextel, 0.0f);
}

//---------------------------------------------------------------------------------------
// Offset the texture coordinates due to parallax occlusion.
//---------------------------------------------------------------------------------------
float2 GetParallaxOcclusionTextureCoordinateOffset(float2 texCoord, float3 texPosW, float3 texNormalW, float heightMapScale, float4x4 texTransform, float3x3 TBN)
{
	// Calculate new parallaxed texture coordinates.
	float3 E = normalize(texPosW - gEyePosW);
	float3x3 worldToTangentSpace = transpose(TBN);
	float3 ET = mul(E, worldToTangentSpace);
	float3 NT = mul(texNormalW, worldToTangentSpace);

	float fParallaxLimit = -length(ET.xy) / ET.z;
	fParallaxLimit *= heightMapScale;
	float2 vOffsetDir = normalize(ET.xy);
	float2 vMaxOffset = vOffsetDir * fParallaxLimit;
	vMaxOffset = mul(float4(vMaxOffset, 0.0f, 1.0f), texTransform).xy; // transform the ray
	int nNumSamples = (int)lerp(PARALLAX_OCCLUSION_MAX_STEPS, PARALLAX_OCCLUSION_MIN_STEPS, dot(ET, NT));
	//nNumSamples = PARALLAX_OCCLUSION_MAX_STEPS;
	float fStepSize = 1.0 / (float)nNumSamples;
	
	float2 dx = ddx(texCoord);
	float2 dy = ddy(texCoord);

	// Initialize for the loop
	float fCurrRayHeight = 1.0;
	float2 vCurrOffset = float2(0, 0);
	float2 vLastOffset = float2(0, 0);
	float fLastSampledHeight = 1;
	float fCurrSampledHeight = 1;
	int nCurrSample = 0;

	[unroll(PARALLAX_OCCLUSION_MAX_STEPS)]
	while (nCurrSample < nNumSamples)
	{
		fCurrSampledHeight = gTextureMaps[2].SampleGrad(gsamAnisotropicWrap, texCoord + vCurrOffset, dx, dy).r;
		if (fCurrSampledHeight > fCurrRayHeight)
		{
			float delta1 = fCurrSampledHeight - fCurrRayHeight;
			float delta2 = (fCurrRayHeight + fStepSize) - fLastSampledHeight;
			float ratio = delta1 / (delta1 + delta2);
			vCurrOffset = (ratio)* vLastOffset + (1.0 - ratio) * vCurrOffset;
			nCurrSample = nNumSamples + 1;
		}
		else
		{
			fCurrRayHeight -= fStepSize;
			vLastOffset = vCurrOffset;
			vCurrOffset += fStepSize * vMaxOffset;
			fLastSampledHeight = fCurrSampledHeight;
			nCurrSample++;
		}
	}

	// Get the final coordinates
	float2 vFinalCoords = texCoord + vCurrOffset;
	return vFinalCoords;
}

#endif
