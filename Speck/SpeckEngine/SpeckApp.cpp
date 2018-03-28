
#include "SpeckApp.h"
#include "DirectXCore.h"
#include "FrameResource.h"
#include "GenericShaderStructures.h"
#include "GeometryGenerator.h"
#include "PSOGroup.h"
#include "RenderItem.h"
#include "SpeckWorld.h"
#include "CubeRenderTarget.h"
#include "DDSTextureGenerator.h"
#include "SpecksHandler.h"
#include "Resources.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Speck;

GeometryGenerator::PointCloud gSSAOOffsetVectors;

void Speck::CreateSpeckApp(HINSTANCE hInstance, float speckRadius, UINT maxNumberOfMaterials, UINT maxRenderItemsCount,
	unique_ptr<EngineCore> *ec, unique_ptr<World> *world, unique_ptr<App> *app)
{
	SpecksHandler::SetSpeckRadius(speckRadius);
	*ec = make_unique<EngineCore>();
	*world = make_unique<SpeckWorld>(maxRenderItemsCount);
	*app = make_unique<SpeckApp>(hInstance, maxNumberOfMaterials, *ec->get(), *world->get());

}

SpeckApp::SpeckApp(HINSTANCE hInstance, UINT maxNumberOfMaterials, EngineCore &ec, World &w)
	: D3DApp(hInstance, ec, w),
	mMaxNumberOfMaterials(maxNumberOfMaterials)
{
}

SpeckApp::~SpeckApp()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	if (dxCore.GetDevice() != nullptr)
	{
		dxCore.FlushCommandQueue();
	}

	// Deinitialize static data.
	CubeRenderTarget::ReleaseStaticMembers();
	SpecksHandler::ReleaseStaticMembers();
}

bool SpeckApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	auto &dxCore = GetEngineCore().GetDirectXCore();
	SpeckWorld *world = static_cast<SpeckWorld *>(&GetWorld());

	// Reset the command list to prep for initialization commands.
	THROW_IF_FAILED(dxCore.GetCommandList()->Reset(dxCore.GetCommandAllocator(), nullptr));
	
	// Initialize static data.
	CubeRenderTarget::BuildStaticMembers(dxCore.GetDevice(), dxCore.GetCommandList());
	SpecksHandler::BuildStaticMembers(dxCore.GetDevice(), dxCore.GetCommandList());
	
	BuildFrameResources();
	BuildScreenGeometry();
	BuildSpeckGeometry();
	BuildRegularGeometry();

	world->Initialize(this);

	BuildDefaultTextures();
	BuildDefaultMaterials();
	BuildRootSignatures();
	BuildPSOs();

	// Execute the initialization commands.
	THROW_IF_FAILED(dxCore.GetCommandList()->Close());
	ID3D12CommandList* cmdsLists[] = { dxCore.GetCommandList() };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	dxCore.FlushCommandQueue();

	return true;
}

void SpeckApp::OnResize()
{
	D3DApp::OnResize();

	auto &dxCore = GetEngineCore().GetDirectXCore();
	// Flush before changing any resources.
	GetEngineCore().GetDirectXCore().FlushCommandQueue();
	THROW_IF_FAILED(dxCore.GetCommandList()->Reset(dxCore.GetCommandAllocator(), nullptr));

	// Do the (re)initialization.
	gSSAOOffsetVectors = GeometryGenerator::CreateOffsetVectors();
	BuildDeferredRenderTargetsAndBuffers();
	BuildSSAORenderTargetsAndBuffers();
	BuildRandomVectorBuffer();
	BuildSRVs();

	// Execute the resize commands.
	THROW_IF_FAILED(dxCore.GetCommandList()->Close());
	ID3D12CommandList* cmdsLists[] = { dxCore.GetCommandList() };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	GetEngineCore().GetDirectXCore().FlushCommandQueue();

	// For some buffers
	mNumFramesDirty = NUM_FRAME_RESOURCES;
}

