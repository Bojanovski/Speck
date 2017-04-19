
#ifndef FP_CAMERA_CONTROLLER_H
#define FP_CAMERA_CONTROLLER_H

#include "SpeckEngineDefinitions.h"
#include "CameraController.h"
#include "EngineUser.h"

namespace Speck
{
	class FPCameraController : public CameraController, public EngineUser
	{
	public:
		DLL_EXPORT FPCameraController(EngineCore &engineCore);
		DLL_EXPORT ~FPCameraController();

		void Update(float dt) override;

	protected:
		void UpdateCamera(float dt, Camera *cam) override;

	private:
		float mSpeed;
		int mInitialWheelValue;
	};
}

#endif
