
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

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

void Speck::CreateSpeckApp(HINSTANCE hInstance, UINT maxNumberOfMaterials, UINT maxInstancedObject, UINT maxSingleObjects, unique_ptr<EngineCore> *ec, unique_ptr<World> *world, unique_ptr<App> *app)
{
	*ec = make_unique<EngineCore>();
	*world = make_unique<SpeckWorld>(maxInstancedObject, maxSingleObjects);
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

	// Deinitialize cube render target data.
	CubeRenderTarget::ReleaseStaticMembers();
}

bool SpeckApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	auto &dxCore = GetEngineCore().GetDirectXCore();
	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(dxCore.GetCommandList()->Reset(dxCore.GetCommandAllocator(), nullptr));

	BuildDefaultTextures();
	BuildRootSignatures();
	BuildScreenGeometry();
	BuildSpeckGeometry();
	BuildRegularGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Initialize cube render target data.
	CubeRenderTarget::BuildStaticMembers(dxCore.GetDevice(), dxCore.GetCommandList());

	// Execute the initialization commands.
	ThrowIfFailed(dxCore.GetCommandList()->Close());
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
	ThrowIfFailed(dxCore.GetCommandList()->Reset(dxCore.GetCommandAllocator(), nullptr));

	// Do the (re)initialization.
	BuildDeferredRenderTargetsAndBuffers();

	// Execute the resize commands.
	ThrowIfFailed(dxCore.GetCommandList()->Close());
	ID3D12CommandList* cmdsLists[] = { dxCore.GetCommandList() };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	GetEngineCore().GetDirectXCore().FlushCommandQueue();
}

void SpeckApp::Update(const GameTimer& gt)
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
		ThrowIfFailed(dxCore.GetFence()->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	GetWorld().Update(this);
	UpdateMaterialBuffer(gt);
}

void SpeckApp::Draw(const GameTimer& gt)
{
	D3DApp::Draw(gt);
	auto &dxCore = GetEngineCore().GetDirectXCore();
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(dxCore.GetCommandList()->Reset(cmdListAlloc.Get(), nullptr));

	//
	// Draw the scene
	//
	dxCore.GetCommandList()->RSSetViewports(1, &mDeferredRTViewport);
	dxCore.GetCommandList()->RSSetScissorRects(1, &mDeferredRTScissorRect);

	// Indicate a state transition on the resource usage and also clear the back buffer.
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = mDeferredRTVHeapHandle;
	for (UINT i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; i++)
	{
		dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDeferredRTBs[i].Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		dxCore.GetCommandList()->ClearRenderTargetView(rtvHandle, dxCore.GetClearRTColor(), 0, nullptr);
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
	dxCore.GetCommandList()->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());

	// Bind the main pass constant buffer.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	dxCore.GetCommandList()->SetGraphicsRootConstantBufferView(4, passCB->GetGPUVirtualAddress());

	// Drawing
	GetWorld().Draw(this, 0);

	// Indicate a state transition on the resource usage.
	for (UINT i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; i++)
	{
		dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDeferredRTBs[i].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
	}

	//
	// Draw the screen quad (postprocess)
	//
	dxCore.GetCommandList()->RSSetViewports(1, &dxCore.GetScreenViewport());
	dxCore.GetCommandList()->RSSetScissorRects(1, &dxCore.GetScissorRect());

	// Indicate a state transition on the resource usage.
	dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	dxCore.GetCommandList()->ClearRenderTargetView(CurrentBackBufferView(), dxCore.GetClearRTColor(), 0, nullptr);
	dxCore.GetCommandList()->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	dxCore.GetCommandList()->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());


	ID3D12DescriptorHeap* descriptorHeaps[] = { mPostProcessSrvDescriptorHeap.Get() };
	dxCore.GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	dxCore.GetCommandList()->SetGraphicsRootSignature(mPostProcessRootSignature.Get());
	dxCore.GetCommandList()->SetPipelineState(mScreenObjPSO.Get());

	// Upload the constants
	UINT values[] = { 
		(UINT)dxCore.GetClientWidth() * mSuperSampling, (UINT)dxCore.GetClientHeight() * mSuperSampling,
		(UINT)dxCore.GetClientWidth(), (UINT)dxCore.GetClientHeight() };
	dxCore.GetCommandList()->SetGraphicsRoot32BitConstants(0, 4, &values, 0);

	// Bind all the pre rendered scene texture components used in this post process pass.
	dxCore.GetCommandList()->SetGraphicsRootDescriptorTable(1, mPostProcessSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	auto geo = mGeometries["screenGeo"].get();
	dxCore.GetCommandList()->IASetVertexBuffers(0, 1, &geo->VertexBufferView());
	dxCore.GetCommandList()->IASetIndexBuffer(&geo->IndexBufferView());
	dxCore.GetCommandList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	auto &subMesh = geo->DrawArgs["quad"];
	dxCore.GetCommandList()->DrawIndexedInstanced(subMesh.IndexCount, 1, subMesh.StartIndexLocation, subMesh.BaseVertexLocation, 0);

	// Indicate a state transition on the resource usage.
	dxCore.GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//
	// Done recording commands.
	//
	ThrowIfFailed(dxCore.GetCommandList()->Close());
	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { dxCore.GetCommandList() };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	// Swap the back and front buffers
	ThrowIfFailed(dxCore.GetSwapChain()->Present(0, 0));
	UpdateBackBuffer();

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = IncrementCurrentFence();
	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	dxCore.GetCommandQueue()->Signal(dxCore.GetFence(), dxCore.GetCurrentFence());
}

