
#include "shaderRoot.hlsl"

struct VertexOut
{
	float4 PosH			: SV_POSITION;
	float3 PosW			: POSITION;
	float3 NormalW		: NORMAL;
	float3 TangentW		: TANGENT;
	float2 TexC			: TEXCOORD;
};

struct PixelOut
{
	float4 Color0		: SV_Target0; // color
	float4 Color1		: SV_Target1; // normal
	float4 Color2		: SV_Target2; // normal view space
	float Color3		: SV_Target3; // depth
	float4 Color4		: SV_Target4; // PBR data
};

PixelOut main(VertexOut pin)
{
	PixelOut pout = (PixelOut)0.0f;
	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	float3 fresnelR0 = matData.FresnelR0;
	float  roughness = matData.Roughness;

	float3x3 TBN = GetTBN(pin.NormalW, pin.TangentW);
	// Interpolating normal can unnormalize it, so renormalize it.
	pin.NormalW = normalize(pin.NormalW);

	// Calculate new parallaxed texture coordinates.
	float fHeightMapScale = 0.0125f;
	float4x4 texTransform = mul(gTexTransform, matData.MatTransform);
	fHeightMapScale *= texTransform._m20*texTransform._m20 + texTransform._m21*texTransform._m21 + texTransform._m22*texTransform._m22;
	float2 vFinalCoords = GetParallaxOcclusionTextureCoordinateOffset(pin.TexC, pin.PosW, pin.NormalW, fHeightMapScale, texTransform, TBN);

	float4 normalMapSample = gTextureMaps[DEFERRED_RENDER_TARGET_NORMAL_MAP].Sample(gsamAnisotropicWrap, vFinalCoords);
	float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, TBN);

	// Dynamically look up the texture in the array.
	diffuseAlbedo *= gTextureMaps[DEFERRED_RENDER_TARGET_DIFFUSE_MAP].Sample(gsamAnisotropicWrap, vFinalCoords);

	// Fetch the PBR data
	float texMetalness = gTextureMaps[DEFERRED_RENDER_TARGET_METALNESS_MAP].Sample(gsamAnisotropicWrap, vFinalCoords).r;
	float texRoughness = 0.0f;// gTextureMaps[roughnessMapIndex].Sample(gsamAnisotropicWrap, vFinalCoords).r;
	float texAO = 0.0f;// gTextureMaps[aoMapIndex].Sample(gsamAnisotropicWrap, vFinalCoords).r;

	//// Vector from point being lit to eye. 
	//float3 toEyeW = normalize(gEyePosW - pin.PosW);

	//// Light terms.
	//float4 ambient = gAmbientLight*diffuseAlbedo;

	//const float shininess = (1.0f - roughness) * normalMapSample.a;
	//Material mat = { diffuseAlbedo, fresnelR0, shininess };
	//float3 shadowFactor = 1.0f;
	//float4 directLight = ComputeLighting(gLights, mat, pin.PosW, bumpedNormalW, toEyeW, shadowFactor);

	//float4 litColor = ambient + directLight;

	//// Add in specular reflections.
	//float3 r = reflect(-toEyeW, bumpedNormalW);
	////float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
	//float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
	////litColor.rgb += shininess * fresnelFactor;// *reflectionColor.rgb;

	//// Common convention to take alpha from diffuse albedo.
	//litColor.a = diffuseAlbedo.a;

	pout.Color0 = diffuseAlbedo;
	pout.Color1 = float4(bumpedNormalW, 0.0f);
	pout.Color2 = mul(float4(bumpedNormalW, 0.0f), gView);
	pout.Color3 = pin.PosH.z;
	pout.Color4 = float4(texMetalness, texRoughness, texAO, 1.0f);
	return pout;
}
