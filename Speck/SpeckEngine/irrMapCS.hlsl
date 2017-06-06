
#include "cpuGpuDefines.txt"

#define PI 3.14159265359f
#define PI_DIV_2 (PI*0.5f)

cbuffer cbSettings					: register(b0)
{
	uint gWidth;
	uint gHeight;

	float gVal0;
	float gVal1;
	float gVal2;
	float gVal3;
};

// Sampler rays
StructuredBuffer<float4> gSamRays	: register(t0);
// Enviroment map and it's sampler.
TextureCube gCubeMap				: register(t1);
// Sampler
SamplerState gsamLinearWrap			: register(s0);
// Output (irradiance map)
RWTexture2D<float4> gOutput			: register(u0);


//#define CacheSize (N + 2*gMaxBlurRadius)
//groupshared float4 gCache[CacheSize];

[numthreads(CUBE_MAP_N_THREADS, 1, 1)]
void main(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	// Spherical to cartesian transformation
	float rho = 1.0f; // just not to catch tiled gradients on the edge of the box, better against repetitiveness
	float phi = (float)dispatchThreadID.x / gWidth * PI*2.0;
	float theta = (float)dispatchThreadID.y / gHeight * PI;
	theta += 0.1f / gHeight; // add a fraction of a textel to te able to calculate the cross product later on.
	float3 up = float3(rho*cos(phi)*sin(theta), rho*cos(theta), rho*sin(phi)*sin(theta));

	// Used to create DCM https://en.wikipedia.org/wiki/Direction_cosine
	float phi2 = phi - PI_DIV_2;
	float3 right = float3(rho*cos(phi2)*sin(theta), rho*cos(theta), rho*sin(phi2)*sin(theta));
	float3 forward = normalize(cross(up, right));
	right = normalize(cross(forward, up));
	float3x3 M = float3x3(right, up, forward);
	// If there are multiple batches of sampler ray groups the computation continues on previous work.
	float4 res = gOutput[dispatchThreadID.xy] * (dispatchThreadID.z) * CUBE_MAP_SAMPLER_RAYS_GROUP_COUNT;

	[unroll]
	for (int i = 0; i < CUBE_MAP_SAMPLER_RAYS_GROUP_COUNT; ++i)
	{
		// Dynamically look up the texture in the random direction on the hemisphere.
		float3 ray = gSamRays[i + (dispatchThreadID.z) * CUBE_MAP_SAMPLER_RAYS_GROUP_COUNT].xyz;
		float3 sampledRay = mul(ray, M);
		res += gCubeMap.SampleLevel(gsamLinearWrap, sampledRay, 0);
	}
	res /= (dispatchThreadID.z + 1) * CUBE_MAP_SAMPLER_RAYS_GROUP_COUNT;

	//// Wait for all threads to finish.
	//GroupMemoryBarrierWithGroupSync();


	//res = gCubeMap.SampleLevel(gsamLinearWrap, up, 0);
	gOutput[dispatchThreadID.xy] = res;
}
