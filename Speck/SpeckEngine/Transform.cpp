
#include "Transform.h"
#include "SpeckEngineDefinitions.h"

using namespace std;
using namespace DirectX;
using namespace Speck;

XMMATRIX Transform::GetWorldMatrix() const
{
	XMVECTOR s = XMLoadFloat3(&mS);
	XMVECTOR r = XMLoadFloat4(&mR);
	XMVECTOR t = XMLoadFloat3(&mT);
	XMMATRIX ret = XMMatrixScalingFromVector(s) * XMMatrixRotationQuaternion(r) * XMMatrixTranslationFromVector(t);
	return ret;
}

XMMATRIX Transform::GetTransposeWorldMatrix() const
{
	XMMATRIX ret = GetWorldMatrix();
	ret = XMMatrixTranspose(ret);
	return ret;
}

XMMATRIX Transform::GetInverseWorldMatrix() const
{
	XMVECTOR s = XMLoadFloat3(&mS);
	XMVECTOR is = XMVectorSet(1.0f / XMVectorGetX(s), 1.0f / XMVectorGetY(s), 1.0f / XMVectorGetZ(s), 0.0f);
	XMVECTOR ir = XMQuaternionInverse(XMLoadFloat4(&mR));
	XMVECTOR it = XMVectorMultiply(XMLoadFloat3(&mT), XMVectorSet(-1.0f, -1.0f, -1.0f, 1.0f));
	XMMATRIX ret = XMMatrixTranslationFromVector(it) * XMMatrixRotationQuaternion(ir) * XMMatrixScalingFromVector(is);
	return ret;
}

void Transform::MakeIdentity()
{
	mT = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mR = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mS = XMFLOAT3(1.0f, 1.0f, 1.0f);
}

void Transform::Store(XMFLOAT4X4 * dest) const
{
	XMStoreFloat4x4(dest, GetWorldMatrix());
}

void Transform::StoreTranspose(XMFLOAT4X4 * dest) const
{
	XMStoreFloat4x4(dest, GetTransposeWorldMatrix());
}

void Transform::StoreInverse(XMFLOAT4X4 * dest) const
{
	XMStoreFloat4x4(dest, GetInverseWorldMatrix());
}

Transform Transform::Identity()
{
	Transform ret;
	ret.MakeIdentity();
	return ret;
}
