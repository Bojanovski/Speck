
#ifndef TEXTURE_GENERATOR_H
#define TEXTURE_GENERATOR_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"

namespace Speck
{
	void BuildRandomVectorTexture(
		ID3D12Device *device,
		ID3D12GraphicsCommandList* cmdList,
		Microsoft::WRL::ComPtr<ID3D12Resource> &mRandomVectorMap,
		Microsoft::WRL::ComPtr<ID3D12Resource> &mRandomVectorMapUploadBuffer);

	void BuildColorTexture(
		const DirectX::XMFLOAT4 &color,
		ID3D12Device *device,
		ID3D12GraphicsCommandList* cmdList,
		Microsoft::WRL::ComPtr<ID3D12Resource> &mColorMap,
		Microsoft::WRL::ComPtr<ID3D12Resource> &mColorMapUploadBuffer);
}

#endif
