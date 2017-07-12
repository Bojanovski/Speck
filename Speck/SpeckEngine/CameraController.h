
#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include "SpeckEngineDefinitions.h"
#include "Camera.h"

namespace Speck
{
	// Visitor design pattern used to update a camera object.
	class CameraController
	{
		friend class Camera;

	public:
		DLL_EXPORT CameraController();
		virtual ~CameraController() {};

		virtual void Update(float dt) {};

	protected:
		DLL_EXPORT virtual void UpdateCamera(float dt, Camera *mCam);

		// Set world camera position.
		DLL_EXPORT void SetPosition(float x, float y, float z, Camera *mCam);
		DLL_EXPORT void SetPosition(const DirectX::XMFLOAT3 &v, Camera *mCam);

		// Strafe/Walk the camera a distance d.
		DLL_EXPORT void Strafe(float d, Camera *mCam);
		DLL_EXPORT void Walk(float d, Camera *mCam);

		// Rotate the camera.
		DLL_EXPORT void Pitch(float angle, Camera *mCam);
		DLL_EXPORT void RotateY(float angle, Camera *mCam);
	};
}

#endif
