
#include "specksCS_Root.hlsl"

// Euler integration of velocity and position.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	if (speckIndex >= gParticleNum)
		return; // early exit

	SpeckData sd = gSpecks[speckIndex];
	for (uint i = 0; i < gNumExternalForces; ++i)
	{
		switch (gExternalForces[i].type)
		{
			case FORCE_TYPE_ACCELERATION: // apply vector as acceleration (ignore the mass of the particle)
				sd.vel = sd.vel + gExternalForces[i].vec * gDeltaTime;
				sd.vel *= 0.999f;
				break;

			default:
				break;
		}
	}

	sd.pos_predicted = sd.pos + sd.vel * gDeltaTime;
	gSpecks[speckIndex] = sd;
	//Flex 2014 whitepaper also recommends mass scaling for stacked rigid bodies.
}
