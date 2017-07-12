
#ifndef CAMERA_H
#define CAMERA_H
#define CAMERA_ZN 0.1f
#define CAMERA_ZF 1000.0f

#include "SpeckEngineDefinitions.h"
#include "EngineUser.h"

namespace Speck
{
	class Camera : public EngineUser
	{
		friend class CameraController;
		friend struct CubeCameraSet;

	public:
		DLL_EXPORT Camera(EngineCore &engineCore);
		DLL_EXPORT ~Camera();

		DLL_EXPORT void Update(float dt, CameraController *cc);

		// Get world camera position.
		DirectX::XMVECTOR GetPositionVector() const { return XMLoadFloat3(&mPosition); }
		DirectX::XMFLOAT3 GetPosition() const { return mPosition; }

		//Get camera basis vectors.
		DirectX::XMVECTOR GetRightVector() const { return XMLoadFloat3(&mRight); }
		DirectX::XMFLOAT3 GetRightFloat3() const { return mRight; }
		DirectX::XMVECTOR GetUpVector() const { return XMLoadFloat3(&mUp); }
		DirectX::XMFLOAT3 GetUpFloat3() const { return mUp; }
		DirectX::XMVECTOR GetLookVector() const{ return XMLoadFloat3(&mLook); }
		DirectX::XMFLOAT3 GetLookFloat3() const { return mLook; }

		// Get frustum properties.
		float GetNearZ() const { return mNearZ; }
		float GetFarZ() const { return mFarZ; }
		float GetFovY() const { return mFovY; }
		DLL_EXPORT float GetFovX() const;

		// Get near and far plane dimensions in view space coordinates.
		DLL_EXPORT float GetNearWindowWidth() const;
		float GetNearWindowHeight() const { return mNearWindowHeight; }
		DLL_EXPORT float GetFarWindowWidth() const;
		float GetFarWindowHeight() const { return mFarWindowHeight; }

		// Get View / Proj matrices.
		DirectX::XMMATRIX GetView() const { return XMLoadFloat4x4(&mView); }
		DirectX::XMMATRIX GetProj() const { return XMLoadFloat4x4(&mProj); }
		DirectX::XMMATRIX GetViewProj() const { return XMLoadFloat4x4(&mView) * XMLoadFloat4x4(&mProj); }

		// Set frustum.
		DLL_EXPORT void SetLens(float fovY, float aspect, float zn, float zf);

		// Define camera space via LookAt parameters.
		DLL_EXPORT void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
		DLL_EXPORT void LookAt(const DirectX::XMFLOAT3 &pos, const DirectX::XMFLOAT3 &target, const DirectX::XMFLOAT3 &worldUp);

		// Get the frustum volume of this camera
		DirectX::BoundingFrustum const &GetFrustum() const { return mFrustum; }

	private:
		// After modifying camera position/orientation, call to rebuild the view matrix once per frame.
		DLL_EXPORT void UpdateViewMatrix();

	private:
		//Camera coordinate system with coordinates relative to world space.
		DirectX::XMFLOAT3 mPosition; // view space origin
		DirectX::XMFLOAT3 mRight;    // view space x-axis
		DirectX::XMFLOAT3 mUp;       // view space y-axis
		DirectX::XMFLOAT3 mLook;     // view space z-axis

		// Cache frustum properties.
		float mNearZ;
		float mFarZ;
		float mFovY;
		float mNearWindowHeight;
		float mFarWindowHeight;
		DirectX::BoundingFrustum mFrustum;

		// Cache View/Proj matrices.
		DirectX::XMFLOAT4X4 mView;
		DirectX::XMFLOAT4X4 mProj;
	};
}

#endif
