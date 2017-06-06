
#include "DirectXCore.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

void DirectXCore::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	mCurrentFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has completed commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

#if defined(_DEBUG) || defined(DEBUG)
GraphicsDebuggerAnnotator::GraphicsDebuggerAnnotator(DirectXCore &dxCore, const std::string &groupLabel)
	: mDXCore(dxCore)
{
	// Warning: This is a support method used internally by the PIX event runtime.It is not intended to be called directly.

	wstring ws(groupLabel.begin(), groupLabel.end());
	ws.push_back(0);
	mDXCore.GetCommandList()->BeginEvent(0, &ws[0], (UINT)ws.length()*sizeof(wchar_t));
}

GraphicsDebuggerAnnotator::~GraphicsDebuggerAnnotator()
{
	mDXCore.GetCommandList()->EndEvent();
}
#endif