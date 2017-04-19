
#include "FPCameraController.h"
#include "EngineCore.h"
#include "InputHandler.h"

using namespace std;
using namespace DirectX;
using namespace Speck;

FPCameraController::FPCameraController(EngineCore &engineCore)
	: CameraController(),
	EngineUser(engineCore),
	mSpeed(1.0f)
{
	MouseState curr, prev;
	GetEngineCore().GetInputHandler()->GetMouseStates(&curr, &prev);
	mInitialWheelValue = curr.wheelValue;
}

FPCameraController::~FPCameraController()
{
}

void Speck::FPCameraController::Update(float dt)
{
	// Mouse wheel
	auto input = GetEngineCore().GetInputHandler();
	MouseState curr, prev;
	input->GetMouseStates(&curr, &prev);
	mSpeed = pow(1.5f, curr.wheelValue - mInitialWheelValue);
	if (mSpeed < 0) mSpeed = 0;
}

void FPCameraController::UpdateCamera(float dt, Camera *cam)
{
	auto input = GetEngineCore().GetInputHandler();
	// KEYBOARD
	float pressedTime;

	// forward
	if (input->IsPressed(0x57, &pressedTime))
	{
		Walk(mSpeed * dt, cam);
	}

	// backward
	if (input->IsPressed(0x53, &pressedTime))
	{
		Walk(-mSpeed * dt, cam);
	}

	// left
	if (input->IsPressed(0x41, &pressedTime))
	{
		Strafe(-mSpeed * dt, cam);
	}

	// right
	if (input->IsPressed(0x44, &pressedTime))
	{
		Strafe(mSpeed * dt, cam);
	}

	//MOUSE
	MouseState curr, prev;
	input->GetMouseStates(&curr, &prev);
	if (curr.IsRMBDown)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(curr.x - prev.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(curr.y - prev.y));

		Pitch(dy, cam);
		RotateY(dx, cam);
	}

	// Finalize in base call
	CameraController::UpdateCamera(dt, cam);
}
