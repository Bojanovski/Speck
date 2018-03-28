
#ifndef DIRECTX_CORE_H
#define DIRECTX_CORE_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"

namespace Speck
{
	//-------------------------------------------------------------------------------------
	//	This structure holds all DirectX related values. It is ment to be initialzed
	//	only once (in EngineCore class) and then passed to game components via pointer.
	//-------------------------------------------------------------------------------------

	class DirectXCore
	{
		friend class D3DApp;

	public:
		DirectXCore() = default;
		// Make these inaccessible.
		DirectXCore(const DirectXCore &v) = delete;
		DirectXCore &operator=(const DirectXCore &v) = delete;

		//
		// GETTERS
		//
		bool Get4xMsaaState() const { return m4xMsaaState; }
		UINT Get4xMsaaQuality() const { return m4xMsaaQuality; }

		IDXGIFactory4 *GetFactory() { return mdxgiFactory.Get(); }
		IDXGISwapChain *GetSwapChain() { return mSwapChain.Get(); }
		ID3D12Device *GetDevice() { return md3dDevice.Get(); }
		ID3D12Fence *GetFence() { return mFence.Get(); }
		const UINT64 &GetCurrentFence() const { return mCurrentFence; }

		ID3D12CommandQueue *GetCommandQueue() { return mCommandQueue.Get(); }
		ID3D12CommandAllocator *GetCommandAllocator() { return mDirectCmdListAlloc.Get(); }
		ID3D12GraphicsCommandList *GetCommandList() { return mCommandList.Get(); }
		void FlushCommandQueue();

		const D3D12_VIEWPORT &GetScreenViewport() const { return mScreenViewport; }
		const D3D12_RECT &GetScissorRect() const { return mScissorRect; }

		UINT GetRtvDescriptorSize() const { return mRtvDescriptorSize; }
		UINT GetDsvDescriptorSize() const { return mDsvDescriptorSize; }
		UINT GetCbvSrvUavDescriptorSize() const { return mCbvSrvUavDescriptorSize; }

		D3D12_CPU_DESCRIPTOR_HANDLE Get_CPU_RTV_HeapStart() { return mRtvHeap->GetCPUDescriptorHandleForHeapStart(); }
		D3D12_CPU_DESCRIPTOR_HANDLE Get_CPU_DSV_HeapStart() { return mDsvHeap->GetCPUDescriptorHandleForHeapStart(); }

		D3D_DRIVER_TYPE GetD3dDriverType() const { return md3dDriverType; }
		DXGI_FORMAT	GetBackBufferFormat() const { return mBackBufferFormat; }
		DXGI_FORMAT	GetDepthStencilFormat() const { return mDepthStencilFormat; }
		DirectX::XMVECTORF32 const &GetClearRTColor() const { return DirectX::Colors::CornflowerBlue; }
		void GetClearRTColor(void *destPt) const { memcpy(destPt, GetClearRTColor(), sizeof(GetClearRTColor())); }

		int GetClientHeight() const { return mClientHeight; }
		int GetClientWidth() const { return mClientWidth; }
		float GetAspectRatio() const { return static_cast<float>(mClientWidth) / mClientHeight; }

	private:
		// Set true to use 4X MSAA (§4.1.8).  The default is false.
		bool      m4xMsaaState = false;    // 4X MSAA enabled
		UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA

		Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
		Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
		Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;
		Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
		UINT64 mCurrentFence = 0;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

		int mCurrBackBuffer = 0;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SWAP_CHAIN_BUFFER_COUNT];
		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

		D3D12_VIEWPORT mScreenViewport;
		D3D12_RECT mScissorRect;

		UINT mRtvDescriptorSize = 0;
		UINT mDsvDescriptorSize = 0;
		UINT mCbvSrvUavDescriptorSize = 0;

		D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
		DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		int mClientWidth = 1980;
		int mClientHeight = 1080;

	};

	// RAII structure that enables grouping DirectX API calls in Graphics Debugger.
	struct GraphicsDebuggerAnnotator
	{
#if defined(_DEBUG) || defined(DEBUG)
		GraphicsDebuggerAnnotator(DirectXCore &dxCore, const std::string &groupLabel);
		~GraphicsDebuggerAnnotator();

	private:
		DirectXCore &mDXCore;
#else
		GraphicsDebuggerAnnotator(DirectXCore &dxCore, const std::string &groupLabel) {/*do nothing*/}
#endif
	};
}

#endif
