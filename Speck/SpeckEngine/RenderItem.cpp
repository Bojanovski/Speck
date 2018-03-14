
#include "RenderItem.h"
#include "SpeckApp.h"
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
	ID3D12GraphicsCommandList *cmdList = GetDirectXCore(app).GetCommandList();
	cmdList->IASetVertexBuffers(0, 1, &mGeo->VertexBufferView());
	cmdList->IASetIndexBuffer(&mGeo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(mPrimitiveType);
	// Set the instance buffer to use for this render-item.  For structured buffers, we can bypass 
	// the heap and set as a root descriptor.
	auto instancedDataBufferResource = frameResource->Buffers[mBufferIndex].first.Get();
	cmdList->SetGraphicsRootShaderResourceView((UINT)SpeckApp::MainPassRootParameter::InstancesConstantBuffer, instancedDataBufferResource->GetGPUVirtualAddress());
	cmdList->DrawIndexedInstanced(mIndexCount, mInstanceCount, mStartIndexLocation, mBaseVertexLocation, 0);
}

void StaticRenderItem::UpdateBufferCPU(App *app, FrameResource *currentFrameResource)
{
	// Only update the cbuffer data if the constants have changed.  
	// This needs to be tracked per frame resource.
	if (mNumFramesDirty > 0)
	{
		XMMATRIX world = mW.GetWorldMatrix();
		XMMATRIX invTransposeWorld = mW.GetInverseWorldMatrix(); // avoid double transposing
		XMMATRIX texTransform = XMLoadFloat4x4(&mTexTransform);

		RenderItemConstants renderItemConstants;
		XMStoreFloat4x4(&renderItemConstants.Transform, XMMatrixTranspose(world));
		XMStoreFloat4x4(&renderItemConstants.InvTransposeTransform, invTransposeWorld); // already transposed
		XMStoreFloat4x4(&renderItemConstants.TexTransform, XMMatrixTranspose(texTransform));
		renderItemConstants.MaterialIndex = mMat->MatCBIndex;
		renderItemConstants.RenderItemType = RENDER_ITEM_TYPE_STATIC;
		auto upBuff = currentFrameResource->RenderItemConstantsBuffer.get();
		upBuff->CopyData(mRenderItemBufferIndex, renderItemConstants);

		// Next FrameResource need to be updated too.
		mNumFramesDirty--;
	}
}

