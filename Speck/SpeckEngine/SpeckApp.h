
#ifndef SPECK_APP_H
#define SPECK_APP_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"
#include "D3DApp.h"

namespace Speck
{
	struct MeshGeometry;
	struct Material;
	struct Texture;
	struct FrameResource;

	class SpeckApp : public D3DApp
	{
		friend class SpeckWorld;

	public:
		SpeckApp(HINSTANCE hInstance, UINT maxNumberOfMaterials, EngineCore &ec, World &w);
		SpeckApp(const SpeckApp& rhs) = delete;
		SpeckApp& operator=(const SpeckApp& rhs) = delete;
		~SpeckApp();

		virtual bool Initialize() override;
		ID3D12RootSignature *GetRootSignature() { return mRootSignature.Get(); }
		FrameResource *GetCurrentFrameResource() { return mCurrFrameResource; }
		ID3D12Resource *GetDefaultTexture(UINT index) const { return mDefaultTextures[index]; }

	private:
		virtual void OnResize()override;
		virtual void Update(const GameTimer& gt)override;
		virtual void Draw(const GameTimer& gt)override;

		void UpdateMaterialBuffer(const GameTimer& gt);
		void BuildDefaultTextures();
		void BuildRootSignatures();
		void BuildScreenGeometry();
		void BuildSpeckGeometry();
		void BuildRegularGeometry();
		void BuildPSOs();
		void BuildFrameResources();
		void BuildDeferredRenderTargetsAndBuffers();
		void BuildRenderItems();

	public:
		std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>		mGeometries;
		std::unordered_map<std::string, std::unique_ptr<Material>>			mMaterials;
		UINT mLatestMatCBIndex = 0;
		const UINT mMaxNumberOfMaterials;
		std::unordered_map<std::string, std::unique_ptr<Texture>>			mTextures;
		ID3D12Resource *mDefaultTextures[MATERIAL_TEXTURES_COUNT]; // for easy access based on index
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>	mShaders;

	private:
		std::vector<std::unique_ptr<FrameResource>> mFrameResources;
		FrameResource* mCurrFrameResource = nullptr;
		int mCurrFrameResourceIndex = 0;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

		// Deferred render targets buffers.
		Microsoft::WRL::ComPtr<ID3D12Resource> mDeferredRTBs[DEFERRED_RENDER_TARGETS_COUNT];
		Microsoft::WRL::ComPtr<ID3D12Resource> mDeferredDepthStencilBuffer = nullptr;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mDeferredRTVHeapHandle;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mDeferredDSVHeapHandle;
		D3D12_VIEWPORT mDeferredRTViewport;
		D3D12_RECT mDeferredRTScissorRect;
		UINT mSuperSampling = 2;
		// PSO used for screen objects like postprocess back buffer.
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mScreenObjPSO;
		// Signature and heaps for postprocess.
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mPostProcessRootSignature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mPostProcessSrvDescriptorHeap = nullptr;
	};
}

#endif
