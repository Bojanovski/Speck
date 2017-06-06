
#include "CubeRenderTarget.h"
#include "EngineCore.h"
#include "DirectXCore.h"
#include "FrameResource.h"
#include "SpeckWorld.h"
#include "UploadBuffer.h"
#include "RandomGenerator.h"
#include "Resources.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

CubeCameraSet::CubeCameraSet(EngineCore &ec)
	: mCubeMapCamera{ ec, ec, ec, ec, ec, ec }
{

}

void CubeCameraSet::SetPosition(const XMFLOAT3 & pos)
{
	// Generate the cube map about the given position.
	XMFLOAT3 center(pos.x, pos.y, pos.z);
	XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);

	// Look along each coordinate axis.
	XMFLOAT3 targets[6] =
	{
		XMFLOAT3(pos.x + 1.0f, pos.y, pos.z), // +X
		XMFLOAT3(pos.x - 1.0f, pos.y, pos.z), // -X
		XMFLOAT3(pos.x, pos.y + 1.0f, pos.z), // +Y
		XMFLOAT3(pos.x, pos.y - 1.0f, pos.z), // -Y
		XMFLOAT3(pos.x, pos.y, pos.z + 1.0f), // +Z
		XMFLOAT3(pos.x, pos.y, pos.z - 1.0f)  // -Z
	};

	// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
	// are looking down +Y or -Y, so we need a different "up" vector.
	XMFLOAT3 ups[6] =
	{
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // +X
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // -X
		XMFLOAT3(0.0f, 0.0f, -1.0f), // +Y
		XMFLOAT3(0.0f, 0.0f, +1.0f), // -Y
		XMFLOAT3(0.0f, 1.0f, 0.0f),	 // +Z
		XMFLOAT3(0.0f, 1.0f, 0.0f)	 // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		mCubeMapCamera[i].LookAt(center, targets[i], ups[i]);
		mCubeMapCamera[i].SetLens(0.5f*XM_PI, 1.0f, 0.1f, 1000.0f);
		mCubeMapCamera[i].UpdateViewMatrix();
	}
}

// Initially set to null, before the CubeRenderTarget first constructor.
ComPtr<ID3D12RootSignature> CubeRenderTarget::mIrrMapCSRootSignature = nullptr;
ComPtr<ID3DBlob> CubeRenderTarget::mIrrMapCS = nullptr;
ComPtr<ID3D12PipelineState> CubeRenderTarget::mIrrMapCSPSO = nullptr;
ComPtr<ID3D12Resource> CubeRenderTarget::mSamplerRaysBuffer = nullptr;
ComPtr<ID3D12Resource> CubeRenderTarget::mSamplerRaysUploadBuffer = nullptr;

CubeRenderTarget::CubeRenderTarget(EngineCore &ec, World &world, UINT width, UINT height, const XMFLOAT3 &pos, DXGI_FORMAT format)
	: EngineUser(ec),
	WorldUser(world),
	mCCS(ec)
{
	auto device = GetEngineCore().GetDirectXCore().GetDevice();
	mWidth = width;
	mHeight = height;
	mFormat = format;
	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	// Initialize the cameras.
	SetPosition(pos);
	// Build pass constant buffer and fill it with data.
	BuildPassConstantBuffer();
	// New resource.
	BuildResource();
	// New descriptors to it.
	BuildDescriptors();
}

ID3D12Resource*  CubeRenderTarget::TexResource()
{
	return mCubeMapBuffer.Get();
}

