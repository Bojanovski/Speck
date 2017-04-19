
#include "Camera.h"
#include "DirectXHeaders.h"
#include "EngineCore.h"
#include "CameraController.h"

using namespace Speck;
using namespace DirectX;

Camera::Camera(EngineCore &engineCore)
	: EngineUser(engineCore)
{

}

Camera::~Camera()
{

}

void Camera::Update(float dt, CameraController *cc)
{
	if (cc)
		cc->UpdateCamera(dt, this);
}

float Camera::GetFovX() const
{
	float halfWidth = 0.5f * GetNearWindowWidth();
	return 2.0f * atan(halfWidth / mNearZ);
}

float Camera::GetNearWindowWidth() const
{
	return GetEngineCore().GetAspectRatio() * mNearWindowHeight;
}

float Camera::GetFarWindowWidth() const
{
	return GetEngineCore().GetAspectRatio() * mFarWindowHeight;
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
	// cache properties
	mFovY = fovY;
	mNearZ = zn;
	mFarZ = zf;

	mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
	mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

	XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, aspect, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
	
	// Frustum culling
	BoundingFrustum::CreateFromMatrix(mFrustum, GetProj());
}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
	XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
	XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&mPosition, pos);
	XMStoreFloat3(&mLook, L);
	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
	XMVECTOR P = XMLoadFloat3(&pos);
	XMVECTOR T = XMLoadFloat3(&target);
	XMVECTOR U = XMLoadFloat3(&up);

	LookAt(P, T, U);
}

void Camera::UpdateViewMatrix()
{
	XMVECTOR r = XMLoadFloat3(&mRight);
	XMVECTOR u = XMLoadFloat3(&mUp);
	XMVECTOR l = XMLoadFloat3(&mLook);
	XMVECTOR p = XMLoadFloat3(&mPosition);

	// Orthonormalize the right, up and look vectors.
	l = XMVector3Normalize(l);
	u = XMVector3Normalize(XMVector3Cross(l, r));
	r = XMVector3Cross(u, l);

	// Fill in the view matrix entries.
	float x = -XMVectorGetX(XMVector3Dot(p, r));
	float y = -XMVectorGetX(XMVector3Dot(p, u));
	float z = -XMVectorGetX(XMVector3Dot(p, l));

	XMStoreFloat3(&mRight, r);
	XMStoreFloat3(&mUp, u);
	XMStoreFloat3(&mLook, l);

	mView(0,0) = mRight.x;
	mView(1,0) = mRight.y;
	mView(2,0) = mRight.z;
	mView(3,0) = x;

	mView(0,1) = mUp.x;
	mView(1,1) = mUp.y;
	mView(2,1) = mUp.z;
	mView(3,1) = y;

	mView(0,2) = mLook.x;
	mView(1,2) = mLook.y;
	mView(2,2) = mLook.z;
	mView(3,2) = z;

	mView(0,3) = 0.0f;
	mView(1,3) = 0.0f;
	mView(2,3) = 0.0f;
	mView(3,3) = 1.0f;
}