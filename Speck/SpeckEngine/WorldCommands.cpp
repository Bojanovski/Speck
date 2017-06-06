
#include "WorldCommands.h"
#include "DirectXHeaders.h"
#include "SpeckApp.h"
#include "DirectXCore.h"
#include "FrameResource.h"
#include "GenericShaderStructures.h"
#include "GeometryGenerator.h"
#include "PSOGroup.h"
#include "RenderItem.h"
#include "SpeckWorld.h"
#include "CubeRenderTarget.h"
#include "SpecksHandler.h"
#include "RandomGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;
using namespace Speck::WorldCommands;

RandomGenerator gRndGen;
int AddStaticColliderCommand::IDCounter = 0;
int AddExternalForceCommand::IDCounter = 0;

int CreatePSOGroup::Execute(void *ptIn, void *ptOut) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());
	auto &dxCore = sApp->GetEngineCore().GetDirectXCore();
	auto srcRes = sWorld->mPSOGroups.find(PSOGroupName);
	if (srcRes != sWorld->mPSOGroups.end() && srcRes->second->mPSO)
	{
		LOG(L"PSO group with this key already exists, returning.", ERROR);
		return 1;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	psoDesc.pRootSignature = sApp->GetRootSignature();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(sApp->mShaders[vsName]->GetBufferPointer()), sApp->mShaders[vsName]->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(sApp->mShaders[psName]->GetBufferPointer()), sApp->mShaders[psName]->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = DEFERRED_RENDER_TARGETS_COUNT;
	for (int i = 0; i < DEFERRED_RENDER_TARGETS_COUNT; ++i)
	{
		psoDesc.RTVFormats[i] = sApp->GetDeferredRTFormat(i);
	}
	psoDesc.SampleDesc.Count = dxCore.Get4xMsaaState() ? 4 : 1;
	psoDesc.SampleDesc.Quality = dxCore.Get4xMsaaState() ? (dxCore.Get4xMsaaQuality() - 1) : 0;
	psoDesc.DSVFormat = dxCore.GetDepthStencilFormat();
	ThrowIfFailed(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sWorld->mPSOGroups[PSOGroupName]->mPSO)));
	return 0;
}

int AddRenderItemCommand::Execute(void *ptIn, void *ptOut) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());

	auto rItem = std::make_unique<SingleRenderItem>();
	rItem->mObjCBIndex = (UINT)sWorld->mPSOGroups[PSOGroupName]->mRItems.size(); // privremeno, treba isto handleat brisanje
	rItem->mGeo = sApp->mGeometries[geometryName].get();
	rItem->mMat = static_cast<PBRMaterial *>(sApp->mMaterials[materialName].get());
	rItem->mTexTransform = texTransform;
	rItem->mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rItem->mIndexCount = rItem->mGeo->DrawArgs[meshName].IndexCount;
	rItem->mStartIndexLocation = rItem->mGeo->DrawArgs[meshName].StartIndexLocation;
	rItem->mBaseVertexLocation = rItem->mGeo->DrawArgs[meshName].BaseVertexLocation;
	rItem->mBounds = &rItem->mGeo->DrawArgs[meshName].Bounds;
	rItem->mT.mS = transform.mS;
	rItem->mT.mT = transform.mT;
	rItem->mT.mR = transform.mR;

	// Add to the render group.
	sWorld->mPSOGroups[PSOGroupName]->mRItems.push_back(std::move(rItem));
	return 0;
}

