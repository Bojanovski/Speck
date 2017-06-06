
#include "specksCS_Root.hlsl"

// Copy the calcuated positions back to the instances buffer.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	if (speckIndex >= gParticleNum)
		return; // early exit

	// New value
	gInstancesOut[speckIndex].Position = gSpecks[speckIndex].pos;
	// Copy this straight from the original input.
	gInstancesOut[speckIndex].MaterialIndex = gInstancesIn[speckIndex].materialIndex;
}
