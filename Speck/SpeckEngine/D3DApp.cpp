//***************************************************************************************
// D3dApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "D3DApp.h"
#include "DirectXHeaders.h"
#include "DirectXCore.h"
#include "ProcessAndSystemData.h"
#include "CameraController.h"
#include "GameTimer.h"
#include "InputHandler.h"
#include <psapi.h> // for system data
#include <pdh.h>
#pragma comment(lib, "pdh.lib")

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

static PDH_HQUERY cpuQuery;
static PDH_HCOUNTER cpuTotal;
static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
static int numProcessors;
static HANDLE self;

	LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		// Forward hwnd on because we can get messages (e.g., WM_CREATE)
		// before CreateWindow returns, and thus before mhMainWnd is valid.
		return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
	}

	D3DApp* D3DApp::mApp = nullptr;
	D3DApp* D3DApp::GetApp()
	{
		return mApp;
	}

	D3DApp::D3DApp(HINSTANCE hInstance, EngineCore &ec, World &w)
		: App(ec, w),
		mhAppInst(hInstance)
	{
		// Only one D3DApp can be constructed.
		assert(mApp == nullptr);
		mApp = this;

		// Members
		GetEngineCore().mDefaultCamera->LookAt(XMFLOAT3(0.0f, 2.0f, -15.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
		CameraController defaultCC;
		GetEngineCore().mDefaultCamera->Update(0.0f, &defaultCC);

		// Process and system data
		PdhOpenQuery(NULL, NULL, &cpuQuery);
		PdhAddCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
		PdhCollectQueryData(cpuQuery);
		SYSTEM_INFO sysInfo;
		FILETIME ftime, fsys, fuser;
		GetSystemInfo(&sysInfo);
		numProcessors = sysInfo.dwNumberOfProcessors;
		GetSystemTimeAsFileTime(&ftime);
		memcpy(&lastCPU, &ftime, sizeof(FILETIME));
		self = GetCurrentProcess();
		GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
		memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
		memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
		MEMORYSTATUSEX memInfo;
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memInfo);
		GetEngineCore().mProcessAndSystemData->mTotalVirtualMem = memInfo.ullTotalPageFile;
		GetEngineCore().mProcessAndSystemData->mVirtualMemUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;
		PROCESS_MEMORY_COUNTERS_EX pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
		GetEngineCore().mProcessAndSystemData->mVirtualMemUsedByMe = pmc.PrivateUsage;
		GetEngineCore().mProcessAndSystemData->mTotalPhysMem = memInfo.ullTotalPhys;
		GetEngineCore().mProcessAndSystemData->mPhysMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
		GetEngineCore().mProcessAndSystemData->mPhysMemUsedByMe = pmc.WorkingSetSize;
	}

	D3DApp::~D3DApp()
	{
		D3DApp::mApp = nullptr;
		if (GetEngineCore().GetDirectXCore().md3dDevice != nullptr)
			GetEngineCore().GetDirectXCore().FlushCommandQueue();
	}

	HINSTANCE D3DApp::AppInst()const
	{
		return mhAppInst;
	}

	HWND D3DApp::MainWnd()const
	{
		return mhMainWnd;
	}

	float D3DApp::AspectRatio()const
	{
		return static_cast<float>(GetEngineCore().mDirectXCore->mClientWidth) / GetEngineCore().mDirectXCore->mClientHeight;
	}

	void D3DApp::ChangeMainWindowCaption(const wstring & newCaption)
	{
		SetWindowText(mhMainWnd, newCaption.c_str());
	}

	int D3DApp::Run(std::unique_ptr<AppState> initialState)
	{
		// Run one pre-update to set everything up.
		GetEngineCore().mTimer->Reset();
		UpdateProcessAndSystemData();
		Update(*GetEngineCore().mTimer.get());

		// Now actually run the application
		App::Run(move(initialState));
		MSG msg = { 0 };

		while (msg.message != WM_QUIT)
		{
			// If there are Window messages then process them.
			if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				GetEngineCore().mInputHandler->Win32MsgProc(&msg);
			}
			// Otherwise, do animation/game stuff.
			else
			{
				GetEngineCore().mTimer->Tick();

				if (!mAppPaused)
				{
					UpdateProcessAndSystemData();
					Update(*GetEngineCore().mTimer.get());
					Draw(*GetEngineCore().mTimer.get());
				}
				else
				{
					Sleep(100);
				}
			}
		}

		return (int)msg.wParam;
	}

	bool D3DApp::Initialize()
	{
		if (!App::Initialize())
			return false;

		if (!InitMainWindow())
			return false;

		if (!InitDirect3D())
			return false;

		// Do the initial resize code.
		OnResize();

		return true;
	}

	void D3DApp::CreateRtvAndDsvDescriptorHeaps()
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		rtvHeapDesc.NumDescriptors = SWAP_CHAIN_BUFFER_COUNT + DEFERRED_RENDER_TARGETS_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;
		ThrowIfFailed(GetEngineCore().mDirectXCore->md3dDevice->CreateDescriptorHeap(
			&rtvHeapDesc, IID_PPV_ARGS(GetEngineCore().mDirectXCore->mRtvHeap.GetAddressOf())));

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
		dsvHeapDesc.NumDescriptors = 1 + 1; // one additional for deferred render targets (can have a different size than the one on the backbuffer)
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 0;
		ThrowIfFailed(GetEngineCore().mDirectXCore->md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(GetEngineCore().mDirectXCore->mDsvHeap.GetAddressOf())));
	}

	void D3DApp::OnResize()
	{
		assert(GetEngineCore().mDirectXCore->md3dDevice);
		assert(GetEngineCore().mDirectXCore->mSwapChain);
		assert(GetEngineCore().mDirectXCore->mDirectCmdListAlloc);

		// Flush before changing any resources.
		GetEngineCore().GetDirectXCore().FlushCommandQueue();
		ThrowIfFailed(GetEngineCore().mDirectXCore->mCommandList->Reset(GetEngineCore().mDirectXCore->mDirectCmdListAlloc.Get(), nullptr));

		// Release the previous resources we will be recreating.
		for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
			GetEngineCore().mDirectXCore->mSwapChainBuffer[i].Reset();
		GetEngineCore().mDirectXCore->mDepthStencilBuffer.Reset();

		// Resize the swap chain.
		ThrowIfFailed(GetEngineCore().mDirectXCore->mSwapChain->ResizeBuffers(
			SWAP_CHAIN_BUFFER_COUNT,
			GetEngineCore().mDirectXCore->mClientWidth, GetEngineCore().mDirectXCore->mClientHeight,
			GetEngineCore().mDirectXCore->mBackBufferFormat,
			DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

		GetEngineCore().mDirectXCore->mCurrBackBuffer = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(GetEngineCore().mDirectXCore->mRtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
		{
			ThrowIfFailed(GetEngineCore().mDirectXCore->mSwapChain->GetBuffer(i, IID_PPV_ARGS(&GetEngineCore().mDirectXCore->mSwapChainBuffer[i])));
			GetEngineCore().mDirectXCore->md3dDevice->CreateRenderTargetView(GetEngineCore().mDirectXCore->mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
			rtvHeapHandle.Offset(1, GetEngineCore().mDirectXCore->mRtvDescriptorSize);
		}

		// Create the depth/stencil buffer and view.
		D3D12_RESOURCE_DESC depthStencilDesc;
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = GetEngineCore().mDirectXCore->mClientWidth;
		depthStencilDesc.Height = GetEngineCore().mDirectXCore->mClientHeight;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;

		// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
		// the depth buffer.  Therefore, because we need to create two views to the same resource:
		//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
		//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
		// we need to create the depth buffer resource with a typeless format.  
		depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

		depthStencilDesc.SampleDesc.Count = GetEngineCore().mDirectXCore->m4xMsaaState ? 4 : 1;
		depthStencilDesc.SampleDesc.Quality = GetEngineCore().mDirectXCore->m4xMsaaState ? (GetEngineCore().mDirectXCore->m4xMsaaQuality - 1) : 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = GetEngineCore().mDirectXCore->mDepthStencilFormat;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;
		ThrowIfFailed(GetEngineCore().mDirectXCore->md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(GetEngineCore().mDirectXCore->mDepthStencilBuffer.GetAddressOf())));

		// Create descriptor to mip level 0 of entire resource using the format of the resource.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = GetEngineCore().mDirectXCore->mDepthStencilFormat;
		dsvDesc.Texture2D.MipSlice = 0;
		GetEngineCore().mDirectXCore->md3dDevice->CreateDepthStencilView(GetEngineCore().mDirectXCore->mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

		// Transition the resource from its initial state to be used as a depth buffer.
		GetEngineCore().mDirectXCore->mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetEngineCore().mDirectXCore->mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		// Execute the resize commands.
		ThrowIfFailed(GetEngineCore().mDirectXCore->mCommandList->Close());
		ID3D12CommandList* cmdsLists[] = { GetEngineCore().mDirectXCore->mCommandList.Get() };
		GetEngineCore().mDirectXCore->mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		// Wait until resize is complete.
		GetEngineCore().GetDirectXCore().FlushCommandQueue();

		// Update the viewport transform to cover the client area.
		GetEngineCore().mDirectXCore->mScreenViewport.TopLeftX = 0;
		GetEngineCore().mDirectXCore->mScreenViewport.TopLeftY = 0;
		GetEngineCore().mDirectXCore->mScreenViewport.Width = static_cast<float>(GetEngineCore().mDirectXCore->mClientWidth);
		GetEngineCore().mDirectXCore->mScreenViewport.Height = static_cast<float>(GetEngineCore().mDirectXCore->mClientHeight);
		GetEngineCore().mDirectXCore->mScreenViewport.MinDepth = 0.0f;
		GetEngineCore().mDirectXCore->mScreenViewport.MaxDepth = 1.0f;

		GetEngineCore().mDirectXCore->mScissorRect = { 0, 0, GetEngineCore().mDirectXCore->mClientWidth, GetEngineCore().mDirectXCore->mClientHeight };

		// Update members
		GetEngineCore().mDefaultCamera->SetLens(0.25f*MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);
		mAppStatesManager.OnResize();
	}

	void D3DApp::Update(const GameTimer &gt)
	{
		GetEngineCore().GetCamera().Update(gt.DeltaTime(), &GetEngineCore().GetCameraController());
		GetEngineCore().mInputHandler->Update(gt.DeltaTime());
		mAppStatesManager.Update(gt.DeltaTime());
	}

	void D3DApp::Draw(const GameTimer & gt)
	{
		mAppStatesManager.Draw(gt.DeltaTime());
	}

	LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
			// WM_ACTIVATE is sent when the window is activated or deactivated.  
			// We pause the game when the window is deactivated and unpause it 
			// when it becomes active.  
			case WM_ACTIVATE:
				if (LOWORD(wParam) == WA_INACTIVE)
				{
					mAppPaused = true;
					GetEngineCore().mTimer->Stop();
				}
				else
				{
					mAppPaused = false;
					GetEngineCore().mTimer->Start();
				}
				return 0;

				// WM_SIZE is sent when the user resizes the window.  
			case WM_SIZE:
				// Save the new client area dimensions.
				GetEngineCore().mDirectXCore->mClientWidth = LOWORD(lParam);
				GetEngineCore().mDirectXCore->mClientHeight = HIWORD(lParam);
				if (GetEngineCore().mDirectXCore->md3dDevice)
				{
					if (wParam == SIZE_MINIMIZED)
					{
						mAppPaused = true;
						mMinimized = true;
						mMaximized = false;
					}
					else if (wParam == SIZE_MAXIMIZED)
					{
						mAppPaused = false;
						mMinimized = false;
						mMaximized = true;
						OnResize();
					}
					else if (wParam == SIZE_RESTORED)
					{

						// Restoring from minimized state?
						if (mMinimized)
						{
							mAppPaused = false;
							mMinimized = false;
							OnResize();
						}

						// Restoring from maximized state?
						else if (mMaximized)
						{
							mAppPaused = false;
							mMaximized = false;
							OnResize();
						}
						else if (mResizing)
						{
							// If user is dragging the resize bars, we do not resize 
							// the buffers here because as the user continuously 
							// drags the resize bars, a stream of WM_SIZE messages are
							// sent to the window, and it would be pointless (and slow)
							// to resize for each WM_SIZE message received from dragging
							// the resize bars.  So instead, we reset after the user is 
							// done resizing the window and releases the resize bars, which 
							// sends a WM_EXITSIZEMOVE message.
						}
						else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
						{
							OnResize();
						}
					}
				}
				return 0;

				// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
			case WM_ENTERSIZEMOVE:
				mAppPaused = true;
				mResizing = true;
				GetEngineCore().mTimer->Stop();
				return 0;

				// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
				// Here we reset everything based on the new window dimensions.
			case WM_EXITSIZEMOVE:
				mAppPaused = false;
				mResizing = false;
				GetEngineCore().mTimer->Start();
				OnResize();
				return 0;

				// WM_DESTROY is sent when the window is being destroyed.
			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;

				// The WM_MENUCHAR message is sent when a menu is active and the user presses 
				// a key that does not correspond to any mnemonic or accelerator key. 
			case WM_MENUCHAR:
				// Don't beep when we alt-enter.
				return MAKELRESULT(0, MNC_CLOSE);

				// Catch this message so to prevent the window from becoming too small.
			case WM_GETMINMAXINFO:
				((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
				((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
				return 0;

			case WM_LBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_MBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MOUSEMOVE:
			case WM_KEYUP:
				if (wParam == VK_ESCAPE)
				{
					PostQuitMessage(0);
				}
				else if ((int)wParam == VK_F2)
				{
					GetEngineCore().mDirectXCore->m4xMsaaState = !GetEngineCore().mDirectXCore->m4xMsaaState;
					// Recreate the swapchain and buffers with new multisample settings.
					CreateSwapChain();
					OnResize();
				}

				return 0;
		}

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	bool D3DApp::InitMainWindow()
	{
		WNDCLASS wc;
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = MainWndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = mhAppInst;
		wc.hIcon = LoadIcon(0, IDI_APPLICATION);
		wc.hCursor = LoadCursor(0, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
		wc.lpszMenuName = 0;
		wc.lpszClassName = L"MainWnd";

		if (!RegisterClass(&wc))
		{
			MessageBox(0, L"RegisterClass Failed.", 0, 0);
			return false;
		}

		// Compute window rectangle dimensions based on requested client area dimensions.
		RECT R = { 0, 0, GetEngineCore().mDirectXCore->mClientWidth, GetEngineCore().mDirectXCore->mClientHeight };
		AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
		int width = R.right - R.left;
		int height = R.bottom - R.top;

		mhMainWnd = CreateWindow(L"MainWnd", L"Window Title",
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
		if (!mhMainWnd)
		{
			MessageBox(0, L"CreateWindow Failed.", 0, 0);
			return false;
		}

		ShowWindow(mhMainWnd, SW_SHOW);
		UpdateWindow(mhMainWnd);
		GetEngineCore().mInputHandler->Initialize(mhMainWnd);
		return true;
	}

	bool D3DApp::InitDirect3D()
	{
#if defined(DEBUG) || defined(_DEBUG) 
		// Enable the D3D12 debug layer.
		{
			ComPtr<ID3D12Debug> debugController;
			ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
			debugController->EnableDebugLayer();
		}
#endif

		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&GetEngineCore().mDirectXCore->mdxgiFactory)));

		// Try to create hardware device.
		HRESULT hardwareResult = D3D12CreateDevice(
			nullptr,             // default adapter
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&GetEngineCore().mDirectXCore->md3dDevice));

		// Fallback to WARP device.
		if (FAILED(hardwareResult))
		{
			ComPtr<IDXGIAdapter> pWarpAdapter;
			ThrowIfFailed(GetEngineCore().mDirectXCore->mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

			ThrowIfFailed(D3D12CreateDevice(
				pWarpAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&GetEngineCore().mDirectXCore->md3dDevice)));
		}

		ThrowIfFailed(GetEngineCore().mDirectXCore->md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&GetEngineCore().mDirectXCore->mFence)));


		// Get the increment size of a descriptor in specific heap type.  This is hardware specific, so we have to query this information.
		GetEngineCore().mDirectXCore->mRtvDescriptorSize = GetEngineCore().mDirectXCore->md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		GetEngineCore().mDirectXCore->mDsvDescriptorSize = GetEngineCore().mDirectXCore->md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		GetEngineCore().mDirectXCore->mCbvSrvUavDescriptorSize = GetEngineCore().mDirectXCore->md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// Check 4X MSAA quality support for our back buffer format.
		// All Direct3D 11 capable devices support 4X MSAA for all render 
		// target formats, so we only need to check quality support.

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
		msQualityLevels.Format = GetEngineCore().mDirectXCore->mBackBufferFormat;
		msQualityLevels.SampleCount = 4;
		msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		msQualityLevels.NumQualityLevels = 0;
		ThrowIfFailed(GetEngineCore().mDirectXCore->md3dDevice->CheckFeatureSupport(
			D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
			&msQualityLevels,
			sizeof(msQualityLevels)));

		GetEngineCore().mDirectXCore->m4xMsaaQuality = msQualityLevels.NumQualityLevels;
		assert(GetEngineCore().mDirectXCore->m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
		LogAdapters();
#endif

		CreateCommandObjects();
		CreateSwapChain();
		CreateRtvAndDsvDescriptorHeaps();

		return true;
	}

	void D3DApp::CreateCommandObjects()
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		ThrowIfFailed(GetEngineCore().mDirectXCore->md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&GetEngineCore().mDirectXCore->mCommandQueue)));

		ThrowIfFailed(GetEngineCore().mDirectXCore->md3dDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(GetEngineCore().mDirectXCore->mDirectCmdListAlloc.GetAddressOf())));

		ThrowIfFailed(GetEngineCore().mDirectXCore->md3dDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			GetEngineCore().mDirectXCore->mDirectCmdListAlloc.Get(),		// Associated command allocator
			nullptr,							// Initial PipelineStateObject
			IID_PPV_ARGS(GetEngineCore().mDirectXCore->mCommandList.GetAddressOf())));

		// Start off in a closed state.  This is because the first time we refer 
		// to the command list we will Reset it, and it needs to be closed before
		// calling Reset.
		GetEngineCore().mDirectXCore->mCommandList->Close();
	}

	void D3DApp::CreateSwapChain()
	{
		// Release the previous swapchain we will be recreating.
		GetEngineCore().mDirectXCore->mSwapChain.Reset();

		DXGI_SWAP_CHAIN_DESC sd;
		sd.BufferDesc.Width = GetEngineCore().mDirectXCore->mClientWidth;
		sd.BufferDesc.Height = GetEngineCore().mDirectXCore->mClientHeight;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferDesc.Format = GetEngineCore().mDirectXCore->mBackBufferFormat;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		sd.SampleDesc.Count = GetEngineCore().mDirectXCore->m4xMsaaState ? 4 : 1;
		sd.SampleDesc.Quality = GetEngineCore().mDirectXCore->m4xMsaaState ? (GetEngineCore().mDirectXCore->m4xMsaaQuality - 1) : 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
		sd.OutputWindow = mhMainWnd;
		sd.Windowed = true;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		// Note: Swap chain uses queue to perform flush.
		ThrowIfFailed(GetEngineCore().mDirectXCore->mdxgiFactory->CreateSwapChain(
			GetEngineCore().mDirectXCore->mCommandQueue.Get(),
			&sd,
			GetEngineCore().mDirectXCore->mSwapChain.GetAddressOf()));
	}

	ID3D12Resource* D3DApp::CurrentBackBuffer()const
	{
		return GetEngineCore().mDirectXCore->mSwapChainBuffer[GetEngineCore().mDirectXCore->mCurrBackBuffer].Get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView()const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(
			GetEngineCore().mDirectXCore->mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
			GetEngineCore().mDirectXCore->mCurrBackBuffer,
			GetEngineCore().mDirectXCore->mRtvDescriptorSize);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView()const
	{
		return GetEngineCore().mDirectXCore->mDsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	UINT64 D3DApp::IncrementCurrentFence()
	{
		return ++GetEngineCore().GetDirectXCore().mCurrentFence;
	}

	int D3DApp::UpdateBackBuffer()
	{ 
		return (GetEngineCore().mDirectXCore->mCurrBackBuffer = (GetEngineCore().mDirectXCore->mCurrBackBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT);
	}

	void D3DApp::UpdateProcessAndSystemData()
	{
		static int frameCnt = 0;
		static float timeElapsed = 0.0f;
		frameCnt++;

		// Compute averages over one second period.
		if ((GetEngineCore().mTimer->TotalTime() - timeElapsed) >= 1.0f)
		{
			// Code computes the average frames per second, and also the 
			// average time it takes to render one frame.  These stats 
			// are appended to the window caption bar.
			GetEngineCore().mProcessAndSystemData->mFPS = (float)frameCnt; // fps = frameCnt / 1
			GetEngineCore().mProcessAndSystemData->m_mSPF = 1000.0f / GetEngineCore().mProcessAndSystemData->mFPS;

			// Reset for next average.
			frameCnt = 0;
			timeElapsed += 1.0f;
		}

		// Process and system data
		MEMORYSTATUSEX memInfo;
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memInfo);
		//mProcessAndSystemData.mTotalVirtualMem = memInfo.ullTotalPageFile;
		GetEngineCore().mProcessAndSystemData->mVirtualMemUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;
		PROCESS_MEMORY_COUNTERS_EX pmc;
		pmc.cb = sizeof(pmc);
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, pmc.cb);
		GetEngineCore().mProcessAndSystemData->mVirtualMemUsedByMe = pmc.PrivateUsage;
		//mProcessAndSystemData.mTotalPhysMem = memInfo.ullTotalPhys;
		GetEngineCore().mProcessAndSystemData->mPhysMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
		GetEngineCore().mProcessAndSystemData->mPhysMemUsedByMe = pmc.WorkingSetSize;
		PDH_FMT_COUNTERVALUE counterVal;
		PdhCollectQueryData(cpuQuery);
		PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
		GetEngineCore().mProcessAndSystemData->mCPU_usage = counterVal.doubleValue;
		FILETIME ftime, fsys, fuser;
		ULARGE_INTEGER now, sys, user;
		double percent;
		GetSystemTimeAsFileTime(&ftime);
		memcpy(&now, &ftime, sizeof(FILETIME));
		GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
		memcpy(&sys, &fsys, sizeof(FILETIME));
		memcpy(&user, &fuser, sizeof(FILETIME));
		percent = (double)((sys.QuadPart - lastSysCPU.QuadPart) + (user.QuadPart - lastUserCPU.QuadPart));
		percent /= (now.QuadPart - lastCPU.QuadPart);
		percent /= numProcessors;
		lastCPU = now;
		lastUserCPU = user;
		lastSysCPU = sys;
		GetEngineCore().mProcessAndSystemData->mCPU_usageByMe = percent * 100;
	}

	void D3DApp::LogAdapters()
	{
		UINT i = 0;
		IDXGIAdapter* adapter = nullptr;
		std::vector<IDXGIAdapter*> adapterList;
		while (GetEngineCore().mDirectXCore->mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC desc;
			adapter->GetDesc(&desc);

			std::wstring text = L"***Adapter: ";
			text += desc.Description;
			text += L"\n";

			OutputDebugString(text.c_str());

			adapterList.push_back(adapter);

			++i;
		}

		for (size_t i = 0; i < adapterList.size(); ++i)
		{
			LogAdapterOutputs(adapterList[i]);
			ReleaseCom(adapterList[i]);
		}
	}

	void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter)
	{
		UINT i = 0;
		IDXGIOutput* output = nullptr;
		while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_OUTPUT_DESC desc;
			output->GetDesc(&desc);

			std::wstring text = L"***Output: ";
			text += desc.DeviceName;
			text += L"\n";
			OutputDebugString(text.c_str());

			LogOutputDisplayModes(output, GetEngineCore().mDirectXCore->mBackBufferFormat);

			ReleaseCom(output);

			++i;
		}
	}

	void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
	{
		UINT count = 0;
		UINT flags = 0;

		// Call with nullptr to get list count.
		output->GetDisplayModeList(format, flags, &count, nullptr);

		std::vector<DXGI_MODE_DESC> modeList(count);
		output->GetDisplayModeList(format, flags, &count, &modeList[0]);

		for (auto& x : modeList)
		{
			UINT n = x.RefreshRate.Numerator;
			UINT d = x.RefreshRate.Denominator;
			std::wstring text =
				L"Width = " + std::to_wstring(x.Width) + L" " +
				L"Height = " + std::to_wstring(x.Height) + L" " +
				L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
				L"\n";

			::OutputDebugString(text.c_str());
		}
	}
