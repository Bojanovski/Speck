
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

int GetWorldPropertiesCommand::Execute(void *ptIn, CommandResult *result) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());

	if (!result)
	{
		LOG(L"Result object is not passed to the function.", ERROR);
		return 1;
	}

	GetWorldPropertiesCommandResult *resPt = dynamic_cast<GetWorldPropertiesCommandResult *>(result);
	if (!resPt)
	{
		LOG(L"Result object is not of the appropriate type.", ERROR);
		return 1;
	}

	resPt->speckRadius = sWorld->GetSpecksHandler()->GetSpeckRadius();
	return 0;
}

int CreatePSOGroupCommand::Execute(void *ptIn, CommandResult *result) const
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
	THROW_IF_FAILED(dxCore.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sWorld->mPSOGroups[PSOGroupName]->mPSO)));
	return 0;
}

int AddRenderItemCommand::Execute(void *ptIn, CommandResult *result) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());

	// Determine the PSO group and create the render item.
	vector<unique_ptr<RenderItem>> *psoGroupItems;
	unique_ptr<RenderItem> rItem;
	switch (type)
	{
		case Static:
			rItem = make_unique<StaticRenderItem>();
			psoGroupItems = (PSOGroupNameOverride.empty()) ? &sWorld->mPSOGroups["static"]->mRItems : &sWorld->mPSOGroups[PSOGroupNameOverride]->mRItems;
			break;
		case SpeckRigidBody:
			rItem = make_unique<SpeckRigidBodyRenderItem>();
			psoGroupItems = (PSOGroupNameOverride.empty()) ? &sWorld->mPSOGroups["static"]->mRItems : &sWorld->mPSOGroups[PSOGroupNameOverride]->mRItems;
			break;
		case SpeckSkeletalBody:
			rItem = make_unique<SpeckSkeletalBodyRenderItem>();
			psoGroupItems = (PSOGroupNameOverride.empty()) ? &sWorld->mPSOGroups["skeletalBody"]->mRItems : &sWorld->mPSOGroups[PSOGroupNameOverride]->mRItems;
			break;
		default:
		{
			LOG(L"Unrecognized type.", ERROR);
			return 1;
		}
	}

	// Fill in the render item with appropriate data.
	rItem->mGeo = sApp->mGeometries[geometryName].get();
	switch (type)
	{
		case Static:
		{
			StaticRenderItem *pSRItem = static_cast<StaticRenderItem*>(rItem.get()); // cast it so that it is easier to manipulate
			pSRItem->mRenderItemBufferIndex = sWorld->GetRenderItemFreeSpace();
			pSRItem->mMat = static_cast<PBRMaterial *>(sApp->mMaterials[materialName].get());
			pSRItem->mTexTransform = texTransform;
			pSRItem->mBounds = &rItem->mGeo->DrawArgs[meshName].Bounds;
			pSRItem->mW.mS = staticRenderItem.worldTransform.mS;
			pSRItem->mW.mT = staticRenderItem.worldTransform.mT;
			pSRItem->mW.mR = staticRenderItem.worldTransform.mR;
			break;
		}
		case SpeckRigidBody:
		{
			SpeckRigidBodyRenderItem *pSRBRItem = static_cast<SpeckRigidBodyRenderItem*>(rItem.get()); // cast it so that it is easier to manipulate
			pSRBRItem->mRenderItemBufferIndex = sWorld->GetRenderItemFreeSpace();
			pSRBRItem->mMat = static_cast<PBRMaterial *>(sApp->mMaterials[materialName].get());
			pSRBRItem->mTexTransform = texTransform;
			pSRBRItem->mSpeckRigidBodyIndex = speckRigidBodyRenderItem.rigidBodyIndex;
			pSRBRItem->mL.mS = staticRenderItem.worldTransform.mS;
			pSRBRItem->mL.mT = staticRenderItem.worldTransform.mT;
			pSRBRItem->mL.mR = staticRenderItem.worldTransform.mR;
			break;
		}
		case SpeckSkeletalBody:
		{
			SpeckSkeletalBodyRenderItem *pSSBRItem = static_cast<SpeckSkeletalBodyRenderItem*>(rItem.get()); // cast it so that it is easier to manipulate
			//pSSBRItem->mRenderItemBufferIndex = sWorld->GetRenderItemFreeSpace();
			pSSBRItem->mMat = static_cast<PBRMaterial *>(sApp->mMaterials[materialName].get());
			pSSBRItem->mTexTransform = texTransform;
			// ...
			break;
		}
		default:
		{
			LOG(L"Unrecognized type.", ERROR);
			return 1;
		}
	}
	rItem->mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rItem->mIndexCount = rItem->mGeo->DrawArgs[meshName].IndexCount;
	rItem->mStartIndexLocation = rItem->mGeo->DrawArgs[meshName].StartIndexLocation;
	rItem->mBaseVertexLocation = rItem->mGeo->DrawArgs[meshName].BaseVertexLocation;

	// Add to the render group.
	psoGroupItems->push_back(std::move(rItem));
	return 0;
}

