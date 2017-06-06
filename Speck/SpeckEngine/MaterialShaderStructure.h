
#ifndef MATERIAL_SHADER_STRUCTURES_H
#define MATERIAL_SHADER_STRUCTURES_H

#include "SpeckEngineDefinitions.h"
#include "MathHelper.h"

namespace Speck
{
	struct Material
	{
		Material() = default;
		virtual ~Material() = default;

		// Basic constants
		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
		float Roughness = 0.25f;

		// Index into constant buffer corresponding to this material.
		int MatCBIndex = -1;

		// Dirty flag indicating the material has changed and we need to update the constant buffer.
		// Because we have a material constant buffer for each FrameResource, we have to apply the
		// update to each FrameResource.  Thus, when we modify a material we should set 
		// NumFramesDirty = NUM_FRAME_RESOURCES so that each frame resource gets the update.
		int NumFramesDirty = NUM_FRAME_RESOURCES;
	};

	// Material that uses diffuse and normal map.
	struct PBRMaterial : public Material
	{
		PBRMaterial() = default;
		virtual ~PBRMaterial() override = default;

		// SRV heap for textures.
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

		// Used in texture mapping.
		DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	};
}

#endif
