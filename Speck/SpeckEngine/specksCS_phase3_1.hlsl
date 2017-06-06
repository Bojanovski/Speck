
#include "specksCS_Root.hlsl"

struct CollTestRes
{
	float dist;
	float3 n, p;
};

CollTestRes GetDistanceFromStaticCollider(float3 pos, StaticColliderData inData)
{
	StaticColliderElementData temp;
	CollTestRes ret;
	ret.dist = -FLT_MAX;
	// For each face
	for (uint i = 0; i < inData.facesCount; ++i)
	{
		temp = gStaticColliderFaces[inData.facesStartIndex + i];
		float3 p = mul(float4(temp.vec[0], 1.0f), inData.world).xyz;
		float3 n = mul(float4(temp.vec[1], 0.0f), inData.invTransposeWorld).xyz;
		n = normalize(n);
		float tempDist = GetDistanceFromPlane(pos, p, n);
		if (tempDist > ret.dist)
		{
			ret.dist = tempDist;
			ret.p = p;
			ret.n = n;
		}
	}
	return ret;
}

// Static collider contact constraints generation.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	uint staticColliderIndex = dispatchThreadID.y;
	if (speckIndex >= gParticleNum || staticColliderIndex >= gNumStaticColliders)
		return; // early exit

	SpeckData thisSpeck = gSpecks[speckIndex];
	float doubleSpeckRadius = gSpeckRadius * 2.0f;
	float dSq = doubleSpeckRadius * doubleSpeckRadius;
	dSq = dSq * COLLISION_DETECTION_MULTIPLIER * COLLISION_DETECTION_MULTIPLIER;

	StaticColliderData scd = gStaticColliders[staticColliderIndex];
	CollTestRes testRes = GetDistanceFromStaticCollider(thisSpeck.pos, scd);
	float dClamped = max(0.0f, testRes.dist);
	float distSq = dClamped * dClamped;

	if (distSq < dSq)
	{
		// add this static collider as a contact constraint
		StaticColliderContactConstraint scc;
		scc.colliderID = staticColliderIndex;
		scc.pos = testRes.p;
		scc.normal = testRes.n;

		// Multiple static colliders from different threads will access this speck data so atomics are needed.
		uint posToWrite;
		InterlockedAdd(gSpecksConstraints[speckIndex].numStaticCollider, 1, posToWrite);
		if (posToWrite < NUM_STATIC_COLLIDERS_CONTACT_CONSTRAINTS_PER_SPECK)
		{
			// Register this speck to the appropriate position in the assigned cell.
			gSpecksConstraints[speckIndex].staticColliderContacts[posToWrite] = scc;
		}

	}
}
