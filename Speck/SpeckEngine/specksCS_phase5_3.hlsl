
#include "specksCS_Root.hlsl"

// Rigid body constraints are handled here per speck.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	if (speckIndex >= gParticleNum)
		return; // early exit

	SpeckData thisSpeck = gSpecks[speckIndex];
	float3 p1 = thisSpeck.pos_predicted;
	float3 totalDeltaP = float3(0.0f, 0.0f, 0.0f);
	uint n = gSpecksConstraints[speckIndex].numSpeckRigidBodies;
	

	for (uint i = 0; i < n; ++i)
	{
		RigidBodyConstraint rbc = gSpecksConstraints[speckIndex].speckRigidBodyConstraint[i];
		float3 r = rbc.posInRigidBody;
		float4x4 world = gRigidBodies[rbc.rigidBodyIndex].world;
		float3 newPos = mul(float4(r, 1.0f), world).xyz;
		float3 deltaP = newPos - p1;
		totalDeltaP += deltaP;
	}





	if (n > 0)
	{
		float3 appliedDeltaP = totalDeltaP / n;
		thisSpeck.pos_predicted += appliedDeltaP;
		gSpecks[speckIndex] = thisSpeck;
	}
}