ID3D12Resource * CubeRenderTarget::CBResource()
{
	mCurrPassCBIndex = (mCurrPassCBIndex + 1) % NUM_FRAME_RESOURCES;
	return mPassCB[mCurrPassCBIndex]->Resource();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CubeRenderTarget::Srv()
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(mCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
	return hDescriptor;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::Rtv(int faceIndex)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	hDescriptor.Offset(faceIndex, GetEngineCore().GetDirectXCore().GetRtvDescriptorSize());
	return hDescriptor;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::Dsv()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	return hDescriptor;
}

D3D12_VIEWPORT CubeRenderTarget::Viewport()const
{
	return mViewport;
}

D3D12_RECT CubeRenderTarget::ScissorRect()const
{
	return mScissorRect;
}

void CubeRenderTarget::UpdateCBs()
{
	auto speckWorld = static_cast<SpeckWorld *>(&GetWorld());

	// Get constant buffer uploader for each frame resource
	for (int i = 0; i < NUM_FRAME_RESOURCES; ++i)
	{
		auto passCB = mPassCB[i].get();
		// Fill with data each cube side's constant buffer
		for (int j = 0; j < 6; ++j)
		{
			PassConstants cubeFacePassCB = *speckWorld->GetPassConstants();

			XMMATRIX view = mCCS.mCubeMapCamera[j].GetView();
			XMMATRIX proj = mCCS.mCubeMapCamera[j].GetProj();

			XMMATRIX viewProj = XMMatrixMultiply(view, proj);
			XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
			XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
			XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

			XMStoreFloat4x4(&cubeFacePassCB.View, XMMatrixTranspose(view));
			XMStoreFloat4x4(&cubeFacePassCB.InvView, XMMatrixTranspose(invView));
			XMStoreFloat4x4(&cubeFacePassCB.Proj, XMMatrixTranspose(proj));
			XMStoreFloat4x4(&cubeFacePassCB.InvProj, XMMatrixTranspose(invProj));
			XMStoreFloat4x4(&cubeFacePassCB.ViewProj, XMMatrixTranspose(viewProj));
			XMStoreFloat4x4(&cubeFacePassCB.InvViewProj, XMMatrixTranspose(invViewProj));
			cubeFacePassCB.EyePosW = mCCS.mCubeMapCamera[j].GetPosition();
			cubeFacePassCB.RenderTargetSize = XMFLOAT2((float)mWidth, (float)mHeight);
			cubeFacePassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mWidth, 1.0f / mHeight);
			// Cube map pass cbuffers are stored in elements 1-6.
			passCB->CopyData(j, cubeFacePassCB);
		}
	}
}

void CubeRenderTarget::UpdateIrradianceMap()
{
	auto device = GetEngineCore().GetDirectXCore().GetDevice();
	auto cmdList = GetEngineCore().GetDirectXCore().GetCommandList();

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->SetComputeRootSignature(mIrrMapCSRootSignature.Get());
	cmdList->SetPipelineState(mIrrMapCSPSO.Get());

	UINT values[] = { mIrrWidth, mIrrHeight };
	cmdList->SetComputeRoot32BitConstants(0, 2, &values, 0);

	// mCubeMapBuffer should be in GENERIC_READ.
	// mCubeMapIrradianceBuffer should be in GENERIC_READ.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCubeMapIrradianceBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Bind the random generated numbers.
	CD3DX12_GPU_DESCRIPTOR_HANDLE h_CBV_SRV_UAV_Descriptor(mCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
	cmdList->SetComputeRootDescriptorTable(1, h_CBV_SRV_UAV_Descriptor);

	h_CBV_SRV_UAV_Descriptor.Offset(1, GetEngineCore().GetDirectXCore().GetCbvSrvUavDescriptorSize());
	cmdList->SetComputeRootDescriptorTable(2, h_CBV_SRV_UAV_Descriptor); // set SRV to the cube map

	h_CBV_SRV_UAV_Descriptor.Offset(1, GetEngineCore().GetDirectXCore().GetCbvSrvUavDescriptorSize());
	cmdList->SetComputeRootDescriptorTable(3, h_CBV_SRV_UAV_Descriptor); // set UAV to the irradiance map

	// How many groups do we need to dispatch to cover a row of pixels, where each
	// group covers 256 pixels (the 256 is defined in the ComputeShader).
	UINT numGroupsX = (UINT)ceilf(mIrrWidth / (float)CUBE_MAP_N_THREADS);
	UINT numGroupsZ = CUBE_MAP_SAMPLER_RAYS_TOTAL_COUNT / CUBE_MAP_SAMPLER_RAYS_GROUP_COUNT;
	cmdList->Dispatch(numGroupsX, mIrrHeight, numGroupsZ);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCubeMapIrradianceBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
}
 
void CubeRenderTarget::BuildDescriptors()
{
	auto device = GetEngineCore().GetDirectXCore().GetDevice();
	auto cmdList = GetEngineCore().GetDirectXCore().GetCommandList();
	// CBV, SRV and UAV heap
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 4;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(mCbvSrvUavHeap.GetAddressOf())));
	CD3DX12_CPU_DESCRIPTOR_HANDLE h_CBV_SRV_UAV_Descriptor(mCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
	// RTV heap
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = 6;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));
	// DSV heap
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

	// Create SRV to the sample rays buffer.
	D3D12_SHADER_RESOURCE_VIEW_DESC dsc;
	dsc.Format = DXGI_FORMAT_UNKNOWN;
	dsc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	dsc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	dsc.Buffer.FirstElement = 0;
	dsc.Buffer.NumElements = (UINT)CUBE_MAP_SAMPLER_RAYS_TOTAL_COUNT;
	dsc.Buffer.StructureByteStride = sizeof(XMFLOAT4);
	dsc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	device->CreateShaderResourceView(mSamplerRaysBuffer.Get(), &dsc, h_CBV_SRV_UAV_Descriptor);
	h_CBV_SRV_UAV_Descriptor.Offset(1, GetEngineCore().GetDirectXCore().GetCbvSrvUavDescriptorSize());

	// Create SRV to the entire cubemap resource (radiance map).
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	device->CreateShaderResourceView(mCubeMapBuffer.Get(), &srvDesc, h_CBV_SRV_UAV_Descriptor);
	h_CBV_SRV_UAV_Descriptor.Offset(1, GetEngineCore().GetDirectXCore().GetCbvSrvUavDescriptorSize());

	// Create UAV to the irradiance map.
	D3D12_UNORDERED_ACCESS_VIEW_DESC irrUavDesc = {};
	irrUavDesc.Format = mFormat;
	irrUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	irrUavDesc.Texture2D.MipSlice = 0;
	device->CreateUnorderedAccessView(mCubeMapIrradianceBuffer.Get(), nullptr, &irrUavDesc, h_CBV_SRV_UAV_Descriptor);
	h_CBV_SRV_UAV_Descriptor.Offset(1, GetEngineCore().GetDirectXCore().GetCbvSrvUavDescriptorSize());
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCubeMapIrradianceBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Create SRV to the irradiance map.
	D3D12_SHADER_RESOURCE_VIEW_DESC irrSrvDesc = {};
	irrSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	irrSrvDesc.Format = mFormat;
	irrSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	irrSrvDesc.Texture2D.MostDetailedMip = 0;
	irrSrvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(mCubeMapIrradianceBuffer.Get(), &irrSrvDesc, h_CBV_SRV_UAV_Descriptor);

	// Create RTV to each cube face.
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for(int i = 0; i < 6; ++i)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc; 
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = mFormat;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;

		// Render target to ith element.
		rtvDesc.Texture2DArray.FirstArraySlice = i;

		// Only view one element of the array.
		rtvDesc.Texture2DArray.ArraySize = 1;

		// Create RTV to ith cubemap face.
		device->CreateRenderTargetView(mCubeMapBuffer.Get(), &rtvDesc, hDescriptor);
		hDescriptor.Offset(1, GetEngineCore().GetDirectXCore().GetRtvDescriptorSize());
	}

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	device->CreateDepthStencilView(mCubeDepthStencilBuffer.Get(), nullptr, mDsvHeap->GetCPUDescriptorHandleForHeapStart());

	// Transition the resource from its initial state to be used as a depth buffer.
	GetEngineCore().GetDirectXCore().GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCubeDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
}

