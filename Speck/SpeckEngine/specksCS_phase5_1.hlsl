
#include "specksCS_Root.hlsl"

// This phase is repeated so that full parallel reduction can be performed.
// First iteration sets the position of the speck and the last saves the calculated center of mass to the rigid body.
[numthreads(SPECK_RIGID_BODY_LINKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint linkIndex = dispatchThreadID.x;
	if (linkIndex >= gNumSpeckRigidBodyLinks)
		return; // early exit

	SpeckRigidBodyLink thisLink = gSpeckRigidBodyLinks[linkIndex];
	uint posInBlock = linkIndex - thisLink.speckLinksBlockStart;
	// Not all 'nodes' calculate the sum, only some of them.
	// useOnlyEvery denotes how many are skipped.
	uint useOnlyEvery = 1 << (gPhaseIteration);
	float3 cm;
	float m;
	if (gPhaseIteration == 0)
	{ 
		// first iteration, calculate c per link (particle)
		SpeckData thisLinkSpeck = gSpecks[thisLink.speckIndex];
		cm = thisLinkSpeck.pos_predicted * thisLinkSpeck.mass;
		m = thisLinkSpeck.mass;
		
		// Save the data
		gSpeckRigidBodyLinkCache[linkIndex].cm = cm;
		gSpeckRigidBodyLinkCache[linkIndex].m = m;
	}
	else if (posInBlock % useOnlyEvery == 0)
	{ 
		// rest of the iterations, summation
		cm = gSpeckRigidBodyLinkCache[linkIndex].cm;
		m = gSpeckRigidBodyLinkCache[linkIndex].m;

		uint offset = 1 << (gPhaseIteration - 1);
		uint otherIndex = linkIndex + offset;
		if (otherIndex - thisLink.speckLinksBlockStart < thisLink.speckLinksBlockCount)
		{
			cm += gSpeckRigidBodyLinkCache[otherIndex].cm;
			m += gSpeckRigidBodyLinkCache[otherIndex].m;
		}
		
		// Save the data
		gSpeckRigidBodyLinkCache[linkIndex].cm = cm;
		gSpeckRigidBodyLinkCache[linkIndex].m = m;

		// If last -> iteration update the data in the rigid body structure
		if (gPhaseIteration == gNumPhaseIterations - 1)
		{
			gRigidBodies[thisLink.rbIndex].c = cm / m;
		}
	}
}
