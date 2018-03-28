
#include "cpuGpuDefines.txt"
#include "SSAORoot.hlsl"

float main(VertexOut pin) : SV_Target
{
	// unpack into float array.
	float blurWeights[12] =
	{
		gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
		gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
		gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
	};
	
	float2 invRenderTargetSize = float2(1.0f / gSSAOMapsWidth, 1.0f / gSSAOMapsHeight);
	float2 texOffset;
	if (gPassType == 0) // horizontal blur
	{
		texOffset = float2(invRenderTargetSize.x, 0.0f);
	}
	else // vertical blur
	{
		texOffset = float2(0.0f, invRenderTargetSize.y);
	}
	
	// The center value always contributes to the sum.
	float color = blurWeights[SSAO_BLUR_RADIUS] * gPreviousResultMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0).r;
	float totalWeight = blurWeights[SSAO_BLUR_RADIUS];
	
	float3 centerNormal = gTextureMaps[SSAO_INPUT_TEXTURE_NORMAL_VIEW_SPACE_MAP].SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz;
	float  centerDepth = NDCDepthToViewDepth(gTextureMaps[SSAO_INPUT_TEXTURE_DEPTH_MAP].SampleLevel(gsamPointClamp, pin.TexC, 0.0f).r);
	
	for (float i = -SSAO_BLUR_RADIUS; i <= SSAO_BLUR_RADIUS; ++i)
	{
		// We already added in the center weight.
		if (i == 0)
			continue;
	
		float2 tex = pin.TexC + i*texOffset;
	
		float3 neighborNormal = gTextureMaps[SSAO_INPUT_TEXTURE_NORMAL_VIEW_SPACE_MAP].SampleLevel(gsamPointClamp, tex, 0.0f).xyz;
		float  neighborDepth = NDCDepthToViewDepth(gTextureMaps[SSAO_INPUT_TEXTURE_DEPTH_MAP].SampleLevel(gsamPointClamp, tex, 0.0f).r);
	
		//
		// If the center value and neighbor values differ too much (either in 
		// normal or depth), then we assume we are sampling across a discontinuity.
		// We discard such samples from the blur.
		//
	
		if (dot(neighborNormal, centerNormal) >= 0.8f && abs(neighborDepth - centerDepth) <= 0.2f)
		{
			float weight = blurWeights[i + SSAO_BLUR_RADIUS];
	
			// Add neighbor pixel to blur.
			color += weight*gPreviousResultMap.SampleLevel(gsamPointClamp, tex, 0.0).r;
	
			totalWeight += weight;
		}
	}
	
	// Compensate for discarded samples by making total weights sum to 1.
	return color / totalWeight;
}