void SpeckApp::UpdateMaterialBuffer(const GameTimer& gt)
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

	auto defaultAlbedo = std::make_unique<Texture>();
	defaultAlbedo->Name = "defaultAlbedo";
	BuildColorTexture({1.0f, 1.0f, 1.0f, 1.0f}, dxCore.GetDevice(), dxCore.GetCommandList(), defaultAlbedo->Resource, defaultAlbedo->UploadHeap);

	auto defaultNormal = std::make_unique<Texture>();
	defaultNormal->Name = "defaultNormal";
	BuildColorTexture({ 128.0f / 255.0f, 128.0f / 255.0f, 1.0f, 1.0f }, dxCore.GetDevice(), dxCore.GetCommandList(), defaultNormal->Resource, defaultNormal->UploadHeap);

	auto defaultHeight = std::make_unique<Texture>();
	defaultHeight->Name = "defaultHeight";
	BuildColorTexture({ 0.5f, 0.5f, 0.5f, 1.0f }, dxCore.GetDevice(), dxCore.GetCommandList(), defaultHeight->Resource, defaultHeight->UploadHeap);

	auto defaultMetalness = std::make_unique<Texture>();
	defaultMetalness->Name = "defaultMetalness";
	BuildColorTexture({ 0.5f, 0.5f, 0.5f, 1.0f }, dxCore.GetDevice(), dxCore.GetCommandList(), defaultMetalness->Resource, defaultMetalness->UploadHeap);

	auto defaultRough = std::make_unique<Texture>();
	defaultRough->Name = "defaultRough";
	BuildColorTexture({ 0.5f, 0.5f, 0.5f, 1.0f }, dxCore.GetDevice(), dxCore.GetCommandList(), defaultRough->Resource, defaultRough->UploadHeap);

	auto defaultAO = std::make_unique<Texture>();
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
	mTextures[defaultAlbedo->Name] = std::move(defaultAlbedo);
	mTextures[defaultNormal->Name] = std::move(defaultNormal);
	mTextures[defaultHeight->Name] = std::move(defaultHeight);
	mTextures[defaultMetalness->Name] = std::move(defaultMetalness);
	mTextures[defaultRough->Name] = std::move(defaultRough);
	mTextures[defaultAO->Name] = std::move(defaultAO);
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
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];
	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);											// root descriptor (for single objects)
	slotRootParameter[1].InitAsShaderResourceView(0, 1);										// root descriptor (for object instances)
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);	// descriptor table (for textures)
	slotRootParameter[3].InitAsShaderResourceView(1, 1);										// root descriptor (for materials)
	slotRootParameter[4].InitAsConstantBufferView(1);											// root descriptor (for pass buffer)

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
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
	ThrowIfFailed(hr);

	ThrowIfFailed(dxCore.GetDevice()->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));

	//
	// Post process signature
	//
	CD3DX12_DESCRIPTOR_RANGE texTablePostProcess;
	texTablePostProcess.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, DEFERRED_RENDER_TARGETS_COUNT, 0, 0);
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameterPostProcess[2];
	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameterPostProcess[0].InitAsConstants(4, 0, 0);														// root constant (for single objects)
	slotRootParameterPostProcess[1].InitAsDescriptorTable(1, &texTablePostProcess, D3D12_SHADER_VISIBILITY_PIXEL);	// descriptor table (for textures)

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER); // addressW

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDescPostProcess(2, slotRootParameterPostProcess, 1, &pointClamp,
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
	ThrowIfFailed(hr);

	ThrowIfFailed(dxCore.GetDevice()->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mPostProcessRootSignature.GetAddressOf())));
}

