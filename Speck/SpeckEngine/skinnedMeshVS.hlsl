
// Include structures and functions for lighting.
#include "shaderRoot.hlsl"

struct VertexIn
{
	float3 PosL			: POSITION;
	float3 NormalL		: NORMAL;
	float3 TangentU		: TANGENT;
	float2 TexC			: TEXCOORD;
	float4 BoneWeights	: WEIGHTS;
	int4 BoneIndices	: BONEINDICES;
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
	float4x4 world1;
	float4x4 world2;
	float4x4 world3;
	float4x4 world4;
	float3x3 invTransposeWorld1;
	float3x3 invTransposeWorld2;
	float3x3 invTransposeWorld3;
	float3x3 invTransposeWorld4;
	switch (gRenderItemType)
	{
		case RENDER_ITEM_TYPE_SPECK_SKELETAL_BODY:
		{
			int speckRigidBodyIndex1 = vin.BoneIndices[0];
			int speckRigidBodyIndex2 = vin.BoneIndices[1];
			int speckRigidBodyIndex3 = vin.BoneIndices[2];
			int speckRigidBodyIndex4 = vin.BoneIndices[3];

			world1 = mul(gTransform, gRigidBodies[speckRigidBodyIndex1].world);
			world2 = mul(gTransform, gRigidBodies[speckRigidBodyIndex2].world);
			world3 = mul(gTransform, gRigidBodies[speckRigidBodyIndex3].world);
			world4 = mul(gTransform, gRigidBodies[speckRigidBodyIndex4].world);

			// Since speck rigid body transform has no scaling in it, simple cast is enough to get the inverse transpose transform.
			float3x3 speckRigidBodyInvTransposeTransform1 = (float3x3)gRigidBodies[speckRigidBodyIndex1].world;
			float3x3 speckRigidBodyInvTransposeTransform2 = (float3x3)gRigidBodies[speckRigidBodyIndex2].world;
			float3x3 speckRigidBodyInvTransposeTransform3 = (float3x3)gRigidBodies[speckRigidBodyIndex3].world;
			float3x3 speckRigidBodyInvTransposeTransform4 = (float3x3)gRigidBodies[speckRigidBodyIndex4].world;

			invTransposeWorld1 = mul((float3x3)gInvTransposeTransform, speckRigidBodyInvTransposeTransform1);
			invTransposeWorld2 = mul((float3x3)gInvTransposeTransform, speckRigidBodyInvTransposeTransform2);
			invTransposeWorld3 = mul((float3x3)gInvTransposeTransform, speckRigidBodyInvTransposeTransform3);
			invTransposeWorld4 = mul((float3x3)gInvTransposeTransform, speckRigidBodyInvTransposeTransform4);
		}
		break;

		default:
		{
			world1 = Identity4x4();
			world2 = Identity4x4();
			world3 = Identity4x4();
			world4 = Identity4x4();
			invTransposeWorld1 = Identity3x3();
			invTransposeWorld2 = Identity3x3();
			invTransposeWorld3 = Identity3x3();
			invTransposeWorld4 = Identity3x3();
		}
		break;
	}

	// Transform to world space.
	float4 posW1 = mul(float4(vin.PosL, 1.0f), world1);
	float4 posW2 = mul(float4(vin.PosL, 1.0f), world2);
	float4 posW3 = mul(float4(vin.PosL, 1.0f), world3);
	float4 posW4 = mul(float4(vin.PosL, 1.0f), world4);
	float4 posW =
		vin.BoneWeights.x * posW1 +
		vin.BoneWeights.y * posW2 +
		vin.BoneWeights.z * posW3 +
		vin.BoneWeights.w * posW4;
	vout.PosW = posW.xyz;

	// Use inverse transpose world matrix for normal and tangent.
	float3 normalW1 = mul(vin.NormalL, invTransposeWorld1);
	float3 normalW2 = mul(vin.NormalL, invTransposeWorld2);
	float3 normalW3 = mul(vin.NormalL, invTransposeWorld3);
	float3 normalW4 = mul(vin.NormalL, invTransposeWorld4);
	vout.NormalW =
		vin.BoneWeights.x * normalW1 +
		vin.BoneWeights.y * normalW2 +
		vin.BoneWeights.z * normalW3 +
		vin.BoneWeights.w * normalW4;

	float3 tangentW1 = mul(vin.TangentU, invTransposeWorld1);
	float3 tangentW2 = mul(vin.TangentU, invTransposeWorld2);
	float3 tangentW3 = mul(vin.TangentU, invTransposeWorld3);
	float3 tangentW4 = mul(vin.TangentU, invTransposeWorld4);
	vout.TangentW =
		vin.BoneWeights.x * tangentW1 +
		vin.BoneWeights.y * tangentW2 +
		vin.BoneWeights.z * tangentW3 +
		vin.BoneWeights.w * tangentW4;

	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);

	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

	return vout;
}
