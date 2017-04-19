
#ifndef PSO_GROUP_H
#define PSO_GROUP_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"

namespace Speck
{
	struct RenderItem;

	// Pipeline state group
	struct PSOGroup
	{
		PSOGroup() = default;
		PSOGroup(const PSOGroup& v) = delete;
		PSOGroup& operator=(const PSOGroup& v) = delete;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
		std::vector<std::unique_ptr<RenderItem>> mRItems;
	};
}

#endif