int AddEnviromentMapCommand::Execute(void * ptIn, void * ptOut) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());
	auto &dxCore = sApp->GetEngineCore().GetDirectXCore();
	auto commandList = sApp->GetEngineCore().GetDirectXCore().GetCommandList();
	auto cmdListAlloc = sApp->GetCurrentFrameResource()->CmdListAlloc;

	// DirectX CommandList is needed to execute this command.
	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(dxCore.GetCommandList()->Reset(dxCore.GetCommandAllocator(), nullptr));

	// Actual building of the CubeRenderTarget.
	auto crt = make_unique<CubeRenderTarget>(sApp->GetEngineCore(), *sWorld, width, height, pos, DXGI_FORMAT_R8G8B8A8_UNORM);
	CubeRenderTarget *cubePtr = crt.get();
	// Add to the map.
	sWorld->mCubeRenderTarget[name] = move(crt);

	// Execute the initialization commands.
	ThrowIfFailed(dxCore.GetCommandList()->Close());
	ID3D12CommandList* cmdsListsInitialization[] = { dxCore.GetCommandList() };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsListsInitialization), cmdsListsInitialization);

	// Wait until initialization is complete.
	dxCore.FlushCommandQueue();

	//
	// Render the cube faces and calculate the irradiance map.
	//

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(commandList->Reset(cmdListAlloc.Get(), nullptr));

	commandList->SetGraphicsRootSignature(sApp->GetRootSignature());

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	auto matBuffer = sApp->GetCurrentFrameResource()->MaterialBuffer->Resource();
	commandList->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());

	commandList->RSSetViewports(1, &cubePtr->Viewport());
	commandList->RSSetScissorRects(1, &cubePtr->ScissorRect());

	// Change to RENDER_TARGET.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cubePtr->TexResource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	UINT passCBByteSize = CalcConstantBufferByteSize(sizeof(PassConstants));

	// For each cube map face.
	for (int i = 0; i < 6; ++i)
	{
		// Clear the back buffer and depth buffer.
		commandList->ClearRenderTargetView(cubePtr->Rtv(i), sApp->GetEngineCore().GetDirectXCore().GetClearRTColor(), 0, nullptr);
		commandList->ClearDepthStencilView(cubePtr->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		// Specify the buffers we are going to render to.
		commandList->OMSetRenderTargets(1, &cubePtr->Rtv(i), true, &cubePtr->Dsv());

		// Bind the pass constant buffer for this cube map face so we use 
		// the right view/proj matrix for this cube face.
		auto passCB = cubePtr->CBResource(); // sApp->mCurrFrameResource->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + i*passCBByteSize;
		commandList->SetGraphicsRootConstantBufferView(4, passCBAddress);

		// Update the camera first.
		Camera *prevCamPt = &sApp->GetEngineCore().GetCamera();
		sApp->GetEngineCore().SetCamera(cubePtr->GetCamera(i));
		sWorld->Draw(sApp, 0);
		// Return the current camera pointer to the previous one.
		sApp->GetEngineCore().SetCamera(prevCamPt);
	}

	// Change back to GENERIC_READ so we can read the texture in a shader.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cubePtr->TexResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
	// Update the irradiance map
	cubePtr->UpdateIrradianceMap();

	// Done recording commands.
	ThrowIfFailed(commandList->Close());
	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsListRenderCubeMap[] = { commandList };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsListRenderCubeMap), cmdsListRenderCubeMap);

	// Wait until rendering is complete.
	dxCore.FlushCommandQueue();
	return 0;
}

AddStaticColliderCommand::AddStaticColliderCommand() : Command(), ID(IDCounter++) {}

int AddStaticColliderCommand::Execute(void * ptIn, void * ptOut) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());

	StaticCollider temp;
	temp.mID = ID;
	XMStoreFloat4x4(&temp.mWorld, transform.GetWorldMatrix());
	XMStoreFloat4x4(&temp.mInvWorld, transform.GetInvWorldMatrix());
	sWorld->mStaticColliders.push_back(temp);
	sWorld->GetSpecksHandler()->InvalidateStaticCollidersBuffers();

	// In case this command will be used again.
	ID = IDCounter++;
	return 0;
}

AddExternalForceCommand::AddExternalForceCommand() : Command(), ID(IDCounter++) {}

int AddExternalForceCommand::Execute(void * ptIn, void * ptOut) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());
	
	ExternalForces temp;
	temp.mID = ID;
	temp.mType = type;
	temp.mVec = vector;
	sWorld->mExternalForces.push_back(temp);
	sWorld->GetSpecksHandler()->InvalidateExternalForcesBuffers();

	// In case this command will be used again.
	ID = IDCounter++;
	return 0;
}

