
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

SpeckWorld::SpeckWorld(UINT maxInstancedObject, UINT maxSingleObjects)
	: World(),
	mMaxSingleObjects(maxSingleObjects)
{
	mPSOGroups["instanced"] = make_unique<PSOGroup>();
	mPSOGroups["single"] = make_unique<PSOGroup>();



	// {-0.2, -0.19999981, -0.2}



	//"{+0.32000002, +0, +0}", "{+0, +0.3199994, +0}", "{+0, +0, +0.32000002}"

	//XMFLOAT3X3 Af( 
	//	+0.31493858, -0.035997204, -0.035997409,
	//	-0.035997495, +0.31493902, -0.035997488,
	//	-0.035997398, -0.035997204, +0.3149386);
//	 "{+11.52, +22.799274, +0}", "{+0, +0.50453484, +0}", "{+0, +0, +0.32000002}"


	XMFLOAT3X3 Af(
		11.52f, 22.799274f, 0,
		0, 0.50453484f, 0,
		0, 0, 0.32000002f);

	//XMFLOAT3X3 Af;
	//Af._11 = 0.32000002f;
	//Af._12 = 0.0f;
	//Af._13 = 0.0f;

	//Af._21 = 0.0f;
	//Af._22 = 0.3199994f;
	//Af._23 = 0.0f;

	//Af._31 = 0.0f;
	//Af._32 = 0.0f;
	//Af._33 = 0.32000002f;

	//XMFLOAT3X3 Af;
	//Af._11 = 1;
	//Af._12 = 2;
	//Af._13 = 1;

	//Af._21 = 6;
	//Af._22 = -1;
	//Af._23 = 0;

	//Af._31 = -1;
	//Af._32 = -2;
	//Af._33 = -1;

	XMMATRIX A = XMLoadFloat3x3(&Af);
	XMMATRIX ATA = XMMatrixTranspose(A) * A;
	XMVECTOR eigVal;
	XMMATRIX eigVec;
	MathHelper::GetEigendecompositionSymmetric3X3(ATA, 20, &eigVal, &eigVec);
	XMMATRIX eigVecInv = MathHelper::GetInverse3X3(eigVec);
	XMMATRIX lambdaSqrt = XMMatrixIdentity();
	lambdaSqrt.r[0].m128_f32[0] = sqrt(eigVal.m128_f32[0]);
	lambdaSqrt.r[1].m128_f32[1] = sqrt(eigVal.m128_f32[1]);
	lambdaSqrt.r[2].m128_f32[2] = sqrt(eigVal.m128_f32[2]);

	XMMATRIX S = eigVec * lambdaSqrt * eigVecInv;
	XMMATRIX SInv = MathHelper::GetInverse3X3(S);
	XMMATRIX Q = A * SInv;
	XMMATRIX ATest = Q * S;


	// "{+1.0000001, -0, +0, +0}", "{-0, +1, -0, -9.0884123}", "{+0, -0, +1.0000001, +0}", "{+0, +0, +0, +1}", "{+0, -9.0884123, +0}"


}

SpeckWorld::~SpeckWorld()
{
}

void SpeckWorld::Initialize(App * app)
{
	SpeckApp *sApp = static_cast<SpeckApp *>(app);
	
	// Create the specks handler
	mSpecksHandler = make_unique<SpecksHandler>(sApp->GetEngineCore(), *this, &sApp->mFrameResources, 0, 2, 4);

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

void SpeckWorld::PreDrawUpdate(App * app)
{
	SpeckApp *sApp = static_cast<SpeckApp *>(app);
	
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
	const BoundingFrustum &mCamFrustum = app->GetEngineCore().GetCamera().GetFrustum();
	BoundingFrustum transformedFrustum;
	mCamFrustum.Transform(transformedFrustum, invView);

	// For all PSO groups
	for (auto &grp : mPSOGroups)
	{
		sApp->GetEngineCore().GetDirectXCore().GetCommandList()->SetPipelineState(grp.second->mPSO.Get());

		// For each render item...
		for (size_t i = 0; i < grp.second->mRItems.size(); ++i)
		{
			auto ri = grp.second->mRItems[i].get();
			ri->Render(app, sApp->mCurrFrameResource, transformedFrustum);
		}
	}
}