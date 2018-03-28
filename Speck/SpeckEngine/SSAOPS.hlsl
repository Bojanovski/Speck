
#include "cpuGpuDefines.txt"
#include "SSAORoot.hlsl"

// Determines how much the sample point q occludes the point p as a function of distZ.
float OcclusionFunction(float distZ)
{
	//
	// If depth(q) is "behind" depth(p), then q cannot occlude p.  Moreover, if 
	// depth(q) and depth(p) are sufficiently close, then we also assume q cannot
	// occlude p because q needs to be in front of p by Epsilon to occlude p.
	//
	// We use the following function to determine the occlusion.  
	// 
	//
	//       1.0     -------------\
	//               |           |  \
	//               |           |    \
	//               |           |      \ 
	//               |           |        \
	//               |           |          \
	//               |           |            \
	//  ------|------|-----------|-------------|---------|--> zv
	//        0     Eps          z0            z1        
	//

	float occlusion = 0.0f;
	if (distZ > gSurfaceEpsilon)
	{
		float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;

		// Linearly decrease occlusion from 1 to 0 as distZ goes 
		// from gOcclusionFadeStart to gOcclusionFadeEnd.	
		occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength);
	}

	return occlusion;
}

float main(VertexOut pin) : SV_Target
{
	// p -- the point we are computing the ambient occlusion for.
	// n -- normal vector at p.
	// q -- a random offset from p.
	// r -- a potential occluder that might occlude p.

	// Get viewspace normal and z-coord of this pixel.  
	float3 n = normalize(gTextureMaps[SSAO_INPUT_TEXTURE_NORMAL_VIEW_SPACE_MAP].SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz);
	float pz = gTextureMaps[SSAO_INPUT_TEXTURE_DEPTH_MAP].SampleLevel(gsamPointClamp, pin.TexC, 0.0f).r;
	pz = NDCDepthToViewDepth(pz);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	float3 p = (pz / pin.PosV.z)*pin.PosV;

	// Extract random vector and map from [0,1] --> [-1, +1].
	float3 randVec = 2.0f*gTextureMaps[SSAO_INPUT_TEXTURE_RANDOM_MAP].SampleLevel(gsamLinearWrap, 4.0f*pin.TexC, 0.0f).rgb - 1.0f;

	//return gTextureMaps[SSAO_INPUT_TEXTURE_DEPTH_MAP].SampleLevel(gsamPointClamp, pin.TexC, 0.0f).r;

	float occlusionSum = 0.0f;

	// Sample neighboring points about p in the hemisphere oriented by n.
	for (int i = 0; i < SSAO_RANDOM_SAMPLES_N; ++i)
	{
		// Are offset vectors are fixed and uniformly distributed (so that our offset vectors
		// do not clump in the same direction).  If we reflect them about a random vector
		// then we get a random uniform distribution of offset vectors.
		float3 offset = reflect(gOffsetVectors[i].xyz, randVec);

		// Flip offset vector if it is behind the plane defined by (p, n).
		float flip = sign(dot(offset, n));

		// Sample a point near p within the occlusion radius.
		float3 q = p + flip * gOcclusionRadius * offset;

		// Project q and generate projective tex-coords.  
		float4 projQ = mul(float4(q, 1.0f), gProjTex);
		projQ /= projQ.w;

		// Find the nearest depth value along the ray from the eye to q (this is not
		// the depth of q, as q is just an arbitrary point near p and might
		// occupy empty space).  To find the nearest depth we look it up in the depthmap.

		float rz = gTextureMaps[SSAO_INPUT_TEXTURE_DEPTH_MAP].SampleLevel(gsamPointClamp, projQ.xy, 0.0f).r;
		rz = NDCDepthToViewDepth(rz);

		// Reconstruct full view space position r = (rx,ry,rz).  We know r
		// lies on the ray of q, so there exists a t such that r = t*q.
		// r.z = t*q.z ==> t = r.z / q.z

		float3 r = (rz / q.z) * q;

		//
		// Test whether r occludes p.
		//   * The product dot(n, normalize(r - p)) measures how much in front
		//     of the plane(p,n) the occluder point r is.  The more in front it is, the
		//     more occlusion weight we give it.  This also prevents self shadowing where 
		//     a point r on an angled plane (p,n) could give a false occlusion since they
		//     have different depth values with respect to the eye.
		//   * The weight of the occlusion is scaled based on how far the occluder is from
		//     the point we are computing the occlusion of.  If the occluder r is far away
		//     from p, then it does not occlude it.
		// 

		float distZ = p.z - r.z;
		float dp = max(dot(n, normalize(r - p)), 0.0f);

		float occlusion = dp*OcclusionFunction(distZ);

		occlusionSum += occlusion;
	}

	occlusionSum /= SSAO_RANDOM_SAMPLES_N;

	float access = 1.0f - occlusionSum;

	// Sharpen the contrast of the SSAO map to make the SSAO affect more dramatic.
	return saturate(pow(access, 6.0f));
}
