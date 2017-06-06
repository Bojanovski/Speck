
#include "specksCS_Root.hlsl"

// This phase iterates over each cell and initializes its 'count' value to zero.
[numthreads(CELLS_CS_N_THREADS, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
	if ((uint)dispatchThreadID.x >= gHashTableSize) 
		return; // early exit

	gSPCells[dispatchThreadID.x].count = 0;
}