void SpeckApp::Update(const Timer& gt)
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	D3DApp::Update(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % NUM_FRAME_RESOURCES;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && dxCore.GetFence()->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		THROW_IF_FAILED(dxCore.GetFence()->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	GetWorld().Update();
	UpdateSSAODataBuffer(gt);
	UpdateMaterialBuffer(gt);

	// Decrease the number of dirty frames if necessary
	if (mNumFramesDirty > 0) mNumFramesDirty--;
}

void SpeckApp::PreDrawUpdate(const Timer & gt)
{
	D3DApp::PreDrawUpdate(gt);
	GetWorld().PreDrawUpdate();
}

void SpeckApp::Draw(const Timer& gt)
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	THROW_IF_FAILED(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	THROW_IF_FAILED(dxCore.GetCommandList()->Reset(cmdListAlloc.Get(), nullptr));

	// Update that needs the command list
	PreDrawUpdate(gt);

	//
	// Draw the scene
	//
	{
		GraphicsDebuggerAnnotator gda(dxCore, "DrawScene");
		D3DApp::Draw(gt);
		dxCore.GetCommandList()->RSSetViewports(1, &mDeferredRTViewport);
		dxCore.GetCommandList()->RSSetScissorRects(1, &mDeferredRTScissorRect);

		// Indicate a state transition on the resource usage and also clear the back buffer.
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = mDeferredRTVHeapHandle;
		for (UINT i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; i++)
		{
			dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDeferredRTBs[i].Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
			dxCore.GetCommandList()->ClearRenderTargetView(rtvHandle, mDeferredRTClearColors[i], 0, nullptr);
			rtvHandle.Offset(1, dxCore.GetRtvDescriptorSize());
		}

		// Clear the depth buffer.
		dxCore.GetCommandList()->ClearDepthStencilView(mDeferredDSVHeapHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		// Specify the buffers we are going to render to.
		dxCore.GetCommandList()->OMSetRenderTargets(DEFERRED_RENDER_TARGETS_COUNT, &mDeferredRTVHeapHandle, true, &mDeferredDSVHeapHandle);
		dxCore.GetCommandList()->SetGraphicsRootSignature(mRootSignature.Get());

		// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
		// set as a root descriptor.
		auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
		dxCore.GetCommandList()->SetGraphicsRootShaderResourceView((UINT)MainPassRootParameter::MaterialsRootDescriptor, matBuffer->GetGPUVirtualAddress());

		// Bind the main pass constant buffer.
		auto passCB = mCurrFrameResource->PassCB->Resource();
		dxCore.GetCommandList()->SetGraphicsRootConstantBufferView((UINT)MainPassRootParameter::PassRootDescriptor, passCB->GetGPUVirtualAddress());

		// Drawing
		GetWorld().Draw(0);

		// Indicate a state transition on the resource usage.
		for (UINT i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; i++)
		{
			dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDeferredRTBs[i].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
		}
	}

	//
	// Draw the SSAO
	//
	{
		GraphicsDebuggerAnnotator gda(dxCore, "SSAO");
		dxCore.GetCommandList()->RSSetViewports(1, &mSSAORTViewport);
		dxCore.GetCommandList()->RSSetScissorRects(1, &mSSAORTScissorRect);

		// Indicate a state transition on the resource usage.
		dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSSAOBuffer[mSSAOWriteBufferIndex].Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

		// Clear the render target.
		dxCore.GetCommandList()->ClearRenderTargetView(mSSAORTVHeapHandle[mSSAOWriteBufferIndex], mSSAOBufferClearColor, 0, nullptr);

		// Specify the buffers we are going to render to.
		dxCore.GetCommandList()->OMSetRenderTargets(1, &mSSAORTVHeapHandle[mSSAOWriteBufferIndex], true, nullptr);

		ID3D12DescriptorHeap* descriptorHeaps[] = { mSSAOSRVDescriptorHeap.Get() };
		dxCore.GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		dxCore.GetCommandList()->SetGraphicsRootSignature(mSSAORootSignature.Get());
		dxCore.GetCommandList()->SetPipelineState(mSSAOPSO.Get());

		// Upload the settings constants
		auto ssaoData = mCurrFrameResource->SSAODataBuffer->Resource();
		dxCore.GetCommandList()->SetGraphicsRootConstantBufferView((UINT)SSAORootParameter::DataBufferRootDescriptor, ssaoData->GetGPUVirtualAddress());

		// Bind all the pre rendered scene texture components used in this post process pass.
		dxCore.GetCommandList()->SetGraphicsRootDescriptorTable((UINT)SSAORootParameter::TexturesDescriptorTable, mSSAOTextureMapsSRVHeapHandle);

		auto geo = mGeometries["screenGeo"].get();
		dxCore.GetCommandList()->IASetVertexBuffers(0, 1, &geo->VertexBufferView());
		dxCore.GetCommandList()->IASetIndexBuffer(&geo->IndexBufferView());
		dxCore.GetCommandList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		auto &subMesh = geo->DrawArgs["quad"];
		dxCore.GetCommandList()->DrawIndexedInstanced(subMesh.IndexCount, 1, subMesh.StartIndexLocation, subMesh.BaseVertexLocation, 0);

		// Indicate a state transition on the resource usage.
		dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSSAOBuffer[mSSAOWriteBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

		// Blurring
		dxCore.GetCommandList()->SetPipelineState(mBlurredSSAOPSO.Get());
		for (UINT i = 0; i < mSSAOBlurringIterationCount; i++)
		{
			{	// Horizontal blur
				GraphicsDebuggerAnnotator gda(dxCore, "HorizontalBlurSSAO");
				mSSAOWriteBufferIndex = (mSSAOWriteBufferIndex + 1) % 2;
				mSSAOReadBufferIndex = (mSSAOReadBufferIndex + 1) % 2;

				// Indicate a state transition on the resource usage.
				dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSSAOBuffer[mSSAOWriteBufferIndex].Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

				dxCore.GetCommandList()->ClearRenderTargetView(mSSAORTVHeapHandle[mSSAOWriteBufferIndex], mSSAOBufferClearColor, 0, nullptr);
				dxCore.GetCommandList()->OMSetRenderTargets(1, &mSSAORTVHeapHandle[mSSAOWriteBufferIndex], true, nullptr);

				// Bind the appropriate previous texture.
				dxCore.GetCommandList()->SetGraphicsRootDescriptorTable((UINT)SSAORootParameter::PreviousResultTextureDescriptorTable, mSSAOPreviousResultSRVHeapHandle[mSSAOReadBufferIndex]);
				
				// Upload the settings constants
				int values[] = { 0 };
				dxCore.GetCommandList()->SetGraphicsRoot32BitConstants((UINT)SSAORootParameter::PerPassRootConstant, _countof(values), &values, 0);

				// Draw
				dxCore.GetCommandList()->DrawIndexedInstanced(subMesh.IndexCount, 1, subMesh.StartIndexLocation, subMesh.BaseVertexLocation, 0);

				// Indicate a state transition on the resource usage.
				dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSSAOBuffer[mSSAOWriteBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
			}

			{	// Vertical blur
				GraphicsDebuggerAnnotator gda(dxCore, "VerticalBlurSSAO");
				mSSAOWriteBufferIndex = (mSSAOWriteBufferIndex + 1) % 2;
				mSSAOReadBufferIndex = (mSSAOReadBufferIndex + 1) % 2;

				// Indicate a state transition on the resource usage.
				dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSSAOBuffer[mSSAOWriteBufferIndex].Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

				dxCore.GetCommandList()->ClearRenderTargetView(mSSAORTVHeapHandle[mSSAOWriteBufferIndex], mSSAOBufferClearColor, 0, nullptr);
				dxCore.GetCommandList()->OMSetRenderTargets(1, &mSSAORTVHeapHandle[mSSAOWriteBufferIndex], true, nullptr);

				// Bind the appropriate previous texture.
				dxCore.GetCommandList()->SetGraphicsRootDescriptorTable((UINT)SSAORootParameter::PreviousResultTextureDescriptorTable, mSSAOPreviousResultSRVHeapHandle[mSSAOReadBufferIndex]);

				// Upload the settings constants
				int values[] = { 1 };
				dxCore.GetCommandList()->SetGraphicsRoot32BitConstants((UINT)SSAORootParameter::PerPassRootConstant, _countof(values), &values, 0);

				// Draw
				dxCore.GetCommandList()->DrawIndexedInstanced(subMesh.IndexCount, 1, subMesh.StartIndexLocation, subMesh.BaseVertexLocation, 0);

				// Indicate a state transition on the resource usage.
				dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSSAOBuffer[mSSAOWriteBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
			}
		}
	}

	//
	// Draw the screen quad (postprocess)
	//
	{
		GraphicsDebuggerAnnotator gda(dxCore, "DrawScenePostProcess");
		dxCore.GetCommandList()->RSSetViewports(1, &dxCore.GetScreenViewport());
		dxCore.GetCommandList()->RSSetScissorRects(1, &dxCore.GetScissorRect());

		// Indicate a state transition on the resource usage.
		dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		// Clear the render target.
		dxCore.GetCommandList()->ClearRenderTargetView(CurrentBackBufferView(), dxCore.GetClearRTColor(), 0, nullptr);

		// Specify the buffers we are going to render to.
		dxCore.GetCommandList()->OMSetRenderTargets(1, &CurrentBackBufferView(), true, nullptr);

		ID3D12DescriptorHeap* descriptorHeaps[] = { mPostProcessSrvDescriptorHeap.Get() };
		dxCore.GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		dxCore.GetCommandList()->SetGraphicsRootSignature(mPostProcessRootSignature.Get());
		dxCore.GetCommandList()->SetPipelineState(mPostProcessPSO.Get());

		// Upload the settings constants
		UINT values[] = {
			(UINT)dxCore.GetClientWidth() * mSuperSampling, (UINT)dxCore.GetClientHeight() * mSuperSampling,
			(UINT)dxCore.GetClientWidth(), (UINT)dxCore.GetClientHeight() };
		dxCore.GetCommandList()->SetGraphicsRoot32BitConstants((UINT)PostProcessRootParameter::SettingsRootConstant, _countof(values), &values, 0);

		// Bind all the pre rendered scene texture components used in this post process pass.
		dxCore.GetCommandList()->SetGraphicsRootDescriptorTable((UINT)PostProcessRootParameter::TexturesDescriptorTable, mPostProcessSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		auto geo = mGeometries["screenGeo"].get();
		dxCore.GetCommandList()->IASetVertexBuffers(0, 1, &geo->VertexBufferView());
		dxCore.GetCommandList()->IASetIndexBuffer(&geo->IndexBufferView());
		dxCore.GetCommandList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		auto &subMesh = geo->DrawArgs["quad"];
		dxCore.GetCommandList()->DrawIndexedInstanced(subMesh.IndexCount, 1, subMesh.StartIndexLocation, subMesh.BaseVertexLocation, 0);

		// Indicate a state transition on the resource usage.
		dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	}

	//
	// Done recording commands.
	//
	THROW_IF_FAILED(dxCore.GetCommandList()->Close());
	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { dxCore.GetCommandList() };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	// Swap the back and front buffers
	THROW_IF_FAILED(dxCore.GetSwapChain()->Present(0, 0));
	UpdateBackBuffer();

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = IncrementCurrentFence();
	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	dxCore.GetCommandQueue()->Signal(dxCore.GetFence(), dxCore.GetCurrentFence());
}

int SpeckApp::GetRTVDescriptorCount()
{
	return 
		SWAP_CHAIN_BUFFER_COUNT +
		DEFERRED_RENDER_TARGETS_COUNT +
		1 +	// for SSAO
		1;	// for blurred SSAO
}

int SpeckApp::GetDSVDescriptorCount()
{
	// one additional for deferred render targets, because it can have a different width and height than the one on the backbuffer.
	return 1 + 1; 
}

void SpeckApp::UpdateSSAODataBuffer(const Timer & gt)
{
	if (mNumFramesDirty > 0)
	{
		auto &dxCore = GetEngineCore().GetDirectXCore();
		auto currSSAODataBuffer = mCurrFrameResource->SSAODataBuffer.get();
		SSAOData data;

		XMMATRIX P = GetEngineCore().GetCamera().GetProj();
		XMMATRIX invP = XMMatrixInverse(&XMMatrixDeterminant(P), P);
		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

		XMStoreFloat4x4(&data.Proj, XMMatrixTranspose(P));
		XMStoreFloat4x4(&data.InvProj, XMMatrixTranspose(invP));
		XMStoreFloat4x4(&data.ProjTex, XMMatrixTranspose(P*T));

		data.SSAOMapsWidth = mSSAOBufferWidth;
		data.SSAOMapsHeight = mSSAOBufferHeight;
		data.TextureMapsWidth = (UINT)dxCore.GetClientWidth() * mSuperSampling;
		data.TextureMapsHeight = (UINT)dxCore.GetClientHeight() * mSuperSampling;

		data.OcclusionRadius = SpecksHandler::GetSpeckRadius() * 4.0f;
		data.OcclusionFadeStart = 0.1f;
		data.OcclusionFadeEnd = SpecksHandler::GetSpeckRadius() * 2.0f;
		data.SurfaceEpsilon = 0.05f;

		memcpy(data.OffsetVectors, gSSAOOffsetVectors.Positions.data(), SSAO_RANDOM_SAMPLES_N*sizeof(XMFLOAT4));

		auto blurWeights = CalcGaussWeights(2.5f);
		data.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
		data.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
		data.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

		currSSAODataBuffer->CopyData(0, data);
	}
}

void SpeckApp::UpdateMaterialBuffer(const Timer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			MaterialBufferData matBufferData(mat);
			currMaterialBuffer->CopyData(mat->MatCBIndex, matBufferData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void SpeckApp::BuildDefaultTextures()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();

	auto defaultAlbedo = make_unique<Texture>();
	defaultAlbedo->Name = "defaultAlbedo";
	BuildColorTexture({1.0f, 1.0f, 1.0f, 1.0f}, dxCore.GetDevice(), dxCore.GetCommandList(), defaultAlbedo->Resource, defaultAlbedo->UploadHeap);

	auto defaultNormal = make_unique<Texture>();
	defaultNormal->Name = "defaultNormal";
	BuildColorTexture({ 128.0f / 255.0f, 128.0f / 255.0f, 1.0f, 1.0f }, dxCore.GetDevice(), dxCore.GetCommandList(), defaultNormal->Resource, defaultNormal->UploadHeap);

	auto defaultHeight = make_unique<Texture>();
	defaultHeight->Name = "defaultHeight";
	BuildColorTexture({ 0.5f, 0.5f, 0.5f, 1.0f }, dxCore.GetDevice(), dxCore.GetCommandList(), defaultHeight->Resource, defaultHeight->UploadHeap);

	auto defaultMetalness = make_unique<Texture>();
	defaultMetalness->Name = "defaultMetalness";
	BuildColorTexture({ 0.5f, 0.5f, 0.5f, 1.0f }, dxCore.GetDevice(), dxCore.GetCommandList(), defaultMetalness->Resource, defaultMetalness->UploadHeap);

	auto defaultRough = make_unique<Texture>();
	defaultRough->Name = "defaultRough";
	BuildColorTexture({ 0.5f, 0.5f, 0.5f, 1.0f }, dxCore.GetDevice(), dxCore.GetCommandList(), defaultRough->Resource, defaultRough->UploadHeap);

	auto defaultAO = make_unique<Texture>();
	defaultAO->Name = "defaultAO";
	BuildColorTexture({ 1.0f, 1.0f, 1.0f, 1.0f }, dxCore.GetDevice(), dxCore.GetCommandList(), defaultAO->Resource, defaultAO->UploadHeap);

	// Save in this array first
	mDefaultTextures[0] = defaultAlbedo->Resource.Get();
	mDefaultTextures[1] = defaultNormal->Resource.Get();
	mDefaultTextures[2] = defaultHeight->Resource.Get();
	mDefaultTextures[3] = defaultMetalness->Resource.Get();
	mDefaultTextures[4] = defaultRough->Resource.Get();
	mDefaultTextures[5] = defaultAO->Resource.Get();

	// Now save to map
	mTextures[defaultAlbedo->Name] = move(defaultAlbedo);
	mTextures[defaultNormal->Name] = move(defaultNormal);
	mTextures[defaultHeight->Name] = move(defaultHeight);
	mTextures[defaultMetalness->Name] = move(defaultMetalness);
	mTextures[defaultRough->Name] = move(defaultRough);
	mTextures[defaultAO->Name] = move(defaultAO);
}

void SpeckApp::BuildDefaultMaterials()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	// Create the material.
	PBRMaterial texMat;

	// Get textures
	ID3D12Resource *t[MATERIAL_TEXTURES_COUNT];
	for (int i = 0; i < MATERIAL_TEXTURES_COUNT; ++i)
	{
		// Use of default
		t[i] = GetDefaultTexture(i);
	}

	// Create the SRV heaps for item.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = MATERIAL_TEXTURES_COUNT;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	THROW_IF_FAILED(dxCore.GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&texMat.mSrvDescriptorHeap)));

	// Fill out the heap with SRV descriptors for scene.
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(texMat.mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < MATERIAL_TEXTURES_COUNT; ++i)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = t[i]->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = t[i]->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		dxCore.GetDevice()->CreateShaderResourceView(t[i], &srvDesc, hDescriptor);
		// next descriptor
		hDescriptor.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
	}

	// Add the new materials.
	texMat.DiffuseAlbedo = XMFLOAT4(0.761f, 0.698f, 0.502f, 1.0f);
	texMat.MatCBIndex = mLatestMatCBIndex++;
	mMaterials["speckSand"] = make_unique<PBRMaterial>(texMat);

	texMat.DiffuseAlbedo = XMFLOAT4(0.3f, 0.4f, 0.9f, 1.0f);
	texMat.MatCBIndex = mLatestMatCBIndex++;
	mMaterials["speckWater"] = make_unique<PBRMaterial>(texMat);

	texMat.DiffuseAlbedo = XMFLOAT4(0.46f, 0.49f, 0.46f, 1.0f);
	texMat.MatCBIndex = mLatestMatCBIndex++;
	mMaterials["speckStone"] = make_unique<PBRMaterial>(texMat);

	texMat.DiffuseAlbedo = XMFLOAT4(0.9f, 0.2f, 0.2f, 1.0f);
	texMat.MatCBIndex = mLatestMatCBIndex++;
	mMaterials["speckJoint"] = make_unique<PBRMaterial>(texMat);
}

array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return{
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

void SpeckApp::BuildRootSignatures()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();

	//
	// Scene drawing signature
	//
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MATERIAL_TEXTURES_COUNT, 0, 0);
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[(UINT)MainPassRootParameter::Count];
	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[(UINT)MainPassRootParameter::StaticConstantBuffer].InitAsConstantBufferView(0);											// root descriptor (for static objects)
	slotRootParameter[(UINT)MainPassRootParameter::InstancesConstantBuffer].InitAsShaderResourceView(0, 1);										// root descriptor (for object instances)
	slotRootParameter[(UINT)MainPassRootParameter::TexturesDescriptorTable].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);	// descriptor table (for textures)
	slotRootParameter[(UINT)MainPassRootParameter::MaterialsRootDescriptor].InitAsShaderResourceView(1, 1);										// root descriptor (for materials)
	slotRootParameter[(UINT)MainPassRootParameter::RigidBodyRootDescriptor].InitAsShaderResourceView(2, 1);										// root descriptor (for rigid bodies)
	slotRootParameter[(UINT)MainPassRootParameter::PassRootDescriptor].InitAsConstantBufferView(1);												// root descriptor (for pass buffer)

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc((UINT)MainPassRootParameter::Count, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	THROW_IF_FAILED(hr);

	THROW_IF_FAILED(dxCore.GetDevice()->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	// SSAO signature
	{
		CD3DX12_DESCRIPTOR_RANGE texTableSSAO_0;
		texTableSSAO_0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTableSSAO_1;
		texTableSSAO_1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, SSAO_INPUT_TEXTURE_COUNT, 0, 1);
		
		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameterSSAO[(UINT)SSAORootParameter::Count];
		// Perfomance TIP: Order from most frequent to least frequent.
		slotRootParameterSSAO[(UINT)SSAORootParameter::PreviousResultTextureDescriptorTable].InitAsDescriptorTable(1, &texTableSSAO_0, D3D12_SHADER_VISIBILITY_PIXEL);		// descriptor table (for input texture)
		slotRootParameterSSAO[(UINT)SSAORootParameter::PerPassRootConstant].InitAsConstants(1, 1);																			// root constant (for per-pass constants)
		slotRootParameterSSAO[(UINT)SSAORootParameter::DataBufferRootDescriptor].InitAsConstantBufferView(0);																// root descriptor (for settings)
		slotRootParameterSSAO[(UINT)SSAORootParameter::TexturesDescriptorTable].InitAsDescriptorTable(1, &texTableSSAO_1, D3D12_SHADER_VISIBILITY_PIXEL);					// descriptor table (for textures)

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = { pointClamp , linearWrap };
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDescSSAO((UINT)SSAORootParameter::Count, slotRootParameterSSAO, _countof(samplers), samplers,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
		serializedRootSig = nullptr;
		errorBlob = nullptr;
		hr = D3D12SerializeRootSignature(&rootSigDescSSAO, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if (errorBlob != nullptr)
		{
			::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		THROW_IF_FAILED(hr);

		THROW_IF_FAILED(dxCore.GetDevice()->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mSSAORootSignature.GetAddressOf())));
	}

	// Post process signature
	{
		CD3DX12_DESCRIPTOR_RANGE texTablePostProcess;
		texTablePostProcess.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, POST_PROCESS_INPUT_TEXTURE_COUNT, 0, 0);
		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameterPostProcess[(UINT)PostProcessRootParameter::Count];
		// Perfomance TIP: Order from most frequent to least frequent.
		slotRootParameterPostProcess[(UINT)PostProcessRootParameter::SettingsRootConstant].InitAsConstants(4, 0, 0);															// root constant (for settings)
		slotRootParameterPostProcess[(UINT)PostProcessRootParameter::TexturesDescriptorTable].InitAsDescriptorTable(1, &texTablePostProcess, D3D12_SHADER_VISIBILITY_PIXEL);	// descriptor table (for textures)

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = { pointClamp , linearWrap };
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDescPostProcess((UINT)PostProcessRootParameter::Count, slotRootParameterPostProcess, _countof(samplers), samplers,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
		serializedRootSig = nullptr;
		errorBlob = nullptr;
		hr = D3D12SerializeRootSignature(&rootSigDescPostProcess, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if (errorBlob != nullptr)
		{
			::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		THROW_IF_FAILED(hr);

		THROW_IF_FAILED(dxCore.GetDevice()->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mPostProcessRootSignature.GetAddressOf())));
	}
}

void SpeckApp::BuildScreenGeometry()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	GeometryGenerator gg;
	GeometryGenerator::StaticMeshData md = gg.CreateQuad(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f);

	struct ScreenQuadVertex
	{
		XMFLOAT3 Position;
		XMFLOAT2 TexCoord;
	};

	vector<ScreenQuadVertex> vertices(md.Vertices.size());
	for (int i = 0; i < md.Vertices.size(); ++i)
	{
		vertices[i].Position = md.Vertices[i].Position;
		vertices[i].TexCoord = md.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(ScreenQuadVertex);
	const UINT ibByteSize = (UINT)md.Indices32.size() * sizeof(int32_t);

	auto geo = make_unique<MeshGeometry>();

	THROW_IF_FAILED(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	THROW_IF_FAILED(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), md.Indices32.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(dxCore.GetDevice(),
		dxCore.GetCommandList(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = CreateDefaultBuffer(dxCore.GetDevice(),
		dxCore.GetCommandList(), md.Indices32.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(ScreenQuadVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)md.Indices32.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["quad"] = submesh;
	mGeometries["screenGeo"] = move(geo);
}

void SpeckApp::BuildSpeckGeometry()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	GeometryGenerator gg;
	GeometryGenerator::StaticMeshData md = gg.CreateGeosphere(SpecksHandler::GetSpeckRadius(), 3);

	BoundingBox bounds;
	XMStoreFloat3(&bounds.Center, XMVectorZero());
	XMStoreFloat3(&bounds.Extents, XMVectorSet(0.5f, 0.5f, 0.5f, 0.0f));

	struct SpeckVertex
	{
		XMFLOAT3 Position;
		XMFLOAT3 Normal;
	};

	vector<SpeckVertex> vertices(md.Vertices.size());
	for (int i = 0; i < md.Vertices.size(); ++i)
	{
		vertices[i].Position = md.Vertices[i].Position;
		vertices[i].Normal = md.Vertices[i].Normal;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(SpeckVertex);
	const UINT ibByteSize = (UINT)md.Indices32.size() * sizeof(int32_t);

	auto geo = make_unique<MeshGeometry>();

	THROW_IF_FAILED(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	THROW_IF_FAILED(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), md.Indices32.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(dxCore.GetDevice(),
		dxCore.GetCommandList(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = CreateDefaultBuffer(dxCore.GetDevice(),
		dxCore.GetCommandList(), md.Indices32.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SpeckVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)md.Indices32.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds = bounds;

	geo->DrawArgs["speck"] = submesh;
	mGeometries["speckGeo"] = move(geo);
}

void SpeckApp::BuildRegularGeometry()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	GeometryGenerator geoGen;
	GeometryGenerator::StaticMeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);
	GeometryGenerator::StaticMeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::StaticMeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::StaticMeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount			= (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation	= boxIndexOffset;
	boxSubmesh.BaseVertexLocation	= boxVertexOffset;
	boxSubmesh.Bounds				= box.CalculateBounds();

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;
	gridSubmesh.Bounds = grid.CalculateBounds();

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;
	sphereSubmesh.Bounds = sphere.CalculateBounds();

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;
	cylinderSubmesh.Bounds = cylinder.CalculateBounds();

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	vector<GeometryGenerator::StaticVertex> vertices(totalVertexCount);
	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k] = box.Vertices[i];
	}
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k] = grid.Vertices[i];
	}
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k] = sphere.Vertices[i];
	}
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k] = cylinder.Vertices[i];
	}

	vector<uint16_t> indices;
	indices.insert(indices.end(), begin(box.GetIndices16()), end(box.GetIndices16()));
	indices.insert(indices.end(), begin(grid.GetIndices16()), end(grid.GetIndices16()));
	indices.insert(indices.end(), begin(sphere.GetIndices16()), end(sphere.GetIndices16()));
	indices.insert(indices.end(), begin(cylinder.GetIndices16()), end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(GeometryGenerator::StaticVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	auto geo = make_unique<MeshGeometry>();

	THROW_IF_FAILED(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	THROW_IF_FAILED(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(dxCore.GetDevice(),
		dxCore.GetCommandList(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = CreateDefaultBuffer(dxCore.GetDevice(),
		dxCore.GetCommandList(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(GeometryGenerator::StaticVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	mGeometries["shapeGeo"] = move(geo);
}

void SpeckApp::BuildPSOs()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	auto &sWorld = static_cast<SpeckWorld &>(GetWorld());
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	//
	// Load CS shaders
	//
	UINT resArray[] = {
		RT_SPECK_VS, RT_SPECK_PS,
		RT_DEFFERED_ASSEMBLER_VS, RT_DEFFERED_ASSEMBLER_PS,
		RT_SSAO_VS, RT_SSAO_PS, RT_SSAO_BLUR_PS,
		RT_STATIC_MESH_VS, RT_STATIC_MESH_PS, RT_SKINNED_MESH_VS };
	char *keyArray[] = {
		"speckVS", "speckPS",
		"defferedAssemblerVS", "defferedAssemblerPS",
		"SSAOVS", "SSAOPS", "SSAOBlurPS",
		"staticMeshVS", "staticMeshPS", "skinnedMeshVS" };
	const UINT numOfShaders = _countof(resArray);
	HMODULE hMod = LoadLibraryEx(ENGINE_LIBRARY_NAME, NULL, LOAD_LIBRARY_AS_DATAFILE);
	HRSRC hRes;
	if (NULL != hMod)
	{
		for (UINT i = 0; i < numOfShaders; i++)
		{
			hRes = FindResource(hMod, MAKEINTRESOURCE(ID_CSO), MAKEINTRESOURCE(resArray[i]));
			if (NULL != hRes)
			{
				HGLOBAL hgbl = LoadResource(hMod, hRes);
				void *pData = LockResource(hgbl);
				UINT32 sizeInBytes = SizeofResource(hMod, hRes);
				ComPtr<ID3DBlob> temp;
				THROW_IF_FAILED(D3DCreateBlob(sizeInBytes, temp.GetAddressOf()));
				memcpy((char*)temp->GetBufferPointer(), pData, sizeInBytes);
				mShaders[keyArray[i]] = temp;
			}
			else
			{
				LOG("Shader loading unsuccessful.", ERROR);
			}
		}
		FreeLibrary(hMod);
	}

	vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

	// PSO for post process.
	{
		ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		psoDesc.pRootSignature = mPostProcessRootSignature.Get();
		psoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["defferedAssemblerVS"]->GetBufferPointer()), mShaders["defferedAssemblerVS"]->GetBufferSize()
		};
		psoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["defferedAssemblerPS"]->GetBufferPointer()), mShaders["defferedAssemblerPS"]->GetBufferSize()
		};
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = dxCore.GetBackBufferFormat();
		psoDesc.SampleDesc.Count = dxCore.Get4xMsaaState() ? 4 : 1;
		psoDesc.SampleDesc.Quality = dxCore.Get4xMsaaState() ? (dxCore.Get4xMsaaQuality() - 1) : 0;
		psoDesc.DSVFormat = dxCore.GetDepthStencilFormat();
		THROW_IF_FAILED(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPostProcessPSO)));
	}

	// PSO for SSAO.
	{
		ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		psoDesc.pRootSignature = mSSAORootSignature.Get();
		psoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["SSAOVS"]->GetBufferPointer()), mShaders["SSAOVS"]->GetBufferSize()
		};
		psoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["SSAOPS"]->GetBufferPointer()), mShaders["SSAOPS"]->GetBufferSize()
		};
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = mSSAOBufferFormat;
		psoDesc.SampleDesc.Count = dxCore.Get4xMsaaState() ? 4 : 1;
		psoDesc.SampleDesc.Quality = dxCore.Get4xMsaaState() ? (dxCore.Get4xMsaaQuality() - 1) : 0;
		psoDesc.DSVFormat = dxCore.GetDepthStencilFormat();
		THROW_IF_FAILED(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mSSAOPSO)));
	}

	// PSO for Blurred SSAO.
	{
		ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		psoDesc.pRootSignature = mSSAORootSignature.Get();
		psoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["SSAOVS"]->GetBufferPointer()), mShaders["SSAOVS"]->GetBufferSize()
		};
		psoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["SSAOBlurPS"]->GetBufferPointer()), mShaders["SSAOBlurPS"]->GetBufferSize()
		};
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = mSSAOBufferFormat;
		psoDesc.SampleDesc.Count = dxCore.Get4xMsaaState() ? 4 : 1;
		psoDesc.SampleDesc.Quality = dxCore.Get4xMsaaState() ? (dxCore.Get4xMsaaQuality() - 1) : 0;
		psoDesc.DSVFormat = dxCore.GetDepthStencilFormat();
		THROW_IF_FAILED(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mBlurredSSAOPSO)));
	}

	// PSO for instanced objects.
	{
		ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		inputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		psoDesc.pRootSignature = mRootSignature.Get();
		psoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["speckVS"]->GetBufferPointer()), mShaders["speckVS"]->GetBufferSize()
		};
		psoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["speckPS"]->GetBufferPointer()), mShaders["speckPS"]->GetBufferSize()
		};
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = DEFERRED_RENDER_TARGETS_COUNT;
		for (int i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; ++i)
		{
			psoDesc.RTVFormats[i] = mDeferredRTFormats[i];
		}
		psoDesc.SampleDesc.Count = dxCore.Get4xMsaaState() ? 4 : 1;
		psoDesc.SampleDesc.Quality = dxCore.Get4xMsaaState() ? (dxCore.Get4xMsaaQuality() - 1) : 0;
		psoDesc.DSVFormat = dxCore.GetDepthStencilFormat();
		THROW_IF_FAILED(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sWorld.mPSOGroups["instanced"]->mPSO)));
	}

	// PSO for static mesh objects.
	{
		ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		GeometryGenerator::StaticVertex::GetInputLayout(&inputLayout);
		psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		psoDesc.pRootSignature = mRootSignature.Get();
		psoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["staticMeshVS"]->GetBufferPointer()), mShaders["staticMeshVS"]->GetBufferSize()
		};
		psoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["staticMeshPS"]->GetBufferPointer()), mShaders["staticMeshPS"]->GetBufferSize()
		};
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = DEFERRED_RENDER_TARGETS_COUNT;
		for (int i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; ++i)
		{
			psoDesc.RTVFormats[i] = mDeferredRTFormats[i];
		}
		psoDesc.SampleDesc.Count = dxCore.Get4xMsaaState() ? 4 : 1;
		psoDesc.SampleDesc.Quality = dxCore.Get4xMsaaState() ? (dxCore.Get4xMsaaQuality() - 1) : 0;
		psoDesc.DSVFormat = dxCore.GetDepthStencilFormat();
		THROW_IF_FAILED(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sWorld.mPSOGroups["static"]->mPSO)));
	}

	// PSO for skinned mesh objects (skeletal body).
	{
		ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		GeometryGenerator::SkinnedVertex::GetInputLayout(&inputLayout);
		psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		psoDesc.pRootSignature = mRootSignature.Get();
		psoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["skinnedMeshVS"]->GetBufferPointer()), mShaders["skinnedMeshVS"]->GetBufferSize()
		};
		psoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["staticMeshPS"]->GetBufferPointer()), mShaders["staticMeshPS"]->GetBufferSize()
		};
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = DEFERRED_RENDER_TARGETS_COUNT;
		for (int i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; ++i)
		{
			psoDesc.RTVFormats[i] = mDeferredRTFormats[i];
		}
		psoDesc.SampleDesc.Count = dxCore.Get4xMsaaState() ? 4 : 1;
		psoDesc.SampleDesc.Quality = dxCore.Get4xMsaaState() ? (dxCore.Get4xMsaaQuality() - 1) : 0;
		psoDesc.DSVFormat = dxCore.GetDepthStencilFormat();
		THROW_IF_FAILED(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sWorld.mPSOGroups["skeletalBody"]->mPSO)));
	}
}

