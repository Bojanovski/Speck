
#include "SpeckWorld.h"
#include "DirectXCore.h"
#include "SpeckApp.h"
#include "PSOGroup.h"
#include "FrameResource.h"
#include "RenderItem.h"
#include "EngineCore.h"
#include "GameTimer.h"
#include "Camera.h"
#include "CubeRenderTarget.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

SpeckWorld::SpeckWorld(UINT maxInstancedObject, UINT maxSingleObjects)
	: World(),
	mMaxInstancedObject(maxInstancedObject),
	mMaxSingleObjects(maxSingleObjects)
{
	mPSOGroups["instanced"] = make_unique<PSOGroup>();
	mPSOGroups["single"] = make_unique<PSOGroup>();
}

SpeckWorld::~SpeckWorld()
{
}

void SpeckWorld::Update(App* app)
{
	SpeckApp *sApp = static_cast<SpeckApp *>(app);
	auto currObjectCB = sApp->mCurrFrameResource->ObjectCB.get();
	// For each PSO group.
	for (auto& psoGrp : mPSOGroups)
	{
		// For each render item in a PSO group.
		for (auto& rItem : psoGrp.second->mRItems)
		{
			rItem->UpdateBuffer(sApp->mCurrFrameResource);
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
}

void SpeckWorld::Draw(App * app, UINT stage)
{
	switch (stage)
	{
		case 0:
			Draw_Scene(app);
			break;
		case 1:
			break;
		default:
			break;
	}
}

void SpeckWorld::Draw_Scene(App* app)
{
	SpeckApp *sApp = static_cast<SpeckApp *>(app);
	XMMATRIX view = sApp->GetEngineCore().GetCamera().GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	BoundingFrustum bf = sApp->GetEngineCore().GetCamera().GetFrustum();

	// For all PSO groups
	for (auto &grp : mPSOGroups)
	{
		sApp->GetEngineCore().GetDirectXCore().GetCommandList()->SetPipelineState(grp.second->mPSO.Get());

		// For each render item...
		for (size_t i = 0; i < grp.second->mRItems.size(); ++i)
		{
			auto ri = grp.second->mRItems[i].get();
			ri->Render(app, sApp->mCurrFrameResource, invView);
		}
	}
}