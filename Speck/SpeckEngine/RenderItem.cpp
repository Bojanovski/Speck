
#include "RenderItem.h"
#include "App.h"
#include "EngineCore.h"
#include "Camera.h"
#include "DirectXCore.h"
#include "SpecksHandler.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

void SpecksRenderItem::Render(App *app, FrameResource* frameResource, const BoundingFrustum &camFrustum)
{
	auto cmdList = app->GetEngineCore().GetDirectXCore().GetCommandList();
	cmdList->IASetVertexBuffers(0, 1, &mGeo->VertexBufferView());
	cmdList->IASetIndexBuffer(&mGeo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(mPrimitiveType);
	// Set the instance buffer to use for this render-item.  For structured buffers, we can bypass 
	// the heap and set as a root descriptor.
	auto instancedDataBufferResource = frameResource->Buffers[mBufferIndex].first.Get();
	cmdList->SetGraphicsRootShaderResourceView(1, instancedDataBufferResource->GetGPUVirtualAddress());
	cmdList->DrawIndexedInstanced(mIndexCount, mInstanceCount, mStartIndexLocation, mBaseVertexLocation, 0);
}

void SingleRenderItem::UpdateBufferCPU(App *app, FrameResource *currentFrameResource)
{
	// Only update the cbuffer data if the constants have changed.  
	// This needs to be tracked per frame resource.
	if (mNumFramesDirty > 0)
	{
		XMMATRIX world = mT.GetWorldMatrix();
		XMMATRIX invTransposeWorld = mT.GetInvWorldMatrix(); // avoid double transposing
		XMMATRIX texTransform = XMLoadFloat4x4(&mTexTransform);

		ObjectConstants objConstants;
		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&objConstants.InvTransposeWorld, invTransposeWorld); // already transposed
		XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
		objConstants.MaterialIndex = mMat->MatCBIndex;
		auto upBuff = currentFrameResource->ObjectCB.get();
		upBuff->CopyData(mObjCBIndex, objConstants);

		// Next FrameResource need to be updated too.
		mNumFramesDirty--;
	}
}

void SingleRenderItem::Render(App *app, FrameResource* frameResource, const BoundingFrustum &camFrustum)
{
	XMVECTOR scale = XMLoadFloat3(&mT.mS);
	XMVECTOR invRotQuat = XMQuaternionInverse(XMLoadFloat4(&mT.mR));
	XMVECTOR invTranslation = XMVectorMultiply(XMLoadFloat3(&mT.mT), XMVectorSet(-1.0f, -1.0f, -1.0f, 1.0f));

	// Transform the camera frustum to the object's local space (while ignoring scale).
	BoundingFrustum localSpaceFrustum;
	// inverse world transform without scaling
	XMMATRIX invWorld = XMMatrixTranslationFromVector(invTranslation) * XMMatrixRotationQuaternion(invRotQuat); 
	camFrustum.Transform(localSpaceFrustum, invWorld);


	BoundingBox bounds;
	bounds.Center = mBounds->Center;
	bounds.Extents.x = XMVectorGetX(scale) * mBounds->Extents.x;
	bounds.Extents.y = XMVectorGetY(scale) * mBounds->Extents.y;
	bounds.Extents.z = XMVectorGetZ(scale) * mBounds->Extents.z;
	
	// Perform the box/frustum intersection test in local space.
	if ((localSpaceFrustum.Contains(bounds) != DirectX::DISJOINT))
	{
		auto cmdList = app->GetEngineCore().GetDirectXCore().GetCommandList();
		cmdList->IASetVertexBuffers(0, 1, &mGeo->VertexBufferView());
		cmdList->IASetIndexBuffer(&mGeo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(mPrimitiveType);

		// Bind all the textures used in this item.
		ID3D12DescriptorHeap* descriptorHeaps[] = { mMat->mSrvDescriptorHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		cmdList->SetGraphicsRootDescriptorTable(2, mMat->mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		// Set the constant buffer to use for this render-item.  For structured buffers, we can bypass 
		// the heap and set as a root descriptor.
		auto cbArrayResource = frameResource->ObjectCB->Resource();
		auto cbElementByteSize = frameResource->ObjectCB->GetElementByteSize();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = cbArrayResource->GetGPUVirtualAddress() + mObjCBIndex*cbElementByteSize;
		cmdList->SetGraphicsRootConstantBufferView(0, cbAddress);
		cmdList->DrawIndexedInstanced(mIndexCount, 1, mStartIndexLocation, mBaseVertexLocation, 0);
	}
}