void SpeckApp::BuildFrameResources()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	auto &sWorld = static_cast<SpeckWorld &>(GetWorld());
	for (int i = 0; i < NUM_FRAME_RESOURCES; ++i)
	{
		mFrameResources.push_back(make_unique<FrameResource>(dxCore.GetDevice(), 1, sWorld.mMaxRenderItemsCount, mMaxNumberOfMaterials));
	}
}

void SpeckApp::BuildDeferredRenderTargetsAndBuffers()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	auto device = GetEngineCore().GetDirectXCore().GetDevice();

	// Release the previous resources we will be recreating.
	for (int i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; ++i)
		mDeferredRTBs[i].Reset();
	mDeferredDepthStencilBuffer.Reset();

	// For supersampling
	UINT width = dxCore.GetClientWidth() * mSuperSampling;
	UINT height = dxCore.GetClientHeight() * mSuperSampling;
	mDeferredRTViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mDeferredRTScissorRect = { 0, 0, (int)width, (int)height };

	// Build the texture resources
	// Create the depth/stencil buffer.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = dxCore.GetDepthStencilFormat();
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE optClearDVB;
	optClearDVB.Format = GetEngineCore().GetDirectXCore().GetDepthStencilFormat();
	optClearDVB.DepthStencil.Depth = 1.0f;
	optClearDVB.DepthStencil.Stencil = 0;
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClearDVB,
		IID_PPV_ARGS(mDeferredDepthStencilBuffer.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHeapHandle(dxCore.Get_CPU_DSV_HeapStart());
	dsvHeapHandle.Offset(1, dxCore.GetDsvDescriptorSize()); // skip the main DSV descriptor
	mDeferredDSVHeapHandle = dsvHeapHandle;
	device->CreateDepthStencilView(mDeferredDepthStencilBuffer.Get(), nullptr, dsvHeapHandle);
	// Transition the resource from its initial state to be used as a depth buffer.
	GetEngineCore().GetDirectXCore().GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDeferredDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Define the formats
	mDeferredRTFormats[0] = dxCore.GetBackBufferFormat();		// diffuse map format
	mDeferredRTFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;		// normal map format
	mDeferredRTFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;		// normal view space map format
	mDeferredRTFormats[3] = DXGI_FORMAT_R32_FLOAT;				// depth map format
	mDeferredRTFormats[4] = DXGI_FORMAT_R8G8B8A8_UNORM;			// PBR data map format

	// Define the clear colors.
	mDeferredRTClearColors[0] = dxCore.GetClearRTColor();	// diffuse map color
	mDeferredRTClearColors[1] = DirectX::Colors::Black;		// normal map color
	mDeferredRTClearColors[2] = DirectX::Colors::Black;		// normal view space map color
	mDeferredRTClearColors[3] = DirectX::Colors::Red;		// depth map color
	mDeferredRTClearColors[4] = DirectX::Colors::Black;		// PBR data map color

	D3D12_CLEAR_VALUE optClear;

	// Diffuse albedo render-to-texture creation.
	D3D12_RESOURCE_DESC diffuseTexDesc;
	ZeroMemory(&diffuseTexDesc, sizeof(D3D12_RESOURCE_DESC));
	diffuseTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	diffuseTexDesc.Alignment = 0;
	diffuseTexDesc.Width = width;
	diffuseTexDesc.Height = height;
	diffuseTexDesc.DepthOrArraySize = 1;
	diffuseTexDesc.MipLevels = 0;
	diffuseTexDesc.Format = mDeferredRTFormats[DEFERRED_RENDER_TARGET_DIFFUSE_MAP];
	diffuseTexDesc.SampleDesc.Count = 1;
	diffuseTexDesc.SampleDesc.Quality = 0;
	diffuseTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	diffuseTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	optClear.Format = diffuseTexDesc.Format;
	memcpy(optClear.Color, mDeferredRTClearColors[DEFERRED_RENDER_TARGET_DIFFUSE_MAP], sizeof(XMVECTORF32));
	GetEngineCore().GetDirectXCore().GetClearRTColor(optClear.Color);
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&diffuseTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mDeferredRTBs[DEFERRED_RENDER_TARGET_DIFFUSE_MAP])));

	// Normal render-to-texture creation.
	D3D12_RESOURCE_DESC normalTexDesc;
	ZeroMemory(&normalTexDesc, sizeof(D3D12_RESOURCE_DESC));
	normalTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	normalTexDesc.Alignment = 0;
	normalTexDesc.Width = width;
	normalTexDesc.Height = height;
	normalTexDesc.DepthOrArraySize = 1;
	normalTexDesc.MipLevels = 0;
	normalTexDesc.Format = mDeferredRTFormats[DEFERRED_RENDER_TARGET_NORMAL_MAP];
	normalTexDesc.SampleDesc.Count = 1;
	normalTexDesc.SampleDesc.Quality = 0;
	normalTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	normalTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	optClear.Format = normalTexDesc.Format;
	memcpy(optClear.Color, mDeferredRTClearColors[DEFERRED_RENDER_TARGET_NORMAL_MAP], sizeof(XMVECTORF32));
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&normalTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mDeferredRTBs[DEFERRED_RENDER_TARGET_NORMAL_MAP])));

	// Normal view space render-to-texture creation.
	D3D12_RESOURCE_DESC normalViewSpaceTexDesc;
	ZeroMemory(&normalViewSpaceTexDesc, sizeof(D3D12_RESOURCE_DESC));
	normalViewSpaceTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	normalViewSpaceTexDesc.Alignment = 0;
	normalViewSpaceTexDesc.Width = width;
	normalViewSpaceTexDesc.Height = height;
	normalViewSpaceTexDesc.DepthOrArraySize = 1;
	normalViewSpaceTexDesc.MipLevels = 0;
	normalViewSpaceTexDesc.Format = mDeferredRTFormats[DEFERRED_RENDER_TARGET_NORMAL_VIEW_SPACE_MAP];
	normalViewSpaceTexDesc.SampleDesc.Count = 1;
	normalViewSpaceTexDesc.SampleDesc.Quality = 0;
	normalViewSpaceTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	normalViewSpaceTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	optClear.Format = normalViewSpaceTexDesc.Format;
	memcpy(optClear.Color, mDeferredRTClearColors[DEFERRED_RENDER_TARGET_NORMAL_VIEW_SPACE_MAP], sizeof(XMVECTORF32));
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&normalViewSpaceTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mDeferredRTBs[DEFERRED_RENDER_TARGET_NORMAL_VIEW_SPACE_MAP])));

	// Depth render-to-texture creation.
	D3D12_RESOURCE_DESC depthTexDesc;
	ZeroMemory(&depthTexDesc, sizeof(D3D12_RESOURCE_DESC));
	depthTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthTexDesc.Alignment = 0;
	depthTexDesc.Width = width;
	depthTexDesc.Height = height;
	depthTexDesc.DepthOrArraySize = 1;
	depthTexDesc.MipLevels = 0;
	depthTexDesc.Format = mDeferredRTFormats[DEFERRED_RENDER_TARGET_DEPTH_MAP];
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	optClear.Format = depthTexDesc.Format;
	memcpy(optClear.Color, mDeferredRTClearColors[DEFERRED_RENDER_TARGET_DEPTH_MAP], sizeof(XMVECTORF32));
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mDeferredRTBs[DEFERRED_RENDER_TARGET_DEPTH_MAP])));

	// PBR data render-to-texture creation.
	D3D12_RESOURCE_DESC pbrDataTexDesc;
	ZeroMemory(&pbrDataTexDesc, sizeof(D3D12_RESOURCE_DESC));
	pbrDataTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	pbrDataTexDesc.Alignment = 0;
	pbrDataTexDesc.Width = width;
	pbrDataTexDesc.Height = height;
	pbrDataTexDesc.DepthOrArraySize = 1;
	pbrDataTexDesc.MipLevels = 0;
	pbrDataTexDesc.Format = mDeferredRTFormats[DEFERRED_RENDER_TARGET_METALNESS_MAP];
	pbrDataTexDesc.SampleDesc.Count = 1;
	pbrDataTexDesc.SampleDesc.Quality = 0;
	pbrDataTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	pbrDataTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	optClear.Format = pbrDataTexDesc.Format;
	memcpy(optClear.Color, mDeferredRTClearColors[DEFERRED_RENDER_TARGET_METALNESS_MAP], sizeof(XMVECTORF32));
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&pbrDataTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mDeferredRTBs[DEFERRED_RENDER_TARGET_METALNESS_MAP])));

	// Render target views
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(dxCore.Get_CPU_RTV_HeapStart());
	rtvHeapHandle.Offset(SWAP_CHAIN_BUFFER_COUNT, dxCore.GetRtvDescriptorSize()); // skip the main RTV descriptors
	mDeferredRTVHeapHandle = rtvHeapHandle;
	for (UINT i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; i++)
	{
		device->CreateRenderTargetView(mDeferredRTBs[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, dxCore.GetRtvDescriptorSize());
	}
}