void SpeckApp::BuildScreenGeometry()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	GeometryGenerator gg;
	GeometryGenerator::MeshData md = gg.CreateQuad(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f);

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
	const UINT ibByteSize = (UINT)md.Indices32.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
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
	mGeometries["screenGeo"] = std::move(geo);
}

void SpeckApp::BuildSpeckGeometry()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	GeometryGenerator gg;
	GeometryGenerator::MeshData md = gg.CreateGeosphere(6.5f, 2);

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
	const UINT ibByteSize = (UINT)md.Indices32.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
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
	mGeometries["speckGeo"] = std::move(geo);
}

void SpeckApp::BuildRegularGeometry()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

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

	std::vector<GeometryGenerator::Vertex> vertices(totalVertexCount);
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

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(GeometryGenerator::Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(dxCore.GetDevice(),
		dxCore.GetCommandList(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = CreateDefaultBuffer(dxCore.GetDevice(),
		dxCore.GetCommandList(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(GeometryGenerator::Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	mGeometries["shapeGeo"] = std::move(geo);
}

void SpeckApp::BuildPSOs()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	auto &sWorld = static_cast<SpeckWorld &>(GetWorld());
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	// Load the shaders.
	mShaders["speckVS"] = LoadBinary(L"Data\\Shaders\\speckVS.cso");
	mShaders["speckPS"] = LoadBinary(L"Data\\Shaders\\speckPS.cso");
	mShaders["defferedAssemblerVS"] = LoadBinary(L"Data\\Shaders\\defferedAssemblerVS.cso");
	mShaders["defferedAssemblerPS"] = LoadBinary(L"Data\\Shaders\\defferedAssemblerPS.cso");
	mShaders["standardVS"] = LoadBinary(L"Data\\Shaders\\standardVS.cso");
	mShaders["standardPS"] = LoadBinary(L"Data\\Shaders\\standardPS.cso");

	//
	// PSO for screen quad.
	//
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
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
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = dxCore.GetBackBufferFormat();
	psoDesc.SampleDesc.Count = dxCore.Get4xMsaaState() ? 4 : 1;
	psoDesc.SampleDesc.Quality = dxCore.Get4xMsaaState() ? (dxCore.Get4xMsaaQuality() - 1) : 0;
	psoDesc.DSVFormat = dxCore.GetDepthStencilFormat();
	ThrowIfFailed(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mScreenObjPSO)));

	//
	// PSO for instanced objects.
	//
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
		psoDesc.RTVFormats[i] = dxCore.GetBackBufferFormat();
	}
	psoDesc.SampleDesc.Count = dxCore.Get4xMsaaState() ? 4 : 1;
	psoDesc.SampleDesc.Quality = dxCore.Get4xMsaaState() ? (dxCore.Get4xMsaaQuality() - 1) : 0;
	psoDesc.DSVFormat = dxCore.GetDepthStencilFormat();
	ThrowIfFailed(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sWorld.mPSOGroups["instanced"]->mPSO)));
}