void CubeRenderTarget::BuildResource()
{
	// Note, compressed formats cannot be used for UAV.  We get error like:
	// ERROR: ID3D11Device::CreateTexture2D: The format (0x4d, BC3_UNORM) 
	// cannot be bound as an UnorderedAccessView, or cast to a format that
	// could be bound as an UnorderedAccessView.  Therefore this format 
	// does not support D3D11_BIND_UNORDERED_ACCESS.
	auto device = GetEngineCore().GetDirectXCore().GetDevice();
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 6;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE optClear1;
	optClear1.Format = mFormat;
	GetEngineCore().GetDirectXCore().GetClearRTColor(optClear1.Color);
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear1,
		IID_PPV_ARGS(&mCubeMapBuffer)));

	// Create the depth/stencil buffer.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mWidth;
	depthStencilDesc.Height = mHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = GetEngineCore().GetDirectXCore().GetDepthStencilFormat();
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE optClear2;
	optClear2.Format = GetEngineCore().GetDirectXCore().GetDepthStencilFormat();
	optClear2.DepthStencil.Depth = 1.0f;
	optClear2.DepthStencil.Stencil = 0;
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear2,
		IID_PPV_ARGS(mCubeDepthStencilBuffer.GetAddressOf())));

	// Irradiance map buffer creation.
	D3D12_RESOURCE_DESC irrTexDesc;
	ZeroMemory(&irrTexDesc, sizeof(D3D12_RESOURCE_DESC));
	irrTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	irrTexDesc.Alignment = 0;
	irrTexDesc.Width = mIrrWidth;
	irrTexDesc.Height = mIrrHeight;
	irrTexDesc.DepthOrArraySize = 1;
	irrTexDesc.MipLevels = 1;
	irrTexDesc.Format = mFormat;
	irrTexDesc.SampleDesc.Count = 1;
	irrTexDesc.SampleDesc.Quality = 0;
	irrTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	irrTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&irrTexDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mCubeMapIrradianceBuffer)));
}

