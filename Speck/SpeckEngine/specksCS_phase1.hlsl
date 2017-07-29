
#include "specksCS_Root.hlsl"

// Purpose of this phase is to copy the speck data from initial format to the one more suitable to our problem
// and to assign it to the appropriate hash grid cell.
// Also all constraints are deleted.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	if (speckIndex >= gParticleNum)
		return; // early exit

	// Create new speck struct and save it.
	SpeckData newData = gSpecks[speckIndex]; // copy, because some values will remain unchanged

	// Should we reinitialize the speck?
	if (speckIndex >= gInitializeSpecksStartIndex)
	{
		newData.pos = newData.pos_predicted = gInstancesIn[speckIndex].position;
		newData.code = gInstancesIn[speckIndex].code;
		newData.mass = gInstancesIn[speckIndex].mass;
		newData.invMass = 1.0f / newData.mass;
		newData.frictionCoefficient = gInstancesIn[speckIndex].frictionCoefficient;
		[unroll(SPECK_SPECIAL_PARAM_N)]
		for (int j = 0; j < SPECK_SPECIAL_PARAM_N; ++j)
		{
			//newData.param[j] = gInstancesIn[speckIndex].param[j];
			SpeckUploadData temp = gInstancesIn[speckIndex];
			newData.param[j] = temp.param[j];
		}
		newData.vel = float3(0.0f, 0.0f, 0.0f);
	}

	// Compute the cell index.
	int3 cellPos = int3(
		(int)floor(newData.pos.x / gCellSize),
		(int)floor(newData.pos.y / gCellSize),
		(int)floor(newData.pos.z / gCellSize)
		);
	uint cellID = calcGridHash(cellPos, gHashTableSize);
	
	// Store updated speck.
	gSpecks[speckIndex] = newData;

	// Update the grid.
	uint posToWrite;
	InterlockedAdd(gSPCells[cellID].count, 1, posToWrite);
	if (posToWrite < MAX_SPECKS_PER_CELL)
	{
		// Register this speck to the appropriate position in the assigned cell.
		gSPCells[cellID].specks[posToWrite].index = speckIndex;
	}
	//else
		// All the specks that get assigned to the cell that has no more room
		// in it will behave as if they do not collide with other specks.




	// Also clear the constraints for this speck
	gSpecksConstraints[speckIndex].numSpeckContacts = 0;
	gSpecksConstraints[speckIndex].numStaticCollider = 0;
	gSpecksConstraints[speckIndex].numSpeckRigidBodies = 0;




	//gContactConstraints[speckIndex].speckContacts[0].speckID = 0;
	//gContactConstraints[speckIndex].speckContacts[1].speckID = 0;
	//gContactConstraints[speckIndex].speckContacts[2].speckID = 0;
	//gContactConstraints[speckIndex].speckContacts[3].speckID = 0;




	uint insertAt = 0;
	// For each neighbour cell
	[unroll(3)]
	for (uint i = 0; i < 3; ++i)
	{
		[unroll(3)]
		for (uint j = 0; j < 3; ++j)
		{
			[unroll(3)]
			for (uint k = 0; k < 3; ++k)
			{
				// Compute the neighbour cell index.
				int3 neighbourCellPos = cellPos + int3(1 - i, 1 - j, 1 - k);
				uint neighbourCellID = calcGridHash(neighbourCellPos, gHashTableSize);
				bool shouldAdd = true;

				// This commented code is useful if number of cells is too small.

				//// If it already exists in the array set the flag to skip adding it.
				//[unroll(i*3*3 + j*3 + k)]
				//for (uint l = 0; l < insertAt; ++l)
				//{
				//	if (gSpeckCollisionSpaces[speckIndex].cells[l] == neighbourCellID)
				//	{
				//		shouldAdd = false;
				//		break;
				//	}
				//}
				
				// Add the data
				if (shouldAdd)
				{
					gSpeckCollisionSpaces[speckIndex].cells[insertAt].index = neighbourCellID;
					++insertAt;
				}
			}
		}
	}
	gSpeckCollisionSpaces[speckIndex].count = insertAt;


	//for (uint i = insertAt; i < 27; ++i)
	//{
	//	gSpeckCollisionSpaces[speckIndex].cells[i] = 2;
	//}


}
