
// Include structures and functions for lighting.
#include "shaderRoot.hlsl"

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	
	// nointerpolation is used so the index is not interpolated 
	// across the triangle.
	nointerpolation uint MatIndex  : MATINDEX;
};

struct PixelOut
{
	float4 Color0    : SV_Target0; // color
	float4 Color1    : SV_Target1; // normal
	float4 Color2    : SV_Target2; // depth
};

PixelOut main(VertexOut pin)
{
	PixelOut pout = (PixelOut)0.0f;

	// Fetch the material data.
	MaterialData matData = gMaterialData[pin.MatIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float  roughness = matData.Roughness;

    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    //// Vector from point being lit to eye. 
    //float3 toEyeW = normalize(gEyePosW - pin.PosW);

    //// Light terms.
    //float4 ambient = gAmbientLight*diffuseAlbedo;

    //const float shininess = 1.0f - roughness;
    //Material mat = { diffuseAlbedo, fresnelR0, shininess };
    //float3 shadowFactor = 1.0f;
    //float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
    //    pin.NormalW, toEyeW, shadowFactor);

    //float4 litColor = ambient + directLight;

    //// Common convention to take alpha from diffuse albedo.
    //litColor.a = diffuseAlbedo.a;

	pout.Color0 = diffuseAlbedo;
	pout.Color1 = NormalInWorldToTextel(pin.NormalW);
	pout.Color2 = float4(0.0f, 0.0f, 1.0f, 1.0f);
	return pout;
}
