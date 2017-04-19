
#ifndef CUBE_RENDER_TARGET_H
#define CUBE_RENDER_TARGET_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"
#include "EngineUser.h"
#include "WorldUser.h"
#include "UploadBuffer.h"
#include "Camera.h"

namespace Speck
{
	struct PassConstants;

	struct CubeCameraSet
	{
		CubeCameraSet(EngineCore &ec);
		void SetPosition(const DirectX::XMFLOAT3 &pos);

		Camera mCubeMapCamera[6];
	};

	enum class CubeMapFace : int
	{
		PositiveX = 0,
		NegativeX = 1,
		PositiveY = 2,
		NegativeY = 3,
		PositiveZ = 4,
		NegativeZ = 5
	};

	class CubeRenderTarget : public EngineUser, public WorldUser
	{
	public:
		CubeRenderTarget(EngineCore &ec, World &world, UINT width, UINT height, const DirectX::XMFLOAT3 &pos, DXGI_FORMAT format);

		CubeRenderTarget(const CubeRenderTarget& rhs) = delete;
		CubeRenderTarget& operator=(const CubeRenderTarget& rhs) = delete;
		~CubeRenderTarget() = default;

		// Get texture buffer resource.
		ID3D12Resource* TexResource();
		// Get constant buffer resource.
		ID3D12Resource* CBResource();
		CD3DX12_GPU_DESCRIPTOR_HANDLE Srv();
		CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv(int faceIndex);
		CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv();

		D3D12_VIEWPORT Viewport()const;
		D3D12_RECT ScissorRect()const;
		// Get one of the six cube cameras.
		Camera *GetCamera(int index) { return &mCCS.mCubeMapCamera[index]; }

		void SetPosition(const DirectX::XMFLOAT3 &pos) { mCCS.SetPosition(pos); }
		void UpdateCBs();
		void UpdateIrradianceMap();
		static void BuildStaticMembers(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList);
		static void ReleaseStaticMembers();

	private:
		void BuildDescriptors();
		void BuildResource();
		void BuildPassConstantBuffer();

	private:
		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		// Width of the enviroment map (per side)
		UINT mWidth = 0;
		// Height of the enviroment map (per side)
		UINT mHeight = 0;
		// Width of the irradiance map (spherical)
		const UINT mIrrWidth = 256;
		// Height of the irradiance map (spherical)
		const UINT mIrrHeight = 128;

		DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		CubeCameraSet mCCS;

		Microsoft::WRL::ComPtr<ID3D12Resource> mCubeMapBuffer = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mCubeDepthStencilBuffer = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mCubeMapIrradianceBuffer = nullptr;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvSrvUavHeap = nullptr;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap = nullptr;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap = nullptr;

		// Constant buffers for each frame resource
		std::vector<std::unique_ptr<UploadBuffer<PassConstants>>> mPassCB;
		UINT mCurrPassCBIndex = 0;

		// Static members
		static Microsoft::WRL::ComPtr<ID3D12RootSignature> mIrrMapCSRootSignature;
		static Microsoft::WRL::ComPtr<ID3DBlob> mIrrMapCS;
		static Microsoft::WRL::ComPtr<ID3D12PipelineState> mIrrMapCSPSO;


		// List of sampler rays for irradiance map calculation (monte carlo approach).
		static Microsoft::WRL::ComPtr<ID3D12Resource> mSamplerRaysBuffer;
		// Residue from creating mSamplerRaysBuffer.
		static Microsoft::WRL::ComPtr<ID3D12Resource> mSamplerRaysUploadBuffer;
	};
}

#endif