void SpeckApp::BuildSSAORenderTargetsAndBuffers()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	auto device = GetEngineCore().GetDirectXCore().GetDevice();

	// Release the previous resources we will be recreating.
	mSSAOBuffer[0].Reset();
	mSSAOBuffer[1].Reset();

	// Dimensions
	mSSAOBufferWidth = (UINT)(dxCore.GetClientWidth() * 0.5f);
	mSSAOBufferHeight = (UINT)(dxCore.GetClientHeight() * 0.5f);
	mSSAORTViewport = { 0.0f, 0.0f, (float)mSSAOBufferWidth, (float)mSSAOBufferHeight, 0.0f, 1.0f };
	mSSAORTScissorRect = { 0, 0, (int)mSSAOBufferWidth, (int)mSSAOBufferHeight };

	// Define the formats and colors
	mSSAOBufferFormat = DXGI_FORMAT_R8_UNORM;
	mSSAOBufferClearColor = DirectX::Colors::White;

	// Diffuse albedo render-to-texture creation.
	D3D12_RESOURCE_DESC diffuseTexDesc;
	ZeroMemory(&diffuseTexDesc, sizeof(D3D12_RESOURCE_DESC));
	diffuseTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	diffuseTexDesc.Alignment = 0;
	diffuseTexDesc.Width = mSSAOBufferWidth;
	diffuseTexDesc.Height = mSSAOBufferHeight;
	diffuseTexDesc.DepthOrArraySize = 1;
	diffuseTexDesc.MipLevels = 0;
	diffuseTexDesc.Format = mSSAOBufferFormat;
	diffuseTexDesc.SampleDesc.Count = 1;
	diffuseTexDesc.SampleDesc.Quality = 0;
	diffuseTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	diffuseTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE optClear1;
	optClear1.Format = diffuseTexDesc.Format;
	memcpy(optClear1.Color, mSSAOBufferClearColor, sizeof(XMVECTORF32));
	// the first
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&diffuseTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear1,
		IID_PPV_ARGS(&mSSAOBuffer[0])));
	// the second
	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&diffuseTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear1,
		IID_PPV_ARGS(&mSSAOBuffer[1])));

	// Render target views
	// the first
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(dxCore.Get_CPU_RTV_HeapStart());
	rtvHeapHandle.Offset(SWAP_CHAIN_BUFFER_COUNT + DEFERRED_RENDER_TARGETS_COUNT, dxCore.GetRtvDescriptorSize()); // skip the swap chain and deferred RTV descriptors
	mSSAORTVHeapHandle[0] = rtvHeapHandle;
	device->CreateRenderTargetView(mSSAOBuffer[0].Get(), nullptr, rtvHeapHandle);
	// the second
	rtvHeapHandle.Offset(1, dxCore.GetRtvDescriptorSize()); // skip the first
	mSSAORTVHeapHandle[1] = rtvHeapHandle;
	device->CreateRenderTargetView(mSSAOBuffer[1].Get(), nullptr, rtvHeapHandle);
}

