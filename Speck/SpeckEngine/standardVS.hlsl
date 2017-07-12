
// Include structures and functions for lighting.
#include "shaderRoot.hlsl"

struct VertexIn
{
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float3 TangentU : TANGENT;
	float2 TexC		: TEXCOORD;
};

struct VertexOut
{
	float4 PosH			: SV_POSITION;
	float3 PosW			: POSITION;
	float3 NormalW		: NORMAL;
	float3 TangentW		: TANGENT;
	float2 TexC			: TEXCOORD;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];

	// World matrix is type dependant
	float4x4 world;
	float3x3 invTransposeWorld;
	switch (gRenderItemType)
	{
		case RENDER_ITEM_TYPE_STATIC:
		{
			world = gTransform;
			invTransposeWorld = (float3x3)gInvTransposeTransform;
		}
		break;

		case RENDER_ITEM_TYPE_SPECK_RIGID_BODY:
		{
			uint speckRigidBodyIndex = gParam[0];
			world = mul(gTransform, gRigidBodies[speckRigidBodyIndex].world);
			// Since speck rigid body transform has no scaling in it, simple cast is enough to get the inverse transpose transform.
			float3x3 speckRigidBodyInvTransposeTransform = (float3x3)gRigidBodies[speckRigidBodyIndex].world;
			invTransposeWorld = mul(gInvTransposeTransform, speckRigidBodyInvTransposeTransform);
		}
		break;

		default:
		{
			world = Identity4x4();
			invTransposeWorld = Identity3x3();
		}
		break;
	}

	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), world);
	vout.PosW = posW.xyz;

	// Use inverse transpose world matrix for normal and tangent.
	vout.NormalW = mul(vin.NormalL, invTransposeWorld);
	vout.TangentW = mul(vin.TangentU, invTransposeWorld);

	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);

	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

	return vout;
}
