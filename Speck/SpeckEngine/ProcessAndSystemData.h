
#ifndef PROCESS_AND_SYSTEMDATA_H
#define PROCESS_AND_SYSTEMDATA_H

#include "SpeckEngineDefinitions.h"

namespace Speck
{
	struct ProcessAndSystemData
	{
		// Frames per second
		float mFPS;

		// Milieconds per frame
		float m_mSPF;

		// Total Virtual Memory
		ULONGLONG mTotalVirtualMem;

		// Virtual Memory currently used
		ULONGLONG mVirtualMemUsed;

		// Virtual Memory currently used by current process
		SIZE_T mVirtualMemUsedByMe;

		// Total Physical Memory(RAM)
		ULONGLONG mTotalPhysMem;

		// Physical Memory currently used
		ULONGLONG mPhysMemUsed;

		// Physical Memory currently used by current process
		SIZE_T mPhysMemUsedByMe;

		// CPU currently used
		double mCPU_usage;

		// CPU currently used by current process
		double mCPU_usageByMe;
	};
}

#endif
