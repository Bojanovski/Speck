
#include "EngineCore.h"
#include "Camera.h"
#include "InputHandler.h"
#include "GameTimer.h"
#include "ProcessAndSystemData.h"
#include "DirectXCore.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

EngineCore::EngineCore()
{ 
	mDefaultCamera			= make_unique<Camera>(*this);
	mProcessAndSystemData	= make_unique<ProcessAndSystemData>();
	mTimer					= make_unique<GameTimer>();
	mInputHandler			= make_unique<InputHandler>();
	mDirectXCore			= make_unique<DirectXCore>();

	mCamera = mDefaultCamera.get();
}

EngineCore::~EngineCore()
{

}

int EngineCore::GetClientHeight() const
{
	return mDirectXCore->GetClientHeight();
}

int EngineCore::GetClientWidth() const
{
	return mDirectXCore->GetClientWidth();
}

float EngineCore::GetAspectRatio() const
{
	return mDirectXCore->GetAspectRatio();
}
