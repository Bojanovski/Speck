
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

	namespace AppCommands
	{
		struct LoadResourceCommand;
		struct CreateMaterialCommand;
		struct CreateGeometryCommand;
	}

	namespace WorldCommands
	{
		struct AddRenderItemCommand;
		struct AddSpecksCommand;
		struct CreatePSOGroupCommand;
	}

	class SpeckApp : public D3DApp
	{
		friend class SpeckWorld;

		friend struct AppCommands::LoadResourceCommand;
		friend struct AppCommands::CreateMaterialCommand;
		friend struct AppCommands::CreateGeometryCommand;
		friend struct WorldCommands::AddRenderItemCommand;
		friend struct WorldCommands::AddSpecksCommand;
		friend struct WorldCommands::CreatePSOGroupCommand;

	public:
		SpeckApp(HINSTANCE hInstance, UINT maxNumberOfMaterials, EngineCore &ec, World &w);
		SpeckApp(const SpeckApp& rhs) = delete;
		SpeckApp& operator=(const SpeckApp& rhs) = delete;
		~SpeckApp();

		virtual bool Initialize() override;
		ID3D12RootSignature *GetRootSignature() { return mRootSignature.Get(); }
		FrameResource *GetCurrentFrameResource() { return mCurrFrameResource; }
		ID3D12Resource *GetDefaultTexture(UINT index) const { return mDefaultTextures[index]; }
		DXGI_FORMAT GetDeferredRTFormat(int index) const { return mDeferredRTFormats[index]; }

		// All the possible root parameters for this app.
		// Perfomance TIP: Order from most frequent to least frequent.
		enum struct MainPassRootParameter
		{
			StaticConstantBuffer = 0,
			InstancesConstantBuffer,
			TexturesDescriptorTable,
			MaterialsRootDescriptor,
			RigidBodyRootDescriptor,
			PassRootDescriptor,
			Count // Number of elements in this enum
		};
		enum struct PostProcessRootParameter
		{
			SettingsRootConstant = 0,
			TexturesDescriptorTable,
			Count // Number of elements in this enum
		};

	private:
		virtual void OnResize()override;
		// Update the system based values
		virtual void Update(const Timer& gt)override;
		// Update that is dependant on the command list.
		virtual void PreDrawUpdate(const Timer& gt)override;
		// Opens and closes the command list for current frame resource.
		virtual void Draw(const Timer& gt)override;

		void UpdateMaterialBuffer(const Timer& gt);
		void BuildDefaultTextures();
		void BuildDefaultMaterials();
		void BuildRootSignatures();
		void BuildScreenGeometry();
		void BuildSpeckGeometry();
		void BuildRegularGeometry();
		void BuildPSOs();
		void BuildFrameResources();
		void BuildDeferredRenderTargetsAndBuffers();

	private:
		std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>		mGeometries;
		std::unordered_map<std::string, std::unique_ptr<Material>>			mMaterials;
		UINT mLatestMatCBIndex = 0;
		const UINT mMaxNumberOfMaterials;
		std::unordered_map<std::string, std::unique_ptr<Texture>>			mTextures;
		ID3D12Resource *mDefaultTextures[MATERIAL_TEXTURES_COUNT]; // for easy access based on index
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>	mShaders;

		std::vector<std::unique_ptr<FrameResource>> mFrameResources;
		FrameResource* mCurrFrameResource = nullptr;
		int mCurrFrameResourceIndex = 0;
		
		// To draw 3d object on MRT.
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

		// Deferred render targets buffers.
		Microsoft::WRL::ComPtr<ID3D12Resource> mDeferredRTBs[DEFERRED_RENDER_TARGETS_COUNT];
		DXGI_FORMAT mDeferredRTFormats[DEFERRED_RENDER_TARGETS_COUNT];
		DirectX::XMVECTORF32 mDeferredRTClearColors[DEFERRED_RENDER_TARGETS_COUNT];
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
