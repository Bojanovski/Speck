
#include "specksCS_Root.hlsl"

// Specks contact constraints generation.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	if (speckIndex >= gParticleNum)
		return; // early exit

	SpeckData thisSpeck = gSpecks[speckIndex];
	float doubleSpeckRadius = gSpeckRadius * 2.0f;
	float d = doubleSpeckRadius * COLLISION_DETECTION_MULTIPLIER;
	float h = doubleSpeckRadius * COLLISION_DETECTION_MULTIPLIER; // for density kernels
	float ro0 = (thisSpeck.mass) / (pow(gSpeckRadius, 3.0f)*PI*4.0f/3.0f); // rest densitiy
	float invRo0 = 1.0f / ro0;
	float roi = 0.0f; // densitiy estimator
	float grad_pi_Ci = 0.0f;
	float lambdaDenominator = 0.0f; // TU TREBA ICI ONAJ EPSILON

	// Go through neighbour cells.
	uint c = gSpeckCollisionSpaces[speckIndex].count;
	[unroll(27)]
	for (uint i = 0; i < c; ++i)
	{
		// Get the neighbour cell index.
		uint neighbourCellIndex = gSpeckCollisionSpaces[speckIndex].cells[i].index;
		uint specksNum = gSPCells[neighbourCellIndex].count; // number of specks
		if (specksNum > MAX_SPECKS_PER_CELL) specksNum = MAX_SPECKS_PER_CELL; // in case there was an overflow

		// Test collision for each speck in neighbour cell.
		//[unroll(MAX_SPECKS_PER_CELL)]
		for (uint l = 0; l < specksNum; ++l)
		{
			// Get the neighbour speck
			uint neighbourSpeckIndex = gSPCells[neighbourCellIndex].specks[l].index;
			if (neighbourSpeckIndex == speckIndex)
				continue; // do not check collision with itself

			// Get neighbour speck.
			SpeckData ns = gSpecks[neighbourSpeckIndex];
			// Calculate it's distance to the speck we are processing
			float3 diff = ns.pos - thisSpeck.pos;
			float dist = length(diff);
			if (dist < d)
			{
				// add this speck as a contact constraint
				SpeckContactConstraint cc;
				cc.speckIndex = neighbourSpeckIndex;
				uint posToWrite = gSpecksConstraints[speckIndex].numSpeckContacts;
				if (posToWrite < NUM_SPECK_CONTACT_CONSTRAINTS_PER_SPECK)
				{
					gSpecksConstraints[speckIndex].speckContacts[posToWrite] = cc;

					// Density values
					roi += ns.mass * W_poly6(dist, h);
					float grad_pj_Ci = -invRo0 * ns.mass * W_spiky_d(dist, h);
					lambdaDenominator += grad_pj_Ci*grad_pj_Ci;
					grad_pi_Ci += ns.mass * W_spiky_d(dist, h);
				}
				++gSpecksConstraints[speckIndex].numSpeckContacts;
			}
		}
	}

	grad_pi_Ci *= invRo0;
	lambdaDenominator += grad_pi_Ci*grad_pi_Ci;
	roi += thisSpeck.mass * W_poly6(0.0f, h); // this particle's contribution to the density
	float C_density_constraint = roi * invRo0 - 1.0f; // densitiy constraint
	float lambda = -C_density_constraint / (lambdaDenominator + 100.0f);
	gSpecksConstraints[speckIndex].densityConstraintLambda = lambda;
}
