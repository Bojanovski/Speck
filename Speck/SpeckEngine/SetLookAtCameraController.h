
#ifndef SET_LOOK_AT_CAMERA_CONTROLLER_H
#define SET_LOOK_AT_CAMERA_CONTROLLER_H

#include "SpeckEngineDefinitions.h"
#include "CameraController.h"
#include "EngineUser.h"

namespace Speck
{
	class SetLookAtCameraController : public Speck::CameraController
	{
	public:
		SetLookAtCameraController(const DirectX::XMFLOAT3 &pos, const DirectX::XMFLOAT3 &target, const DirectX::XMFLOAT3 &upDir)
			: Speck::CameraController(), mPos(pos), mTarget(target), mUpDir(upDir) {	}
		~SetLookAtCameraController() {	}

	protected:
		void UpdateCamera(float dt, Camera *cam) override { cam->LookAt(mPos, mTarget, mUpDir); }

	private:
		const DirectX::XMFLOAT3 mPos;
		const DirectX::XMFLOAT3 mTarget;
		const DirectX::XMFLOAT3 mUpDir;
	};
}

#endif
