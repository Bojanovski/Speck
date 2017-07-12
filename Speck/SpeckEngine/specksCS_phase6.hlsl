
#include "specksCS_Root.hlsl"

float3 SafeDiv(float3 a, float b)
{
	if (b == 0.0f)
		a = float3(0.0f, 0.0f, 0.0f);
	else
		a /= b;

	return a;
}

// All constraints resolution used for solver iterations.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	if (speckIndex >= gParticleNum)
		return; // early exit

	SpeckData s = gSpecks[speckIndex];
	uint thisSpeckUpperCode = s.code & SPECK_CODE_UPPER_WORD_MASK;

	float sleepEpsilon = gSpeckRadius * 0.5f;
	float sleepEpsilonSq = sleepEpsilon * sleepEpsilon;
	float3 diff = SafeDiv(s.pos_predicted - s.pos, gDeltaTime);
	float difLenSq = dot(diff, diff);

	if (difLenSq >= sleepEpsilonSq ||
		thisSpeckUpperCode == SPECK_CODE_FLUID) // fluids behave differntly somehow :S
	{
		s.pos = s.pos_predicted;
	}

	s.vel = diff;

	gSpecks[speckIndex] = s;
}