void SpeckApp::BuildFrameResources()
{
	auto &dxCore = GetEngineCore().GetDirectXCore();
	auto &sWorld = static_cast<SpeckWorld &>(GetWorld());
	for (int i = 0; i < NUM_FRAME_RESOURCES; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(dxCore.GetDevice(), 1, sWorld.mMaxInstancedObject, sWorld.mMaxSingleObjects, mMaxNumberOfMaterials));
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
	ThrowIfFailed(device->CreateCommittedResource(
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

	// Diffuse albedo render-to-texture creation.
	D3D12_RESOURCE_DESC diffuseTexDesc;
	ZeroMemory(&diffuseTexDesc, sizeof(D3D12_RESOURCE_DESC));
	diffuseTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	diffuseTexDesc.Alignment = 0;
	diffuseTexDesc.Width = width;
	diffuseTexDesc.Height = height;
	diffuseTexDesc.DepthOrArraySize = 1;
	diffuseTexDesc.MipLevels = 0;
	diffuseTexDesc.Format = dxCore.GetBackBufferFormat();
	diffuseTexDesc.SampleDesc.Count = 1;
	diffuseTexDesc.SampleDesc.Quality = 0;
	diffuseTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	diffuseTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE optClear1;
	optClear1.Format = dxCore.GetBackBufferFormat();
	GetEngineCore().GetDirectXCore().GetClearRTColor(optClear1.Color);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&diffuseTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear1,
		IID_PPV_ARGS(&mDeferredRTBs[0])));

	// Normal render-to-texture creation.
	D3D12_RESOURCE_DESC normalTexDesc;
	ZeroMemory(&normalTexDesc, sizeof(D3D12_RESOURCE_DESC));
	normalTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	normalTexDesc.Alignment = 0;
	normalTexDesc.Width = width;
	normalTexDesc.Height = height;
	normalTexDesc.DepthOrArraySize = 1;
	normalTexDesc.MipLevels = 0;
	normalTexDesc.Format = dxCore.GetBackBufferFormat();
	normalTexDesc.SampleDesc.Count = 1;
	normalTexDesc.SampleDesc.Quality = 0;
	normalTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	normalTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE optClear2;
	optClear2.Format = dxCore.GetBackBufferFormat();
	GetEngineCore().GetDirectXCore().GetClearRTColor(optClear2.Color);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&normalTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear2,
		IID_PPV_ARGS(&mDeferredRTBs[1])));

	// Depth render-to-texture creation.
	D3D12_RESOURCE_DESC depthTexDesc;
	ZeroMemory(&depthTexDesc, sizeof(D3D12_RESOURCE_DESC));
	depthTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthTexDesc.Alignment = 0;
	depthTexDesc.Width = width;
	depthTexDesc.Height = height;
	depthTexDesc.DepthOrArraySize = 1;
	depthTexDesc.MipLevels = 0;
	depthTexDesc.Format = dxCore.GetBackBufferFormat();
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE optClear3;
	optClear3.Format = dxCore.GetBackBufferFormat();
	GetEngineCore().GetDirectXCore().GetClearRTColor(optClear3.Color);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthTexDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear3,
		IID_PPV_ARGS(&mDeferredRTBs[2])));

	// Render target views
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(dxCore.Get_CPU_RTV_HeapStart());
	rtvHeapHandle.Offset(SWAP_CHAIN_BUFFER_COUNT, dxCore.GetRtvDescriptorSize()); // skip the main RTV descriptors
	mDeferredRTVHeapHandle = rtvHeapHandle;
	for (UINT i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; i++)
	{
		device->CreateRenderTargetView(mDeferredRTBs[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, dxCore.GetRtvDescriptorSize());
	}

	// Reset the heap if necessary and create new one for updated descriptors.
	mPostProcessSrvDescriptorHeap.Reset();
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDescPostProcess = {};
	srvHeapDescPostProcess.NumDescriptors = DEFERRED_RENDER_TARGETS_COUNT;
	srvHeapDescPostProcess.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDescPostProcess.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(dxCore.GetDevice()->CreateDescriptorHeap(&srvHeapDescPostProcess, IID_PPV_ARGS(&mPostProcessSrvDescriptorHeap)));

	// Fill out the heap with SRV descriptors for post process.
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptorPostProcess(mPostProcessSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; i++)
	{
		dxCore.GetDevice()->CreateShaderResourceView(mDeferredRTBs[i].Get(), nullptr, hDescriptorPostProcess);
		hDescriptorPostProcess.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
	}
}

void SpeckApp::BuildRenderItems()
{
	auto &sWorld = static_cast<SpeckWorld &>(GetWorld());
	//
	// Build the specks.
	//
	auto rItem = std::make_unique<InstancedRenderItem>();
	rItem->mGeo = mGeometries["speckGeo"].get();
	rItem->mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rItem->mInstanceCount = 0;
	rItem->mIndexCount = rItem->mGeo->DrawArgs["speck"].IndexCount;
	rItem->mStartIndexLocation = rItem->mGeo->DrawArgs["speck"].StartIndexLocation;
	rItem->mBaseVertexLocation = rItem->mGeo->DrawArgs["speck"].BaseVertexLocation;

	// Generate instance data.
	const int n = (int)pow(sWorld.mMaxInstancedObject, 1.0f/3.0f);
	sWorld.mMaxInstancedObject = n*n*n;
	rItem->mInstances.resize(sWorld.mMaxInstancedObject);

	float width = 200.0f;
	float height = 200.0f;
	float depth = 200.0f;

	float x = -0.5f*width;
	float y = -0.5f*height;
	float z = -0.5f*depth;
	float dx = width / (n - 1);
	float dy = height / (n - 1);
	float dz = depth / (n - 1);
	for (int k = 0; k < n; ++k)
	{
		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < n; ++j)
			{
				int index = k*n*n + i*n + j;
				// Position instanced along a 3D grid.
				rItem->mInstances[index].Position = XMFLOAT3(
					x + j*dx, y + i*dy, z + k*dz);
				rItem->mInstances[index].MaterialIndex = (i + j + k) % 3;
			}
		}
	}
	// Add to the render group.
	sWorld.mPSOGroups["instanced"]->mRItems.push_back(move(rItem));
}