void CubeRenderTarget::BuildPassConstantBuffer()
{
	auto device = GetEngineCore().GetDirectXCore().GetDevice();
	mPassCB.clear();
	mPassCB.reserve(NUM_FRAME_RESOURCES);
	// Make constant buffer for each frame resource
	for (int i = 0; i < NUM_FRAME_RESOURCES; ++i)
	{
		// Make constant buffer, with the room for six PassConstants. Each for one cube side.
		auto passCB = make_unique<UploadBuffer<PassConstants>>(device, 6, true);
		mPassCB.push_back(move(passCB));
	}
}

void CubeRenderTarget::BuildStaticMembers(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList)
{
	// Build root signature that is used for CS that builds the irradiance map
	CD3DX12_DESCRIPTOR_RANGE srvTable1;
	srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE srvTable2;
	srvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstants(12, 0);				// root constant
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable1);	// descriptor table (for sampler rays)
	slotRootParameter[2].InitAsDescriptorTable(1, &srvTable2);	// descriptor table (for read texture)
	slotRootParameter[3].InitAsDescriptorTable(1, &uavTable);	// descriptor table (for write texture)

	// Defining sampler
	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		1, &linearWrap,
		D3D12_ROOT_SIGNATURE_FLAG_NONE);

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
	ThrowIfFailed(device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mIrrMapCSRootSignature.GetAddressOf())));

	// Load the CS shader
	HMODULE hMod = LoadLibraryEx(ENGINE_LIBRARY_NAME, NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (NULL != hMod)
	{
		HRSRC hRes = FindResource(hMod, MAKEINTRESOURCE(ID_CSO), MAKEINTRESOURCE(RT_IRR_MAP_CS));
		if (NULL != hRes)
		{
			HGLOBAL hgbl = LoadResource(hMod, hRes);
			void *pData = LockResource(hgbl);
			UINT32 sizeInBytes = SizeofResource(hMod, hRes);
			ThrowIfFailed(D3DCreateBlob(sizeInBytes, mIrrMapCS.GetAddressOf()));
			memcpy((char*)mIrrMapCS->GetBufferPointer(), pData, sizeInBytes);
		}
		FreeLibrary(hMod);
	}

	// PSO for CS
	D3D12_COMPUTE_PIPELINE_STATE_DESC horzBlurPSO = {};
	horzBlurPSO.pRootSignature = mIrrMapCSRootSignature.Get();
	horzBlurPSO.CS =
	{
		reinterpret_cast<BYTE*>(mIrrMapCS->GetBufferPointer()), mIrrMapCS->GetBufferSize()
	};
	horzBlurPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(device->CreateComputePipelineState(&horzBlurPSO, IID_PPV_ARGS(&mIrrMapCSPSO)));

	// Compute sampler rays and create buffer for it.	
	vector<XMFLOAT4> data;
	data.resize(CUBE_MAP_SAMPLER_RAYS_TOTAL_COUNT);
	RandomGenerator rndGen(123);
	for (int i = 0; i < CUBE_MAP_SAMPLER_RAYS_TOTAL_COUNT; ++i)
	{
		// based on: http://www.codinglabs.net/article_physically_based_rendering_cook_torrance.aspx
		float alpha = 1.0f;
		float xsi1 = rndGen.GetReal();
		float xsi2 = rndGen.GetReal();
		float rho = 1.0f;
		float theta = atanf(alpha * sqrtf(xsi1) / sqrtf(1 - xsi1));
		float phi = 2.0f * XM_PI * xsi2;
		XMFLOAT4 f;
		f.x = rho*cos(phi)*sin(theta);
		f.y = rho*cos(theta);
		f.z = rho*sin(phi)*sin(theta);
		f.w = 0.0f;
		data[i] = f;
	}

	UINT byteSize = (UINT)data.size() * sizeof(XMFLOAT4);
	mSamplerRaysBuffer = CreateDefaultBuffer(device, cmdList, &data[0], byteSize, mSamplerRaysUploadBuffer);
}

void CubeRenderTarget::ReleaseStaticMembers()
{
	mIrrMapCSRootSignature.Reset();
	mIrrMapCS.Reset();
	mIrrMapCSPSO.Reset();
	mSamplerRaysBuffer.Reset();
	mSamplerRaysUploadBuffer.Reset();
}