int AddEnviromentMapCommand::Execute(void * ptIn, CommandResult *result) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());
	auto &dxCore = sApp->GetEngineCore().GetDirectXCore();
	auto commandList = sApp->GetEngineCore().GetDirectXCore().GetCommandList();
	auto cmdListAlloc = sApp->GetCurrentFrameResource()->CmdListAlloc;

	// DirectX CommandList is needed to execute this command.
	// Reset the command list to prep for initialization commands.
	THROW_IF_FAILED(dxCore.GetCommandList()->Reset(dxCore.GetCommandAllocator(), nullptr));

	// Actual building of the CubeRenderTarget.
	auto crt = make_unique<CubeRenderTarget>(sApp->GetEngineCore(), *sWorld, width, height, pos, DXGI_FORMAT_R8G8B8A8_UNORM);
	CubeRenderTarget *cubePtr = crt.get();
	// Add to the map.
	sWorld->mCubeRenderTarget[name] = move(crt);

	// Execute the initialization commands.
	THROW_IF_FAILED(dxCore.GetCommandList()->Close());
	ID3D12CommandList* cmdsListsInitialization[] = { dxCore.GetCommandList() };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsListsInitialization), cmdsListsInitialization);

	// Wait until initialization is complete.
	dxCore.FlushCommandQueue();

	//
	// Render the cube faces and calculate the irradiance map.
	//

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	THROW_IF_FAILED(commandList->Reset(cmdListAlloc.Get(), nullptr));

	commandList->SetGraphicsRootSignature(sApp->GetRootSignature());

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	auto matBuffer = sApp->GetCurrentFrameResource()->MaterialBuffer->Resource();
	commandList->SetGraphicsRootShaderResourceView((UINT)SpeckApp::MainPassRootParameter::MaterialsRootDescriptor, matBuffer->GetGPUVirtualAddress());

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
		commandList->SetGraphicsRootConstantBufferView((UINT)SpeckApp::MainPassRootParameter::PassRootDescriptor, passCBAddress);

		// Update the camera first.
		Camera *prevCamPt = &sApp->GetEngineCore().GetCamera();
		sApp->GetEngineCore().SetCamera(cubePtr->GetCamera(i));
		sWorld->Draw(0);
		// Return the current camera pointer to the previous one.
		sApp->GetEngineCore().SetCamera(prevCamPt);
	}

	// Change back to GENERIC_READ so we can read the texture in a shader.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cubePtr->TexResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
	// Update the irradiance map
	cubePtr->UpdateIrradianceMap();

	// Done recording commands.
	THROW_IF_FAILED(commandList->Close());
	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsListRenderCubeMap[] = { commandList };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsListRenderCubeMap), cmdsListRenderCubeMap);

	// Wait until rendering is complete.
	dxCore.FlushCommandQueue();
	return 0;
}

AddStaticColliderCommand::AddStaticColliderCommand() : WorldCommand(), ID(IDCounter++) {}

int AddStaticColliderCommand::Execute(void * ptIn, CommandResult *result) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());

	StaticCollider temp;
	temp.mID = ID;
	XMStoreFloat4x4(&temp.mWorld, transform.GetWorldMatrix());
	XMStoreFloat4x4(&temp.mInvWorld, transform.GetInverseWorldMatrix());
	sWorld->mStaticColliders.push_back(temp);
	sWorld->GetSpecksHandler()->InvalidateStaticCollidersBuffers();

	// In case this command will be used again.
	ID = IDCounter++;
	return 0;
}

AddExternalForceCommand::AddExternalForceCommand() : WorldCommand(), ID(IDCounter++) {}

int AddExternalForceCommand::Execute(void * ptIn, CommandResult *result) const
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

