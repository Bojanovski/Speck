
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
		CameraController();
		virtual ~CameraController() {};

		virtual void Update(float dt) {};

	protected:
		virtual void UpdateCamera(float dt, Camera *mCam);

		// Set world camera position.
		void SetPosition(float x, float y, float z, Camera *mCam);
		void SetPosition(const DirectX::XMFLOAT3 &v, Camera *mCam);

		// Strafe/Walk the camera a distance d.
		void Strafe(float d, Camera *mCam);
		void Walk(float d, Camera *mCam);

		// Rotate the camera.
		void Pitch(float angle, Camera *mCam);
		void RotateY(float angle, Camera *mCam);
	};
}

#endif
