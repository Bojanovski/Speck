#include "CameraController.h"

using namespace Speck;
using namespace DirectX;

CameraController::CameraController()
{

}

void CameraController::UpdateCamera(float dt, Camera *mCam)
{
	mCam->UpdateViewMatrix();
}

void CameraController::SetPosition(float x, float y, float z, Camera *mCam)
{
	mCam->mPosition = XMFLOAT3(x, y, z);
}

void CameraController::SetPosition(const XMFLOAT3& v, Camera *mCam)
{
	mCam->mPosition = v;
}

void CameraController::Strafe(float d, Camera *mCam)
{
	// mPosition += d * mRight;
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&mCam->mRight);
	XMVECTOR p = XMLoadFloat3(&mCam->mPosition);
	XMStoreFloat3(&mCam->mPosition, XMVectorMultiplyAdd(s, r, p));
}

void CameraController::Walk(float d, Camera *mCam)
{
	// mPosition += d * mRight;
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&mCam->mLook);
	XMVECTOR p = XMLoadFloat3(&mCam->mPosition);
	XMStoreFloat3(&mCam->mPosition, XMVectorMultiplyAdd(s, l, p));
}

void CameraController::Pitch(float angle, Camera *mCam)
{
	// Rotate up and look vector about the right vector.
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mCam->mRight), angle);
	XMStoreFloat3(&mCam->mUp, XMVector3TransformNormal(XMLoadFloat3(&mCam->mUp), R));
	XMStoreFloat3(&mCam->mLook, XMVector3TransformNormal(XMLoadFloat3(&mCam->mLook), R));
}

void CameraController::RotateY(float angle, Camera *mCam)
{
	// Rotate the basis vectors about the world y-axis.
	XMMATRIX R = XMMatrixRotationY(angle);
	XMStoreFloat3(&mCam->mRight, XMVector3TransformNormal(XMLoadFloat3(&mCam->mRight), R));
	XMStoreFloat3(&mCam->mUp, XMVector3TransformNormal(XMLoadFloat3(&mCam->mUp), R));
	XMStoreFloat3(&mCam->mLook, XMVector3TransformNormal(XMLoadFloat3(&mCam->mLook), R));
}
