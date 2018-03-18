
#include "shaderRoot.hlsl"

struct VertexIn
{
	float4 PosL[MAX_BONES_PER_VERTEX]			: POSITION;
	float3 NormalL[MAX_BONES_PER_VERTEX]		: NORMAL;
	float3 TangentU[MAX_BONES_PER_VERTEX]		: TANGENT;
	float2 TexC									: TEXCOORD;
	int4 BoneIndices							: BONEINDICES;
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
	float4 posW1 = mul(vin.PosL[0], world1);
	float4 posW2 = mul(vin.PosL[1], world2);
	float4 posW3 = mul(vin.PosL[2], world3);
	float4 posW4 = mul(vin.PosL[3], world4);
	float4 posW = posW1 + posW2 + posW3 + posW4;
	vout.PosW = posW.xyz;

	// Use inverse transpose world matrix for normal and tangent.
	float3 normalW1 = mul(vin.NormalL[0], invTransposeWorld1);
	float3 normalW2 = mul(vin.NormalL[1], invTransposeWorld2);
	float3 normalW3 = mul(vin.NormalL[2], invTransposeWorld3);
	float3 normalW4 = mul(vin.NormalL[3], invTransposeWorld4);
	vout.NormalW = normalW1 + normalW2 + normalW3 + normalW4;

	float3 tangentW1 = mul(vin.TangentU[0], invTransposeWorld1);
	float3 tangentW2 = mul(vin.TangentU[1], invTransposeWorld2);
	float3 tangentW3 = mul(vin.TangentU[2], invTransposeWorld3);
	float3 tangentW4 = mul(vin.TangentU[3], invTransposeWorld4);
	vout.TangentW = tangentW1 + tangentW2 + tangentW3 + tangentW4;

	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);

	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

	return vout;
}