int AddSpecksCommand::Execute(void * ptIn, void * ptOut) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());

	UINT newCount = sWorld->GetSpeckInstancesRenderItem()->mInstanceCount += (UINT)newSpecks.size();
	sWorld->mSpecks.reserve(newCount);

	// Name of the material used for rendering
	std::string materialName;
	// Phase for the simulation
	UINT code;
	switch (speckType)
	{
		case Normal:
			materialName = "speckSand";
			code = SPECK_CODE_NORMAL;
			break;
		case RigidBody:
		{
			materialName = "speckStone";
			code = SPECK_CODE_RIGID_BODY;
			short rigidBodyCode = gRndGen.GetInt(0, SHRT_MAX);
			code |= rigidBodyCode;
		}
		break;
		case Fluid:
			materialName = "speckWater";
			code = SPECK_CODE_FLUID;
			break;
		default:
			materialName = "speckSand";
			code = SPECK_CODE_NORMAL;
			break;
	}
	PBRMaterial *matPt = static_cast<PBRMaterial *>(sApp->mMaterials[materialName].get());

	for (UINT i = 0; i < (UINT)newSpecks.size(); i++)
	{
		SpeckData tempS;
		tempS.mPosition = newSpecks[i].position;
		tempS.mMaterialIndex = matPt->MatCBIndex;
		tempS.mFrictionCoefficient = frictionCoefficient;
		tempS.mMass = speckMass;
		tempS.mCode = code;
		switch (speckType)
		{
			case Normal:
				break;
			case RigidBody:
			{
				// Calculate SDF gradient
				XMVECTOR gradient1 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f); // used for calculating the actual gradient
				XMVECTOR gradient2 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f); // used as an indicator of the boundary specks
				XMVECTOR pos = XMLoadFloat3(&newSpecks[i].position);
				float maxDist = sWorld->GetSpecksHandler()->GetSpeckRadius() * 2.0f * 1.2f;
				float maxDistSq = maxDist*maxDist;
				for (UINT j = 0; j < (UINT)newSpecks.size(); ++j)
				{
					if (i == j) continue;
					XMVECTOR neighbourPos = XMLoadFloat3(&newSpecks[j].position);
					XMVECTOR r = pos - neighbourPos;
					float lenSq = XMVectorGetX(XMVector3LengthSq(r));
					float lenPow = powf(lenSq, 10.0f);
					float invLenPow = 1.0f / lenPow;

					gradient1 += r * invLenPow;
					if (lenSq < maxDistSq)
					{
						gradient2 += r;
					}
				}
				gradient1 = XMVector3Normalize(gradient1);
				float grad2Len = XMVectorGetX(XMVector3LengthEst(gradient2));
				float epsilon = 0.001f;
				if (grad2Len > epsilon)
				{
					gradient1 *= 2.0f; // indication that this is a boundary speck
				}
				XMFLOAT3 gradientF;
				XMStoreFloat3(&gradientF, gradient1);
				tempS.mParam[0] = gradientF.x;
				tempS.mParam[1] = gradientF.y;
				tempS.mParam[2] = gradientF.z;
			}
			break;
			case Fluid:
				tempS.mParam[0] = cohesionCoefficient;
				tempS.mParam[1] = viscosityCoefficient;
				break;
			default:
				break;
		}
		sWorld->mSpecks.push_back(tempS);
	}
	sWorld->mSpecksHandler->AddParticles((UINT)newSpecks.size());

	switch (speckType)
	{
		case Normal:
			break;
		case RigidBody:
			AddRigidBody(ptIn);
			break;
		case Fluid:
			break;
		default:
			break;
	}
	return 0;
}

void AddSpecksCommand::AddRigidBody(void *ptIn) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());

	// Calculate center of mass.
	float cmX = 0.0f;
	float cmY = 0.0f;
	float cmZ = 0.0f;
	float massSum = 0.0f;
	for (UINT i = 0; i < (UINT)newSpecks.size(); ++i)
	{
		float mass = newSpecks[i].mass;
		XMFLOAT3 position = newSpecks[i].position;
		cmX += mass * position.x;
		cmY += mass * position.y;
		cmZ += mass * position.z;
		massSum += mass;
	}
	cmX /= massSum;
	cmY /= massSum;
	cmZ /= massSum;
	Transform transform = Transform::Identity(); // World transformation of this speck rigid body.
	transform.mT = XMFLOAT3(cmX, cmY, cmZ);

	SpeckRigidBodyData data;
	transform.Store(&data.mRBData.mWorld);
	XMMATRIX invW = transform.GetInvWorldMatrix();
	data.mLinks.resize(newSpecks.size());
	UINT firstIndex = (UINT)(sWorld->GetSpeckInstancesRenderItem()->mInstanceCount - newSpecks.size());

	for (UINT i = 0; i < data.mLinks.size(); i++)
	{
		UINT speckIndex = firstIndex + i;
		data.mLinks[i].mSpeckIndex = speckIndex;
		XMVECTOR posW = XMLoadFloat3(&sWorld->mSpecks[speckIndex].mPosition);
		XMVECTOR posL = XMVector3TransformCoord(posW, invW);
		XMStoreFloat3(&data.mLinks[i].mPosInRigidBody, posL);
	}
	sWorld->mSpeckRigidBodyData.push_back(data);
	sWorld->mSpecksHandler->InvalidateRigidBodyLinksBuffers();
}
