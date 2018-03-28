
#include "cpuGpuDefines.txt"
#include "SSAORoot.hlsl"

struct VertexIn
{
	float3 PosL		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
	// Texture coordinates
	vout.TexC = vin.TexC;

	// Quad covering screen in NDC space.
	vout.PosH = float4(vin.PosL, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, gInvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}
