
#ifndef D3DAPP_H
#define D3DAPP_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"
#include "EngineCore.h"
#include "App.h"

namespace Speck
{
	class D3DApp : public App
	{
	protected:
		D3DApp(HINSTANCE hInstance, EngineCore &ec, World &w);
		D3DApp(const D3DApp& rhs) = delete;
		D3DApp& operator=(const D3DApp& rhs) = delete;
		virtual ~D3DApp();

	public:
		static D3DApp* GetApp();

		HINSTANCE			AppInst()const;
		HWND				MainWnd()const;
		float				AspectRatio()const;
		void				ChangeMainWindowCaption(const std::wstring &newCaption);

		virtual int Run(std::unique_ptr<AppState> initialState) override;
		virtual bool Initialize() override;
		virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	protected:
		virtual void CreateRtvAndDsvDescriptorHeaps();
		virtual void OnResize();
		// Update the system based values
		virtual void Update(const Timer& gt) = 0;
		// Update that is dependant on the command list.
		virtual void PreDrawUpdate(const Timer& gt) = 0;
		// Opens and closes the command list for current frame resource.
		virtual void Draw(const Timer& gt) = 0;

		virtual int GetRTVDescriptorCount() = 0;
		virtual int GetDSVDescriptorCount() = 0;

	protected:
		bool InitMainWindow();
		bool InitDirect3D();
		void CreateCommandObjects();
		void CreateSwapChain();

		ID3D12Resource* CurrentBackBuffer()const;
		D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
		D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;
		UINT64 IncrementCurrentFence();
		int UpdateBackBuffer();
		void UpdateProcessAndSystemData();

		void LogAdapters();
		void LogAdapterOutputs(IDXGIAdapter* adapter);
		void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	private:
		static D3DApp* mApp;

		HINSTANCE	mhAppInst = nullptr;			// application instance handle
		HWND		mhMainWnd = nullptr;			// main window handle
		bool		mAppPaused = false;				// is the application paused?
		bool		mMinimized = false;				// is the application minimized?
		bool		mMaximized = false;				// is the application maximized?
		bool		mResizing = false;				// are the resize bars being dragged?
		bool		mFullscreenState = false;		// fullscreen enabled
	};
}

#endif
