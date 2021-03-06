
#ifndef FRAME_RESOURCE_H
#define FRAME_RESOURCE_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"
#include "UploadBuffer.h"
#include "GenericShaderStructures.h"

namespace Speck
{
	struct SpeckInstanceData
	{
		DirectX::XMFLOAT3 Position;
		UINT MaterialIndex = 0;
	};

	struct RenderItemConstants
	{
		DirectX::XMFLOAT4X4 Transform = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 InvTransposeTransform = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

		UINT RenderItemType;
		UINT MaterialIndex;
		UINT2 gPadding;
		UINT4 Param[RENDER_ITEM_SPECIAL_PARAM_N];
	};

	struct PassConstants
	{
		DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
		DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
		float cbPerObjectPad1 = 0.0f;
		DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		float TotalTime = 0.0f;
		float DeltaTime = 0.0f;
	};

	struct MaterialBufferData
	{
		MaterialBufferData(Material *mat)
		{
			PBRMaterial *tMat = static_cast<PBRMaterial *>(mat);
			DirectX::XMMATRIX matTransform = XMLoadFloat4x4(&tMat->TexTransform);
			DiffuseAlbedo = tMat->DiffuseAlbedo;
			FresnelR0 = tMat->FresnelR0;
			Roughness = tMat->Roughness;
			XMStoreFloat4x4(&TexTransform, XMMatrixTranspose(matTransform));
		}

		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
		float Roughness = 64.0f;

		// Used in texture mapping.
		DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	};

	struct SSAOData
	{
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ProjTex;

		UINT SSAOMapsWidth;
		UINT SSAOMapsHeight;
		UINT TextureMapsWidth;
		UINT TextureMapsHeight;

		float OcclusionRadius;
		float OcclusionFadeStart;
		float OcclusionFadeEnd;
		float SurfaceEpsilon;

		DirectX::XMFLOAT4 OffsetVectors[SSAO_RANDOM_SAMPLES_N];
		DirectX::XMFLOAT4 BlurWeights[SSAO_BLUR_WEIGHTS_N];
	};

	// Stores the resources needed for the CPU to build the command lists
	// for a frame.  
	struct FrameResource
	{
	public:

		FrameResource(ID3D12Device* device, UINT passCount, UINT maxRenderItemsCount, UINT materialCount);
		FrameResource(const FrameResource& rhs) = delete;
		FrameResource& operator=(const FrameResource& rhs) = delete;
		~FrameResource();

		// We cannot reset the allocator until the GPU is done processing the commands.
		// So each frame needs their own allocator.
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

		// We cannot update a cbuffer until the GPU is done processing the commands
		// that reference it.  So each frame needs their own cbuffers.
		std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
		std::unique_ptr<UploadBuffer<SSAOData>> SSAODataBuffer = nullptr;
		std::unique_ptr<UploadBuffer<MaterialBufferData>> MaterialBuffer = nullptr;
		std::unique_ptr<UploadBuffer<RenderItemConstants>> RenderItemConstantsBuffer = nullptr;

		// Array of abstract buffers for general purpose.
		std::vector<std::unique_ptr<UploadBufferBase>> UploadBuffers;
		// Array of abstract buffers for general purpose.
		std::vector<ResourcePair> Buffers;

		// Fence value to mark commands up to this fence point.  This lets us
		// check if these frame resources are still in use by the GPU.
		UINT64 Fence = 0;
	};
}

#endif
