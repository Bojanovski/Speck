
// Include structures and functions for lighting.
#include "shaderRoot.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	
	// nointerpolation is used so the index is not interpolated 
	// across the triangle.
	nointerpolation uint MatIndex  : MATINDEX;
};

VertexOut main(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout = (VertexOut)0.0f;
	
	// Fetch the instance data.
	InstanceData instData = gInstanceData[instanceID];
	uint matIndex = instData.MaterialIndex;

	vout.MatIndex = matIndex;
	
	// Fetch the material data.
	MaterialData matData = gMaterialData[matIndex];
	
    // Translate to world space.
	vout.PosW = vin.PosL + instData.Position;
    vout.NormalW = vin.NormalL;

    // Transform to homogeneous clip space.
    vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
	
    return vout;
}