void StaticRenderItem::Render(App *app, FrameResource* frameResource, const BoundingFrustum &camFrustum)
{
	XMVECTOR scale = XMLoadFloat3(&mW.mS);
	XMVECTOR invRotQuat = XMQuaternionInverse(XMLoadFloat4(&mW.mR));
	XMVECTOR invTranslation = XMVectorMultiply(XMLoadFloat3(&mW.mT), XMVectorSet(-1.0f, -1.0f, -1.0f, 1.0f));

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
		ID3D12GraphicsCommandList *cmdList = GetDirectXCore(app).GetCommandList();
		cmdList->IASetVertexBuffers(0, 1, &mGeo->VertexBufferView());
		cmdList->IASetIndexBuffer(&mGeo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(mPrimitiveType);

		// Bind all the textures used in this item.
		ID3D12DescriptorHeap* descriptorHeaps[] = { mMat->mSrvDescriptorHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		cmdList->SetGraphicsRootDescriptorTable((UINT)SpeckApp::MainPassRootParameter::TexturesDescriptorTable, mMat->mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		// Set the constant buffer to use for this render-item.  For structured buffers, we can bypass 
		// the heap and set as a root descriptor.
		auto cbArrayResource = frameResource->RenderItemConstantsBuffer->Resource();
		auto cbElementByteSize = frameResource->RenderItemConstantsBuffer->GetElementByteSize();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = cbArrayResource->GetGPUVirtualAddress() + mRenderItemBufferIndex*cbElementByteSize;
		cmdList->SetGraphicsRootConstantBufferView((UINT)SpeckApp::MainPassRootParameter::StaticConstantBuffer, cbAddress);
		cmdList->DrawIndexedInstanced(mIndexCount, 1, mStartIndexLocation, mBaseVertexLocation, 0);
	}
}

void SpeckRigidBodyRenderItem::UpdateBufferCPU(App * app, FrameResource * currentFrameResource)
{
	// Only update the cbuffer data if the constants have changed.  
	// This needs to be tracked per frame resource.
	if (mNumFramesDirty > 0)
	{
		XMMATRIX world = mL.GetWorldMatrix();
		XMMATRIX invTransposeWorld = mL.GetInverseWorldMatrix(); // avoid double transposing
		XMMATRIX texTransform = XMLoadFloat4x4(&mTexTransform);

		RenderItemConstants renderItemConstants;
		XMStoreFloat4x4(&renderItemConstants.Transform, XMMatrixTranspose(world));
		XMStoreFloat4x4(&renderItemConstants.InvTransposeTransform, invTransposeWorld); // already transposed
		XMStoreFloat4x4(&renderItemConstants.TexTransform, XMMatrixTranspose(texTransform));
		renderItemConstants.MaterialIndex = mMat->MatCBIndex;
		renderItemConstants.RenderItemType = RENDER_ITEM_TYPE_SPECK_RIGID_BODY;
		renderItemConstants.Param[0].x = mSpeckRigidBodyIndex;
		auto upBuff = currentFrameResource->RenderItemConstantsBuffer.get();
		upBuff->CopyData(mRenderItemBufferIndex, renderItemConstants);

		// Next FrameResource need to be updated too.
		mNumFramesDirty--;
	}
}

void SpeckRigidBodyRenderItem::Render(App * app, FrameResource * frameResource, const BoundingFrustum & camFrustum)
{
	ID3D12GraphicsCommandList *cmdList = GetDirectXCore(app).GetCommandList();
	cmdList->IASetVertexBuffers(0, 1, &mGeo->VertexBufferView());
	cmdList->IASetIndexBuffer(&mGeo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(mPrimitiveType);

	// Bind all the textures used in this item.
	ID3D12DescriptorHeap* descriptorHeaps[] = { mMat->mSrvDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	cmdList->SetGraphicsRootDescriptorTable((UINT)SpeckApp::MainPassRootParameter::TexturesDescriptorTable, mMat->mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	// Set the constant buffer to use for this render-item.  For structured buffers, we can bypass 
	// the heap and set as a root descriptor.
	auto cbArrayResource = frameResource->RenderItemConstantsBuffer->Resource();
	auto cbElementByteSize = frameResource->RenderItemConstantsBuffer->GetElementByteSize();
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = cbArrayResource->GetGPUVirtualAddress() + mRenderItemBufferIndex*cbElementByteSize;
	cmdList->SetGraphicsRootConstantBufferView((UINT)SpeckApp::MainPassRootParameter::StaticConstantBuffer, cbAddress);
	cmdList->DrawIndexedInstanced(mIndexCount, 1, mStartIndexLocation, mBaseVertexLocation, 0);
}

void SpeckSkeletalBodyRenderItem::UpdateBufferCPU(App * app, FrameResource * currentFrameResource)
{
	// Only update the cbuffer data if the constants have changed.  
	// This needs to be tracked per frame resource.
	if (mNumFramesDirty > 0)
	{
		XMMATRIX world = XMMatrixIdentity();
		XMMATRIX invTransposeWorld = XMMatrixIdentity();
		XMMATRIX texTransform = XMLoadFloat4x4(&mTexTransform);

		RenderItemConstants renderItemConstants;
		XMStoreFloat4x4(&renderItemConstants.Transform, XMMatrixTranspose(world));
		XMStoreFloat4x4(&renderItemConstants.InvTransposeTransform, invTransposeWorld); // already transposed
		XMStoreFloat4x4(&renderItemConstants.TexTransform, XMMatrixTranspose(texTransform));
		renderItemConstants.MaterialIndex = mMat->MatCBIndex;
		renderItemConstants.RenderItemType = RENDER_ITEM_TYPE_SPECK_SKELETAL_BODY;
		auto upBuff = currentFrameResource->RenderItemConstantsBuffer.get();
		upBuff->CopyData(mRenderItemBufferIndex, renderItemConstants);

		// Next FrameResource need to be updated too.
		mNumFramesDirty--;
	}
}

void SpeckSkeletalBodyRenderItem::Render(App * app, FrameResource * frameResource, const BoundingFrustum & camFrustum)
{
	ID3D12GraphicsCommandList *cmdList = GetDirectXCore(app).GetCommandList();
	cmdList->IASetVertexBuffers(0, 1, &mGeo->VertexBufferView());
	cmdList->IASetIndexBuffer(&mGeo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(mPrimitiveType);

	// Bind all the textures used in this item.
	ID3D12DescriptorHeap* descriptorHeaps[] = { mMat->mSrvDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	cmdList->SetGraphicsRootDescriptorTable((UINT)SpeckApp::MainPassRootParameter::TexturesDescriptorTable, mMat->mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	// Set the constant buffer to use for this render-item.  For structured buffers, we can bypass 
	// the heap and set as a root descriptor.
	auto cbArrayResource = frameResource->RenderItemConstantsBuffer->Resource();
	auto cbElementByteSize = frameResource->RenderItemConstantsBuffer->GetElementByteSize();
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = cbArrayResource->GetGPUVirtualAddress() + mRenderItemBufferIndex*cbElementByteSize;
	cmdList->SetGraphicsRootConstantBufferView((UINT)SpeckApp::MainPassRootParameter::StaticConstantBuffer, cbAddress);
	cmdList->DrawIndexedInstanced(mIndexCount, 1, mStartIndexLocation, mBaseVertexLocation, 0);
}

DirectXCore &RenderItem::GetDirectXCore(App * app) const
{
	return app->GetEngineCore().GetDirectXCore();
}
