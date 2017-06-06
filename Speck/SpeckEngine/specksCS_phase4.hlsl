
#include "specksCS_Root.hlsl"

// Contact constraints resolution used for stabilization iterations.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	if (speckIndex >= gParticleNum)
		return; // early exit

	SpeckData thisSpeck = gSpecks[speckIndex];
	uint thisSpeckUpperCode = thisSpeck.code & SPECK_CODE_UPPER_WORD_MASK;
	float3 p1 = thisSpeck.pos;
	float w1 = thisSpeck.invMass;
	float d = gSpeckRadius * 2.0f;
	float3 totalDeltaP = float3(0.0f, 0.0f, 0.0f);
	int n = 0;

	// Specks
	for (uint i = 0; i < gSpecksConstraints[speckIndex].numSpeckContacts; ++i)
	{
		uint otherSpeckIndex = gSpecksConstraints[speckIndex].speckContacts[i].speckIndex;
		SpeckData otherSpeck = gSpecks[otherSpeckIndex];
		uint otherSpeckUpperCode = otherSpeck.code & SPECK_CODE_UPPER_WORD_MASK;

		if (thisSpeckUpperCode == SPECK_CODE_FLUID && // UREDITI
			thisSpeckUpperCode == otherSpeckUpperCode)
			continue;

		float w2 = otherSpeck.invMass;
		float w = w1 + w2;
		float3 p2 = otherSpeck.pos;
		float3 p21 = p1 - p2;
		float lenP21 = length(p21);
		if (lenP21 == 0.0f)
		{ // in a highly improbable case where both specks share the same position in space
			lenP21 = 0.001f;
			p21 = float3(0.0f, 0.0f, (speckIndex < otherSpeckIndex) * lenP21);
		}
		float s = (lenP21 - d) / w;
		s = min(s, 0); // This is inequality constraint, so clamp every positive value of s to zero.
		float3 grad_p1_C = (p21) / (lenP21);
		float3 deltaP = -w1 * s * grad_p1_C;
		totalDeltaP += deltaP;
		++n;
	}

	// Static colliders
	for (uint j = 0; j < gSpecksConstraints[speckIndex].numStaticCollider; ++j)
	{
		float w2 = 0.0f;
		float w = w1 + w2;
		StaticColliderContactConstraint sccc = gSpecksConstraints[speckIndex].staticColliderContacts[j];
		float s = (dot(p1 - sccc.pos, sccc.normal) - gSpeckRadius) / w;
		s = min(s, 0); // This is inequality constraint, so clamp every positive value of s to zero.
		float3 grad_p1_C = sccc.normal;
		float3 deltaP = -w1 * s * grad_p1_C;
		totalDeltaP += deltaP;
		++n;
	}

	if (n > 0)
	{
		float3 appliedDeltaP = totalDeltaP  / n;
		gSpecks[speckIndex].pos += appliedDeltaP;
		gSpecks[speckIndex].pos_predicted += appliedDeltaP;
	}
}