int AddSpecksCommand::Execute(void * ptIn, CommandResult *result) const
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
		case RigidBodyJoint:
		{
			materialName = "speckJoint";
			code = SPECK_CODE_RIGID_BODY;
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
			case RigidBodyJoint:
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
				tempS.mParam[0] = fluid.cohesionCoefficient;
				tempS.mParam[1] = fluid.viscosityCoefficient;
				break;
			default:
				break;
		}
		sWorld->mSpecks.push_back(tempS);
	}
	// Invalidate buffers.
	sWorld->mSpecksHandler->AddParticles((UINT)newSpecks.size());

	switch (speckType)
	{
		case Normal:
			break;
		case RigidBody:
			AddRigidBody(ptIn, result);
			break;
		case RigidBodyJoint:
			AddRigidBodyJoint(ptIn, result);
			break;
		case Fluid:
			break;
		default:
			break;
	}

	return 0;
}

void AddSpecksCommand::AddRigidBody(void *ptIn, CommandResult *result) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());
	UINT firstIndex = (UINT)(sWorld->GetSpeckInstancesRenderItem()->mInstanceCount - newSpecks.size());

	// Calculate center of mass.
	float cmX = 0.0f;
	float cmY = 0.0f;
	float cmZ = 0.0f;
	float massSum = 0.0f;
	for (UINT i = 0; i < (UINT)newSpecks.size(); ++i)
	{
		float mass = speckMass;
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
	data.mRBData.updateToGPU = true;
	data.mRBData.movementMode = RIGID_BODY_MOVEMENT_MODE_GPU;
	XMMATRIX invW = transform.GetInverseWorldMatrix();
	data.mLinks.resize(newSpecks.size());

	for (UINT i = 0; i < data.mLinks.size(); i++)
	{
		UINT speckIndex = firstIndex + i;
		data.mLinks[i].mSpeckIndex = speckIndex;
		XMVECTOR posW = XMLoadFloat3(&sWorld->mSpecks[speckIndex].mPosition);
		XMVECTOR posL = XMVector3TransformCoord(posW, invW);
		XMStoreFloat3(&data.mLinks[i].mPosInRigidBody, posL);
	}
	sWorld->mSpeckRigidBodyData.push_back(data);
	// For speck links
	sWorld->mSpecksHandler->InvalidateRigidBodyLinksBuffers();
	// For the correct movement mode flag
	sWorld->mSpecksHandler->InvalidateRigidBodyUploaderBuffer();

	// Return values
	UINT rigidBodyIndex = (UINT)(sWorld->mSpeckRigidBodyData.size() - 1);
	AddSpecksCommandResult *resPt;
	if (!result || !(resPt = dynamic_cast<AddSpecksCommandResult *>(result)))
	{
		LOG(L"Result object is not passed to the function or it is not of the appropriate type.", WARNING);
	}
	else
	{
		resPt->rigidBodyIndex = rigidBodyIndex;
	}
}

