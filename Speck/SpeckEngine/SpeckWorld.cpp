
#include "SpeckWorld.h"
#include "DirectXCore.h"
#include "SpeckApp.h"
#include "PSOGroup.h"
#include "FrameResource.h"
#include "RenderItem.h"
#include "EngineCore.h"
#include "Timer.h"
#include "Camera.h"
#include "CubeRenderTarget.h"
#include "SpecksHandler.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

SpeckWorld::SpeckWorld(UINT maxRenderItemsCount)
	: World(),
	mMaxRenderItemsCount(maxRenderItemsCount)
{
	// Create the PSO groups.
	mPSOGroups["instanced"] = make_unique<PSOGroup>();
	mPSOGroups["static"]	= make_unique<PSOGroup>();
	//mPSOGroups["skeletalBody"]	= make_unique<PSOGroup>();

	// Initalize the free space stack.
	mFreeSpacesRenderItemBuffer.resize(mMaxRenderItemsCount);
	for (UINT i = 0; i < mMaxRenderItemsCount; i++)
	{
		mFreeSpacesRenderItemBuffer[i] = mMaxRenderItemsCount - i - 1;
	}
}

SpeckWorld::~SpeckWorld()
{
}

void SpeckWorld::Initialize(App * app)
{
	World::Initialize(app);
	SpeckApp *sApp = static_cast<SpeckApp *>(app);
	
	// Create the specks handler.
	mSpecksHandler = make_unique<SpecksHandler>(sApp->GetEngineCore(), *this, &sApp->mFrameResources, 2, 4, 1);

	// Build the specks render item.
	auto rItem = make_unique<SpecksRenderItem>();
	rItem->mGeo = sApp->mGeometries["speckGeo"].get();
	rItem->mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rItem->mInstanceCount = 0;
	rItem->mIndexCount = rItem->mGeo->DrawArgs["speck"].IndexCount;
	rItem->mStartIndexLocation = rItem->mGeo->DrawArgs["speck"].StartIndexLocation;
	rItem->mBaseVertexLocation = rItem->mGeo->DrawArgs["speck"].BaseVertexLocation;
	rItem->mBufferIndex = mSpecksHandler->GetSpecksBufferIndex();
	// Save reference.
	mSpeckInstancesRenderItem = rItem.get();
	// Add to the render group.
	mPSOGroups["instanced"]->mRItems.push_back(move(rItem));
}

int SpeckWorld::ExecuteCommand(const WorldCommand &command, CommandResult *result)
{
	return World::ExecuteCommand(command, result);
}

void SpeckWorld::Update()
{
	SpeckApp *sApp = static_cast<SpeckApp *>(mApp);
	auto currCB = sApp->mCurrFrameResource->RenderItemConstantsBuffer.get();
	// For each PSO group.
	for (auto& psoGrp : mPSOGroups)
	{
		// For each render item in the PSO group.
		for (auto& rItem : psoGrp.second->mRItems)
		{
			rItem->UpdateBufferCPU(sApp, sApp->mCurrFrameResource);
		}
	}

	XMMATRIX view = sApp->GetEngineCore().GetCamera().GetView();
	XMMATRIX proj = sApp->GetEngineCore().GetCamera().GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = sApp->GetEngineCore().GetCamera().GetPosition();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)sApp->GetEngineCore().GetClientWidth(), (float)sApp->GetEngineCore().GetClientHeight());
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / sApp->GetEngineCore().GetClientWidth(), 1.0f / sApp->GetEngineCore().GetClientHeight());
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = sApp->GetEngineCore().GetTimer()->TotalTime();
	mMainPassCB.DeltaTime = sApp->GetEngineCore().GetTimer()->DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto currPassCB = sApp->mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);

	// Update the cube maps
	for (auto &cube : mCubeRenderTarget)
		cube.second->UpdateCBs();

	// Speck handler
	mSpecksHandler->UpdateCPU(sApp->mCurrFrameResource);
}

void SpeckWorld::PreDrawUpdate()
{
	SpeckApp *sApp = static_cast<SpeckApp *>(mApp);
	
	// Speck handler
	mSpecksHandler->UpdateGPU();

	// For each PSO group.
	for (auto& psoGrp : mPSOGroups)
	{
		// For each render item in a PSO group.
		for (auto& rItem : psoGrp.second->mRItems)
		{
			rItem->UpdateBufferGPU(sApp, sApp->mCurrFrameResource);
		}
	}
}

void SpeckWorld::Draw(UINT stage)
{
	switch (stage)
	{
		case 0:
			Draw_Scene();
			break;
		case 1:
			break;
		default:
			break;
	}
}

void SpeckWorld::Draw_Scene()
{
	SpeckApp *sApp = static_cast<SpeckApp *>(mApp);
	auto &dxCore = sApp->GetEngineCore().GetDirectXCore();
	XMMATRIX view = sApp->GetEngineCore().GetCamera().GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	const BoundingFrustum &mCamFrustum = mApp->GetEngineCore().GetCamera().GetFrustum();
	BoundingFrustum transformedFrustum;
	mCamFrustum.Transform(transformedFrustum, invView);

	// Bind all the rigid bodies from the specks handler.
	auto rigidBodyBuffer = mSpecksHandler->GetRigidBodyBufferResource();
	dxCore.GetCommandList()->SetGraphicsRootShaderResourceView((UINT)SpeckApp::MainPassRootParameter::RigidBodyRootDescriptor, rigidBodyBuffer->GetGPUVirtualAddress());

	// For all PSO groups
	for (auto &grp : mPSOGroups)
	{
		dxCore.GetCommandList()->SetPipelineState(grp.second->mPSO.Get());

		// For each render item...
		for (size_t i = 0; i < grp.second->mRItems.size(); ++i)
		{
			auto ri = grp.second->mRItems[i].get();
			ri->Render(mApp, sApp->mCurrFrameResource, transformedFrustum);
		}
	}
}

UINT SpeckWorld::GetRenderItemFreeSpace()
{
	UINT ret = mFreeSpacesRenderItemBuffer.back();
	mFreeSpacesRenderItemBuffer.pop_back();
	return ret;
}