void SpeckApp::BuildRandomVectorBuffer()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	auto device = GetEngineCore().GetDirectXCore().GetDevice();

	// Release the previous resources we will be recreating.
	mRandomVectorMap.Reset();
	mRandomVectorMapUploadBuffer.Reset();

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 256;
	texDesc.Height = 256;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mRandomVectorMap)));

	//
	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. 
	//

	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap.Get(), 0, num2DSubresources);

	THROW_IF_FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mRandomVectorMapUploadBuffer.GetAddressOf())));

	XMCOLOR initData[256 * 256];
	for (int i = 0; i < 256; ++i)
	{
		for (int j = 0; j < 256; ++j)
		{
			// Random vector in [0,1].  We will decompress in shader to [-1,1].
			XMFLOAT3 v(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());

			initData[i * 256 + j] = DirectX::PackedVector::XMCOLOR(v.x, v.y, v.z, 0.0f);
		}
	}

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = 256 * sizeof(XMCOLOR);
	subResourceData.SlicePitch = subResourceData.RowPitch * 256;

	//
	// Schedule to copy the data to the default resource, and change states.
	// Note that mCurrSol is put in the GENERIC_READ state so it can be 
	// read by a shader.
	//

	dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(dxCore.GetCommandList(), mRandomVectorMap.Get(), mRandomVectorMapUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
	dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SpeckApp::BuildSRVs()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();

	//	SSAO
	{
		// create the heap and populate it
		mSSAOSRVDescriptorHeap.Reset();
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDescSSAO = {};
		srvHeapDescSSAO.NumDescriptors = SSAO_INPUT_TEXTURE_COUNT + 2; // + 2 for ping pong 'previous result' textures
		srvHeapDescSSAO.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDescSSAO.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		THROW_IF_FAILED(dxCore.GetDevice()->CreateDescriptorHeap(&srvHeapDescSSAO, IID_PPV_ARGS(&mSSAOSRVDescriptorHeap)));
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptorCPU_SSAO(mSSAOSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptorGPU_SSAO(mSSAOSRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		mSSAOTextureMapsSRVHeapHandle = hDescriptorGPU_SSAO;

		// Fill out the heap with normal view space buffer SRV descriptors
		dxCore.GetDevice()->CreateShaderResourceView(mDeferredRTBs[DEFERRED_RENDER_TARGET_NORMAL_VIEW_SPACE_MAP].Get(), nullptr, hDescriptorCPU_SSAO);
		hDescriptorCPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
		hDescriptorGPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());

		// Fill out the heap with depth buffer SRV descriptors
		dxCore.GetDevice()->CreateShaderResourceView(mDeferredRTBs[DEFERRED_RENDER_TARGET_DEPTH_MAP].Get(), nullptr, hDescriptorCPU_SSAO);
		hDescriptorCPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
		hDescriptorGPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());

		// Fill out the heap with random vector buffer SRV descriptors
		dxCore.GetDevice()->CreateShaderResourceView(mRandomVectorMap.Get(), nullptr, hDescriptorCPU_SSAO);
		hDescriptorCPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
		hDescriptorGPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());

		// Fill out the heap with the first previous result texure buffer SRV descriptors
		dxCore.GetDevice()->CreateShaderResourceView(mSSAOBuffer[0].Get(), nullptr, hDescriptorCPU_SSAO);
		mSSAOPreviousResultSRVHeapHandle[0] = hDescriptorGPU_SSAO;
		hDescriptorCPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
		hDescriptorGPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());

		// Fill out the heap with the second previous result texure buffer SRV descriptors
		dxCore.GetDevice()->CreateShaderResourceView(mSSAOBuffer[1].Get(), nullptr, hDescriptorCPU_SSAO);
		mSSAOPreviousResultSRVHeapHandle[1] = hDescriptorGPU_SSAO;
		hDescriptorCPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
		hDescriptorGPU_SSAO.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
	}

	//	Post-process
	{
		mPostProcessSrvDescriptorHeap.Reset();
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDescPostProcess = {};
		srvHeapDescPostProcess.NumDescriptors = POST_PROCESS_INPUT_TEXTURE_COUNT;
		srvHeapDescPostProcess.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDescPostProcess.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		THROW_IF_FAILED(dxCore.GetDevice()->CreateDescriptorHeap(&srvHeapDescPostProcess, IID_PPV_ARGS(&mPostProcessSrvDescriptorHeap)));

		// Fill out the heap with deferred SRV descriptors for post process.
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptorPostProcess(mPostProcessSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; i++)
		{
			dxCore.GetDevice()->CreateShaderResourceView(mDeferredRTBs[i].Get(), nullptr, hDescriptorPostProcess);
			hDescriptorPostProcess.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
		}

		// Fill out the heap with SSAO SRV descriptors for post process.
		dxCore.GetDevice()->CreateShaderResourceView(mSSAOBuffer[1].Get(), nullptr, hDescriptorPostProcess);
		hDescriptorPostProcess.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
	}
}