void AddSpecksCommand::AddRigidBodyJoint(void *ptIn, CommandResult *result) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());
	UINT firstIndex = (UINT)(sWorld->GetSpeckInstancesRenderItem()->mInstanceCount - newSpecks.size());

	UINT rb1Index = rigidBodyJoint.rigidBodyIndex[0];
	UINT rb2Index = rigidBodyJoint.rigidBodyIndex[1];

	SpeckRigidBodyData &rbd1 = sWorld->mSpeckRigidBodyData[rb1Index];
	SpeckRigidBodyData &rbd2 = sWorld->mSpeckRigidBodyData[rb2Index];

	UINT addingStartIndex1 = (UINT)rbd1.mLinks.size();
	UINT addingStartIndex2 = (UINT)rbd2.mLinks.size();

	rbd1.mLinks.resize(rbd1.mLinks.size() + newSpecks.size());
	rbd2.mLinks.resize(rbd2.mLinks.size() + newSpecks.size());

	// Add the joint specks to the rigid body
	for (UINT i = 0; i < newSpecks.size(); i++)
	{
		UINT speckIndex = firstIndex + i;
		XMVECTOR posW = XMLoadFloat3(&sWorld->mSpecks[speckIndex].mPosition);

		UINT linkIndex1 = addingStartIndex1 + i;
		UINT linkIndex2 = addingStartIndex2 + i;

		rbd1.mLinks[linkIndex1].mSpeckIndex = speckIndex;
		rbd2.mLinks[linkIndex2].mSpeckIndex = speckIndex;
	}

	// Recalculate center of masses for body 1
	float cmX = 0.0f;
	float cmY = 0.0f;
	float cmZ = 0.0f;
	float massSum = 0.0f;
	for (UINT i = 0; i < rbd1.mLinks.size(); i++)
	{
		UINT speckIndex = rbd1.mLinks[i].mSpeckIndex;
		float mass = sWorld->mSpecks[speckIndex].mMass;
		XMFLOAT3 position = sWorld->mSpecks[speckIndex].mPosition;
		cmX += mass * position.x;
		cmY += mass * position.y;
		cmZ += mass * position.z;
		massSum += mass;
	}
	cmX /= massSum;
	cmY /= massSum;
	cmZ /= massSum;
	rbd1.mRBData.mWorld._41 = cmX; // change just the translation component
	rbd1.mRBData.mWorld._42 = cmY;
	rbd1.mRBData.mWorld._43 = cmZ;
	XMMATRIX W1 = XMLoadFloat4x4(&rbd1.mRBData.mWorld);
	XMMATRIX invW1 = XMMatrixInverse(0, W1);

	// Recalculate center of masses for body 2
	cmX = 0.0f;
	cmY = 0.0f;
	cmZ = 0.0f;
	massSum = 0.0f;
	for (UINT i = 0; i < rbd2.mLinks.size(); i++)
	{
		UINT speckIndex = rbd2.mLinks[i].mSpeckIndex;
		float mass = sWorld->mSpecks[speckIndex].mMass;
		XMFLOAT3 position = sWorld->mSpecks[speckIndex].mPosition;
		cmX += mass * position.x;
		cmY += mass * position.y;
		cmZ += mass * position.z;
		massSum += mass;
	}
	cmX /= massSum;
	cmY /= massSum;
	cmZ /= massSum;
	rbd2.mRBData.mWorld._41 = cmX; // change just the translation component
	rbd2.mRBData.mWorld._42 = cmY;
	rbd2.mRBData.mWorld._43 = cmZ;
	XMMATRIX W2 = XMLoadFloat4x4(&rbd2.mRBData.mWorld);
	XMMATRIX invW2 = XMMatrixInverse(0, W2);

	// Recalculate relative speck positions in body 1
	for (UINT i = 0; i < rbd1.mLinks.size(); i++)
	{
		UINT speckIndex = rbd1.mLinks[i].mSpeckIndex; // We already have this for all member specks
		XMVECTOR posW = XMLoadFloat3(&sWorld->mSpecks[speckIndex].mPosition);
		XMVECTOR posL = XMVector3TransformCoord(posW, invW1);
		XMStoreFloat3(&rbd1.mLinks[i].mPosInRigidBody, posL);
	}

	// Recalculate relative speck positions in body 2
	for (UINT i = 0; i < rbd2.mLinks.size(); i++)
	{
		UINT speckIndex = rbd2.mLinks[i].mSpeckIndex; // We already have this for all member specks
		XMVECTOR posW = XMLoadFloat3(&sWorld->mSpecks[speckIndex].mPosition);
		XMVECTOR posL = XMVector3TransformCoord(posW, invW2);
		XMStoreFloat3(&rbd2.mLinks[i].mPosInRigidBody, posL);
	}

	// Buffer invalidation for speck links
	sWorld->mSpecksHandler->InvalidateRigidBodyLinksBuffers();
	// For the correct movement mode flag
	sWorld->mSpecksHandler->InvalidateRigidBodyUploaderBuffer();
}

int UpdateSpeckRigidBodyCommand::Execute(void * ptIn, CommandResult * result) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());

	RigidBodyData &rbd = sWorld->mSpeckRigidBodyData[rigidBodyIndex].mRBData;
	rbd.updateToGPU = true;
	rbd.movementMode = (movementMode == RigidBodyMovementMode::CPU) ? RIGID_BODY_MOVEMENT_MODE_CPU : RIGID_BODY_MOVEMENT_MODE_GPU;
	transform.StoreTranspose(&rbd.mWorld);
	sWorld->mSpecksHandler->InvalidateRigidBodyUploaderBuffer();

	return 0;
}

int SetTimeMultiplierCommand::Execute(void * ptIn, CommandResult * result) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());
	sWorld->mSpecksHandler->SetTimeMultiplier(timeMultiplierConstant);

	return 0;
}

int SetSpecksSolverParametersCommand::Execute(void * ptIn, CommandResult * result) const
{

	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	SpeckWorld *sWorld = static_cast<SpeckWorld*>(&sApp->GetWorld());

	if (stabilizationIteraions != UINT_MAX)
		sWorld->mSpecksHandler->SetStabilizationIteraions(stabilizationIteraions);
	if (solverIterations != UINT_MAX)
		sWorld->mSpecksHandler->SetSolverIterations(solverIterations);
	if (substepsIterations != UINT_MAX)
		sWorld->mSpecksHandler->SetSubstepsIterations(substepsIterations);

	return 0;
}
