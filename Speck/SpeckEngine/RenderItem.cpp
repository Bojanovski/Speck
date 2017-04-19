
#include "RenderItem.h"
#include "App.h"
#include "EngineCore.h"
#include "Camera.h"
#include "DirectXCore.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

void InstancedRenderItem::UpdateBuffer(FrameResource *currentFrameResource)
{
	auto upBuff = currentFrameResource->InstanceBuffer.get();
	int visibleInstanceCount = 0;

	for (UINT i = 0; i < (UINT)mInstances.size(); ++i)
	{
		upBuff->CopyData(visibleInstanceCount++, mInstances[i]);
	}

	mInstanceCount = visibleInstanceCount;
}

void InstancedRenderItem::Render(App *app, FrameResource* frameResource, FXMMATRIX invView)
{
	auto cmdList = app->GetEngineCore().GetDirectXCore().GetCommandList();
	cmdList->IASetVertexBuffers(0, 1, &mGeo->VertexBufferView());
	cmdList->IASetIndexBuffer(&mGeo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(mPrimitiveType);
	// Set the instance buffer to use for this render-item.  For structured buffers, we can bypass 
	// the heap and set as a root descriptor.
	auto instancedDataBufferResource = frameResource->InstanceBuffer->Resource();
	cmdList->SetGraphicsRootShaderResourceView(1, instancedDataBufferResource->GetGPUVirtualAddress());
	cmdList->DrawIndexedInstanced(mIndexCount, mInstanceCount, mStartIndexLocation, mBaseVertexLocation, 0);
}

void SingleRenderItem::UpdateBuffer(FrameResource *currentFrameResource)
{
	// Only update the cbuffer data if the constants have changed.  
	// This needs to be tracked per frame resource.
	if (mNumFramesDirty > 0)
	{
		XMMATRIX world = XMLoadFloat4x4(&mWorld);
		XMMATRIX texTransform = XMLoadFloat4x4(&mTexTransform);

		ObjectConstants objConstants;
		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
		objConstants.MaterialIndex = mMat->MatCBIndex;
		auto upBuff = currentFrameResource->ObjectCB.get();
		upBuff->CopyData(mObjCBIndex, objConstants);

		// Next FrameResource need to be updated too.
		mNumFramesDirty--;
	}
}

void SingleRenderItem::Render(App *app, FrameResource* frameResource, FXMMATRIX invView)
{
	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);
	// View space to the object's local space.
	XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);
	// Transform the camera frustum from view space to the object's local space.
	BoundingFrustum mCamFrustum = app->GetEngineCore().GetCamera().GetFrustum();
	BoundingFrustum localSpaceFrustum;
	mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

	// Perform the box/frustum intersection test in local space.
	if ((localSpaceFrustum.Contains(mBounds) != DirectX::DISJOINT))
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
