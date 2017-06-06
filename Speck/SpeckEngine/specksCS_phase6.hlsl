
#include "specksCS_Root.hlsl"

// All constraints resolution used for solver iterations.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	if (speckIndex >= gParticleNum)
		return; // early exit

	SpeckData s = gSpecks[speckIndex];

	float sleepEpsilon = gSpeckRadius * 0.5f;
	float sleepEpsilonSq = sleepEpsilon;
	float3 diff = (s.pos_predicted - s.pos) / gDeltaTime;
	float difLenSq = dot(diff, diff);

	if (difLenSq > sleepEpsilonSq)
	{
		s.pos = s.pos_predicted;
	}

	s.vel = diff;

	gSpecks[speckIndex] = s;
}
