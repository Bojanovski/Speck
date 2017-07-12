
#include "SpecksHandler.h"
#include "FrameResource.h"
#include "EngineCore.h"
#include "DirectXCore.h"
#include "SpeckWorld.h"
#include "Resources.h"
#include "Timer.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck;

ComPtr<ID3D12RootSignature> SpecksHandler::mRootSignature = nullptr;
ComPtr<ID3DBlob> SpecksHandler::mCS_phases[SpecksHandler::mCS_phasesCount];
ComPtr<ID3D12PipelineState> SpecksHandler::mPSOs[SpecksHandler::mCS_phasesCount];
vector<UINT> SpecksHandler::mPrimes;
float SpecksHandler::mSpeckRadius;
float SpecksHandler::mCellSize;

// Number of 32-bit values in the constant buffer
const int gNumConstVals = 12;

namespace GPU
{
	// Helper structure used to pass the information about specks to the device.
	struct SpeckUploadData
	{
		// Position of the speck
		DirectX::XMFLOAT3 position;
		// Type of the speck is coded into this variable
		UINT code;
		// Mass of this speck
		float mass;
		// Friction coefficient
		float frictionCoefficient;
		// Special parameters that depend on speck type
		float param[SPECK_SPECIAL_PARAM_N];
		// Used for rendering
		UINT materialIndex;
	};

	// Information about a single speck on the device used for simulation
	struct SpeckData
	{
		XMFLOAT3 pos;
		XMFLOAT3 pos_predicted;
		XMFLOAT3 vel;
		float frictionCoefficient;
		float param[SPECK_SPECIAL_PARAM_N];
		float mass;
		float invMass;
		UINT code;
	};

	struct SpeckCollisionSpace
	{
		// Contains index to the cell that contains the speck and indices to all the sourounding cells.
		UINT cells[3 * 3 * 3];
		UINT count;
	};

	// Information about a single spatial hash cell on the device.
	struct SpatialHashingCellData
	{
		UINT specks[MAX_SPECKS_PER_CELL];	// array of particles
		UINT count;							// number of particles
	};

	// Information about a single static collider on the device.
	struct StaticColliderData
	{
		XMFLOAT4X4 world;
		XMFLOAT4X4 invTransposeWorld;
		UINT facesStartIndex;
		UINT facesCount;
		UINT edgesStartIndex;
		UINT edgesCount;
	};

	// This can either be a plane or an edge used for an collidion detection between a speck and a static collider.
	struct StaticColliderElementData
	{
		XMFLOAT3 vec[2];
	};

	struct SpeckContactConstraint
	{
		UINT speckIndex;
	};

	struct StaticColliderContactConstraint
	{
		UINT colliderID;
		XMFLOAT3 pos;
		XMFLOAT3 normal;
	};

	struct RigidBodyConstraint
	{
		// Index of the rigid body with this constraint.
		UINT rigidBodyIndex;
		// This value is also in SpeckRigidBodyLink structure making this a duplicate,
		// but this is to improve cache friendliness.
		XMFLOAT3 posInRigidBody;
	};

	// Information about constraints of a single speck on the device.
	struct SpeckConstraints
	{
		// From other specks
		UINT numSpeckContacts;
		SpeckContactConstraint speckContacts[NUM_SPECK_CONTACT_CONSTRAINTS_PER_SPECK];
		// From static colliders
		UINT numStaticCollider;
		StaticColliderContactConstraint staticColliderContacts[NUM_STATIC_COLLIDERS_CONTACT_CONSTRAINTS_PER_SPECK];
		// From rigid body membership (only if this speck is part of some rigid body or bodies).
		UINT numSpeckRigidBodies;
		RigidBodyConstraint speckRigidBodyIndices[NUM_RIGID_BODY_CONSTRAINTS_PER_SPECK];
		// Used for fluid simulation
		float densityConstraintLambda;
		// Position delta that will be applied after a single solver iteration.
		XMFLOAT3 appliedDeltaPos;
		UINT n;
	};

	// Information about a single external force on the device.
	struct ExternalForceData
	{
		XMFLOAT3 vec;
		int type;
	};

	// Data element that links a speck with a rigid body 
	// and defines it's relative position (in rigid body's local coordinate system).
	struct SpeckRigidBodyLink
	{
		UINT speckIndex;
		UINT rbIndex;
		XMFLOAT3 posInRigidBody;
		// Start of the block of speck links this rigid body is made of.
		UINT speckLinksBlockStart;
		// Size of the block of speck links this rigid body is made of.
		UINT speckLinksBlockCount;
	};

	// Cache for the structure above.
	struct SpeckRigidBodyLinkCache
	{
		// Center of mass multiplied by total mass of all specks in the rigid body.
		XMFLOAT3 cm;
		// Total mass of the rigid body.
		float m;
		// This is A matrix from the NVidia Flex whitepaper (only used as cache).
		XMFLOAT3X3 A;
	};

	struct RigidBodyData
	{
		// World transform of the rigid body.
		XMFLOAT4X4 world;
		// Center of mass of all specks in the rigid body.
		XMFLOAT3 c;
	};

	// Used for overwriting rigid body matrix calculated from simulation.
	struct RigidBodyUploadData
	{
		UINT movementMode;
		// World transform of the rigid body.
		XMFLOAT4X4 world;
	};
}

SpecksHandler::SpecksHandler(EngineCore &ec, World &world, std::vector<std::unique_ptr<FrameResource>> *frameResources, UINT stabilizationIteraions, UINT solverIterations, UINT substepsIterations)
	: EngineUser(ec),
	WorldUser(world),
	mCurrentFrameResource(nullptr),
	mParticleNum(0),
	mSpeckRigidBodyLinksNum(0),
	mBiggestRigidBodySpeckNum(0),
	mStabilizationIteraions(stabilizationIteraions),
	mSolverIterations(solverIterations),
	mSubstepsIterations(substepsIterations),
	mOmega(1.5f), // (1 < omega < 2) is proposed in nvidiaFlex2014
	mDeltaTime(1.0f / 60.0f),
	mTimeMultiplier(1.0f)
{
	// Extend this for special case when there are too few specks which in 
	// result makes the finding of neighbour cell IDs unstable (repetitive IDs)
	UINT maxParticleNum = MAX_SPECKS;
	if (mPrimes.empty())
	{
		mHashTableSize = maxParticleNum;
		LOG("Prime numbers not loaded!", ERROR);
	}
	else
	{
		// find the smallest prime number that is bigger than the max number of particles
		mHashTableSize = *upper_bound(mPrimes.begin(), mPrimes.end(), maxParticleNum);
	}
	BuildBuffers(frameResources);
}

SpecksHandler::~SpecksHandler()
{
}

void SpecksHandler::AddParticles(UINT num)
{
	InvalidateSpecksBuffers();
	InvalidateSpecksRenderBuffers(mParticleNum);
	mParticleNum += num;
}

void SpecksHandler::SetSpeckRadius(float speckRadius)
{
	mSpeckRadius = speckRadius;
	mCellSize = mSpeckRadius * 2.0f;
}

void SpecksHandler::BuildStaticMembers(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList)
{
	HMODULE hMod = LoadLibraryEx(ENGINE_LIBRARY_NAME, NULL, LOAD_LIBRARY_AS_DATAFILE);
	HRSRC hRes;
	if (NULL != hMod)
	{
		// Load one milion prime numbers.
		hRes = FindResource(hMod, MAKEINTRESOURCE(ID_BIN), MAKEINTRESOURCE(RT_MILION_PRIMES));
		if (NULL != hRes)
		{
			HGLOBAL hgbl = LoadResource(hMod, hRes);
			void *pData = LockResource(hgbl);
			UINT32 sizeInBytes = SizeofResource(hMod, hRes);
			mPrimes.resize(sizeInBytes / sizeof(UINT));
			memcpy(&mPrimes[0], pData, sizeInBytes);
		}

		// Load CS shaders
		UINT resArray[mCS_phasesCount] = { 
			RT_SPECKS_CS_PHASE_0,
			RT_SPECKS_CS_PHASE_1,
			RT_SPECKS_CS_PHASE_2,
			RT_SPECKS_CS_PHASE_3_0,
			RT_SPECKS_CS_PHASE_3_1,
			RT_SPECKS_CS_PHASE_4,
			RT_SPECKS_CS_PHASE_5_0,
			RT_SPECKS_CS_PHASE_5_1,
			RT_SPECKS_CS_PHASE_5_2,
			RT_SPECKS_CS_PHASE_5_3,
			RT_SPECKS_CS_PHASE_6,
			RT_SPECKS_CS_PHASE_FINAL };
		for (UINT i = 0; i < mCS_phasesCount; i++)
		{
			hRes = FindResource(hMod, MAKEINTRESOURCE(ID_CSO), MAKEINTRESOURCE(resArray[i]));
			if (NULL != hRes)
			{
				HGLOBAL hgbl = LoadResource(hMod, hRes);
				void *pData = LockResource(hgbl);
				UINT32 sizeInBytes = SizeofResource(hMod, hRes);
				ThrowIfFailed(D3DCreateBlob(sizeInBytes, mCS_phases[i].GetAddressOf()));
				memcpy((char*)mCS_phases[i]->GetBufferPointer(), pData, sizeInBytes);
			}
		}
		FreeLibrary(hMod);
	}

	// Root parameter can be a table, root descriptor or root constants.
	const int rootParametersNum = 15;
	CD3DX12_ROOT_PARAMETER slotRootParameter[rootParametersNum];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstants(gNumConstVals, 0);			// root constant (simulation related data)
	slotRootParameter[1].InitAsShaderResourceView(0, 0);			// descriptor table (for input instances)
	slotRootParameter[2].InitAsUnorderedAccessView(0, 0);			// for output instances
	slotRootParameter[3].InitAsShaderResourceView(1, 0);			// descriptor table (for static colliders)
	slotRootParameter[4].InitAsShaderResourceView(2, 0);			// descriptor table (for static collider faces)
	slotRootParameter[5].InitAsShaderResourceView(3, 0);			// descriptor table (for static collider edges)
	slotRootParameter[6].InitAsShaderResourceView(4, 0);			// descriptor table (for external forces)
	slotRootParameter[7].InitAsShaderResourceView(5, 0);			// descriptor table (for speck - rigid body links)
	slotRootParameter[8].InitAsShaderResourceView(6, 0);			// descriptor table (for rigid body uploader)
	slotRootParameter[9].InitAsUnorderedAccessView(1, 0);			// for specks (physics buffer)
	slotRootParameter[10].InitAsUnorderedAccessView(2, 0);			// for grid cells
	slotRootParameter[11].InitAsUnorderedAccessView(3, 0);			// for constraints
	slotRootParameter[12].InitAsUnorderedAccessView(4, 0);			// for collision spaces
	slotRootParameter[13].InitAsUnorderedAccessView(5, 0);			// for rigid bodies
	slotRootParameter[14].InitAsUnorderedAccessView(6, 0);			// for speck rigid body links cache

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(rootParametersNum, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

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
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));

	//
	// PSO for CS
	//
	for (UINT i = 0; i < mCS_phasesCount; i++)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC descPSO = {};
		descPSO.pRootSignature = mRootSignature.Get();
		descPSO.CS =
		{
			reinterpret_cast<BYTE*>(mCS_phases[i]->GetBufferPointer()), mCS_phases[i]->GetBufferSize()
		};
		descPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		ThrowIfFailed(device->CreateComputePipelineState(&descPSO, IID_PPV_ARGS(&mPSOs[i])));
	}
}

void SpecksHandler::ReleaseStaticMembers()
{
	mPrimes.clear();
	mRootSignature.Reset();
	for (UINT i = 0; i < mCS_phasesCount; i++)
	{
		mCS_phases[i].Reset();
		mPSOs[i].Reset();
	}
}

void SpecksHandler::BuildBuffers(std::vector<std::unique_ptr<FrameResource>> *frameResources)
{
	auto device = GetEngineCore().GetDirectXCore().GetDevice();
	auto cmdList = GetEngineCore().GetDirectXCore().GetCommandList();
	auto world = static_cast<SpeckWorld *>(&GetWorld());

	// Specks physics buffer
	vector<GPU::SpeckData> data1(MAX_SPECKS);
	for (auto &d : data1)
	{
		d.frictionCoefficient = 0.5f;
		d.code = SPECK_CODE_NORMAL;
		d.mass = 1.0f;
		d.invMass = 1.0f / d.mass;
		d.vel = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
	UINT byteSize = (UINT)data1.size() * sizeof(GPU::SpeckData);
	D3D12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	mSpecksBuffer.first = CreateDefaultBuffer(device, cmdList, &data1[0], byteSize, rd, mSpecksBuffer.second);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSpecksBuffer.first.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Cells buffer (hash table)
	vector<GPU::SpatialHashingCellData> data2(mHashTableSize);
	byteSize = (UINT)data2.size() * sizeof(GPU::SpatialHashingCellData);
	rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	// Create both buffers in swap buffer
	mGridCellsBuffer.first = CreateDefaultBuffer(device, cmdList, &data2[0], byteSize, rd, mGridCellsBuffer.second);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGridCellsBuffer.first.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Constraints buffer
	vector<GPU::SpeckConstraints> data3(MAX_SPECKS);
	byteSize = (UINT)data3.size() * sizeof(GPU::SpeckConstraints);
	rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	// Create both buffers in swap buffer
	mContactConstraintsBuffer.first = CreateDefaultBuffer(device, cmdList, &data3[0], byteSize, rd, mContactConstraintsBuffer.second);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mContactConstraintsBuffer.first.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Speck collision spaces buffer
	vector<GPU::SpeckCollisionSpace> data4(MAX_SPECKS);
	byteSize = (UINT)data4.size() * sizeof(GPU::SpeckCollisionSpace);
	rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	// Create both buffers in swap buffer
	mSpeckCollisionSpacesBuffer.first = CreateDefaultBuffer(device, cmdList, &data4[0], byteSize, rd, mSpeckCollisionSpacesBuffer.second);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSpeckCollisionSpacesBuffer.first.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Rigid bodies data
	vector<GPU::RigidBodyData> data5(MAX_RIGID_BODIES);
	byteSize = (UINT)data5.size() * sizeof(GPU::RigidBodyData);
	rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	// Create both buffers in swap buffer
	mRigidBodiesBuffer.first = CreateDefaultBuffer(device, cmdList, &data5[0], byteSize, rd, mRigidBodiesBuffer.second);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRigidBodiesBuffer.first.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Speck rigid body links cache data
	vector<GPU::SpeckRigidBodyLinkCache> data6(MAX_SPECKS);
	byteSize = (UINT)data6.size() * sizeof(GPU::SpeckRigidBodyLinkCache);
	rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	// Create both buffers in swap buffer
	mSpeckRigidBodyLinkCacheBuffer.first = CreateDefaultBuffer(device, cmdList, &data6[0], byteSize, rd, mSpeckRigidBodyLinkCacheBuffer.second);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSpeckRigidBodyLinkCacheBuffer.first.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// These buffers needs to be built for each frame resource.
	for (int i = 0; i < NUM_FRAME_RESOURCES; ++i)
	{
		// Create specks buffer
		(*frameResources)[i]->UploadBuffers.push_back(make_unique<UploadBuffer<GPU::SpeckUploadData>>(device, MAX_SPECKS, false));
		// Create static colliders buffers
		(*frameResources)[i]->UploadBuffers.push_back(make_unique<UploadBuffer<GPU::StaticColliderData>>(device, MAX_STATIC_COLLIDERS, false));
		// Create static collider faces buffers
		(*frameResources)[i]->UploadBuffers.push_back(make_unique<UploadBuffer<GPU::StaticColliderElementData>>(device, MAX_STATIC_COLLIDERS, false));
		// Create static collider edge buffers
		(*frameResources)[i]->UploadBuffers.push_back(make_unique<UploadBuffer<GPU::StaticColliderElementData>>(device, MAX_STATIC_COLLIDERS, false));
		// Create external forces buffers
		(*frameResources)[i]->UploadBuffers.push_back(make_unique<UploadBuffer<GPU::ExternalForceData>>(device, MAX_EXTERNAL_FORCES, false));
		// Create speck rigid body link buffers
		(*frameResources)[i]->UploadBuffers.push_back(make_unique<UploadBuffer<GPU::SpeckRigidBodyLink>>(device, MAX_SPECK_RIGID_BODY_LINKS, false));
		// Create rigid body uploader structures
		(*frameResources)[i]->UploadBuffers.push_back(make_unique<UploadBuffer<GPU::RigidBodyUploadData>>(device, MAX_RIGID_BODIES, false));
		
		// Create buffer for specks rendering.
		ResourcePair buffer;
		vector<SpeckInstanceData> data(MAX_SPECKS);
		UINT byteSize = (UINT)data.size() * sizeof(SpeckInstanceData);
		D3D12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
		rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		buffer.first = CreateDefaultBuffer(device, cmdList, &data[0], byteSize, rd, buffer.second);
		(*frameResources)[i]->Buffers.push_back(buffer);
	}
	mSpecks.mBufferIndex				= (UINT)((*frameResources)[0]->UploadBuffers.size() - 7);
	mStaticColliders.mBufferIndex		= (UINT)((*frameResources)[0]->UploadBuffers.size() - 6);
	mStaticColliderFaces.mBufferIndex	= (UINT)((*frameResources)[0]->UploadBuffers.size() - 5);
	mStaticColliderEdges.mBufferIndex	= (UINT)((*frameResources)[0]->UploadBuffers.size() - 4);
	mExternalForces.mBufferIndex		= (UINT)((*frameResources)[0]->UploadBuffers.size() - 3);
	mSpeckRigidBodyLink.mBufferIndex	= (UINT)((*frameResources)[0]->UploadBuffers.size() - 2);
	mRigidBodyUploader.mBufferIndex		= (UINT)((*frameResources)[0]->UploadBuffers.size() - 1);

	mSpecksRender.mBufferIndex = (UINT)((*frameResources)[0]->Buffers.size() - 1);
}

void SpecksHandler::UpdateCSPhases()
{
	auto speckWorld = static_cast<SpeckWorld *>(&GetWorld());

	// per grid cell
	phasesCSTG[0].mX = (UINT)ceilf(mHashTableSize / (float)CELLS_CS_N_THREADS);
	phasesCSTG[0].mY = 1;
	phasesCSTG[0].mZ = 1;
	phasesCSTG[0].mUseBarrierOnGridCellsBuffer = true;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[0].mName = "PHASE_0";
#endif

	// per particle, but also cells are updated
	phasesCSTG[1].mX = (UINT)ceilf(mParticleNum / (float)SPECKS_CS_N_THREADS);
	phasesCSTG[1].mY = 1;
	phasesCSTG[1].mZ = 1;
	phasesCSTG[1].mUseBarrierOnSpecksBuffer = true;
	phasesCSTG[1].mUseBarrierOnGridCellsBuffer = true;
	phasesCSTG[1].mUseBarrierOnConstraintsBuffer = true;
	phasesCSTG[1].mUseBarrierOnSpeckCollisionSpacesBuffer = true;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[1].mName = "PHASE_1";
#endif

	// per particle, integrate
	phasesCSTG[2].mX = (UINT)ceilf(mParticleNum / (float)SPECKS_CS_N_THREADS);
	phasesCSTG[2].mY = 1;
	phasesCSTG[2].mZ = 1;
	phasesCSTG[2].mUseBarrierOnSpecksBuffer = true;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[2].mName = "PHASE_2";
#endif

	// per particle, generate speck contact constraints
	phasesCSTG[3].mX = (UINT)ceilf(mParticleNum / (float)SPECKS_CS_N_THREADS);
	phasesCSTG[3].mY = 1;
	phasesCSTG[3].mZ = 1;
	phasesCSTG[3].mUseBarrierOnConstraintsBuffer = true;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[3].mName = "PHASE_3_0";
#endif

	// per particle, generate static collider contact constraints
	phasesCSTG[4].mX = (UINT)ceilf(mParticleNum / (float)SPECKS_CS_N_THREADS);
	phasesCSTG[4].mY = (UINT)speckWorld->mStaticColliders.size();
	phasesCSTG[4].mZ = 1;
	phasesCSTG[4].mUseBarrierOnConstraintsBuffer = true;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[4].mName = "PHASE_3_1";
#endif

	// per particle, stabilization iterations
	phasesCSTG[5].mX = (UINT)ceilf(mParticleNum / (float)SPECKS_CS_N_THREADS);
	phasesCSTG[5].mY = 1;
	phasesCSTG[5].mZ = 1;
	phasesCSTG[5].mUseBarrierOnSpecksBuffer = true;
	phasesCSTG[5].mRepeat = mStabilizationIteraions;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[5].mName = "PHASE_4";
#endif

	// per speck, solver iterations
	phasesCSTG[6].mX = (UINT)ceilf(mParticleNum / (float)SPECKS_CS_N_THREADS);
	phasesCSTG[6].mY = 1;
	phasesCSTG[6].mZ = 1;
	phasesCSTG[6].mUseBarrierOnSpecksBuffer = true;
	phasesCSTG[6].mRepeat = mSolverIterations * 2; // calculate and apply for each step in the solver
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[6].mName = "PHASE_5_0";
#endif

	// per speck rigid body link, calculate center of rigid body masses
	phasesCSTG[7].mX = (UINT)ceilf(mSpeckRigidBodyLinksNum / (float)SPECK_RIGID_BODY_LINKS_CS_N_THREADS);
	phasesCSTG[7].mY = 1;
	phasesCSTG[7].mZ = 1;
	// This phase uses parallel reduce so repeat this procedure log times + one additional time for initial data calculation.
	phasesCSTG[7].mRepeat = (UINT)ceilf(log2f((float)mBiggestRigidBodySpeckNum)) + 1;
	phasesCSTG[7].mUseSpeckRigidBodyLinkCacheBuffer = true;
	phasesCSTG[7].mUseBarrierOnRigidBodiesBuffer = true;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[7].mName = "PHASE_5_1";
#endif

	// per speck rigid body link, calculate rigid body world transform matrix
	phasesCSTG[8].mX = (UINT)ceilf(mSpeckRigidBodyLinksNum / (float)SPECK_RIGID_BODY_LINKS_CS_N_THREADS);
	phasesCSTG[8].mY = 1;
	phasesCSTG[8].mZ = 1;
	// This phase uses parallel reduce so repeat this procedure log times + one additional time for initial data calculation.
	phasesCSTG[8].mRepeat = (UINT)ceilf(log2f((float)mBiggestRigidBodySpeckNum)) + 1;
	phasesCSTG[8].mUseSpeckRigidBodyLinkCacheBuffer = true;
	phasesCSTG[8].mUseBarrierOnRigidBodiesBuffer = true;
	phasesCSTG[8].mUseBarrierOnConstraintsBuffer = true;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[8].mName = "PHASE_5_2";
#endif

	// per speck, solver for rigid bodies
	phasesCSTG[9].mX = (UINT)ceilf(mParticleNum / (float)SPECKS_CS_N_THREADS);
	phasesCSTG[9].mY = 1;
	phasesCSTG[9].mZ = 1;
	phasesCSTG[9].mUseBarrierOnSpecksBuffer = true;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[9].mName = "PHASE_5_3";
#endif

	// per particle, apply changes
	phasesCSTG[10].mX = (UINT)ceilf(mParticleNum / (float)SPECKS_CS_N_THREADS);
	phasesCSTG[10].mY = 1;
	phasesCSTG[10].mZ = 1;
	phasesCSTG[10].mUseBarrierOnSpecksBuffer = true;
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[10].mName = "PHASE_6";
#endif

	// final (per particle)
	phasesCSTG[11].mX = (UINT)ceilf(mParticleNum / (float)SPECKS_CS_N_THREADS);
	phasesCSTG[11].mY = 1;
	phasesCSTG[11].mZ = 1;
	phasesCSTG[11].mUseBarrierOnSpecksBuffer = true; // treba li ovo ?????
#if defined(_DEBUG) || defined(DEBUG)
	phasesCSTG[11].mName = "PHASE_FINAL";
#endif


	//phasesCSTG[5].mRepeat = 0;
	//phasesCSTG[6].mRepeat = 0;
	//phasesCSTG[7].mRepeat = 0;
	//phasesCSTG[8].mRepeat = 0;
	//phasesCSTG[9].mRepeat = 0;
	//phasesCSTG[10].mRepeat = 0;


}

void SpecksHandler::UpdateCPU(FrameResource *currentFrameResource)
{
	auto world = static_cast<SpeckWorld *>(&GetWorld());
	mPreviousFrameResource = mCurrentFrameResource;
	mCurrentFrameResource = currentFrameResource;

	// Update specks.
	if (mSpecks.mNumFramesDirty > 0)
	{
		auto upBuff = static_cast<UploadBuffer<GPU::SpeckUploadData> *>(currentFrameResource->UploadBuffers[mSpecks.mBufferIndex].get());
		GPU::SpeckUploadData data;
		for (UINT i = 0; i < (UINT)world->mSpecks.size(); ++i)
		{
			data.materialIndex = world->mSpecks[i].mMaterialIndex;
			data.code = world->mSpecks[i].mCode;
			data.mass = world->mSpecks[i].mMass;
			data.frictionCoefficient = world->mSpecks[i].mFrictionCoefficient;
			for (int j = 0; j < SPECK_SPECIAL_PARAM_N; ++j)
			{
				data.param[j] = world->mSpecks[i].mParam[j];
			}
			data.position = world->mSpecks[i].mPosition;
			upBuff->CopyData(i, data);
		}
		mSpecks.mNumFramesDirty--;
	}

	// Update static colliders.
	if (mStaticColliders.mNumFramesDirty > 0)
	{
		auto upBuff = static_cast<UploadBuffer<GPU::StaticColliderData> *>(currentFrameResource->UploadBuffers[mStaticColliders.mBufferIndex].get());
		GPU::StaticColliderData data;
		for (UINT i = 0; i < (UINT)world->mStaticColliders.size(); ++i)
		{
			XMMATRIX worldMat = XMLoadFloat4x4(&world->mStaticColliders[i].mWorld);
			XMStoreFloat4x4(&data.world, XMMatrixTranspose(worldMat));
			data.invTransposeWorld = world->mStaticColliders[i].mInvWorld; // already transposed
			data.facesStartIndex = 0;
			data.facesCount = 6;
			data.edgesStartIndex = 0;
			data.edgesCount = 0;
			upBuff->CopyData(i, data);
		}
		mStaticColliders.mNumFramesDirty--;
	}

	// Update static collider faces.
	if (mStaticColliderFaces.mNumFramesDirty > 0)
	{
		auto upBuff = static_cast<UploadBuffer<GPU::StaticColliderElementData> *>(currentFrameResource->UploadBuffers[mStaticColliderFaces.mBufferIndex].get());
		GPU::StaticColliderElementData data;

		data.vec[0] = XMFLOAT3(0.5f, 0.5f, 0.5f);
		data.vec[1] = XMFLOAT3(1.0f, 0.0f, 0.0f);
		upBuff->CopyData(0, data);
		data.vec[0] = XMFLOAT3(0.5f, 0.5f, 0.5f);
		data.vec[1] = XMFLOAT3(0.0f, 1.0f, 0.0f);
		upBuff->CopyData(1, data);
		data.vec[0] = XMFLOAT3(0.5f, 0.5f, 0.5f);
		data.vec[1] = XMFLOAT3(0.0f, 0.0f, 1.0f);
		upBuff->CopyData(2, data);

		data.vec[0] = XMFLOAT3(-0.5f, -0.5f, -0.5f);
		data.vec[1] = XMFLOAT3(-1.0f, 0.0f, 0.0f);
		upBuff->CopyData(3, data);
		data.vec[0] = XMFLOAT3(-0.5f, -0.5f, -0.5f);
		data.vec[1] = XMFLOAT3(0.0f, -1.0f, 0.0f);
		upBuff->CopyData(4, data);
		data.vec[0] = XMFLOAT3(-0.5f, -0.5f, -0.5f);
		data.vec[1] = XMFLOAT3(0.0f, 0.0f, -1.0f);
		upBuff->CopyData(5, data);

		mStaticColliderFaces.mNumFramesDirty--;
	}

	// Update external forces.
	if (mExternalForces.mNumFramesDirty > 0)
	{
		auto upBuff = static_cast<UploadBuffer<GPU::ExternalForceData> *>(currentFrameResource->UploadBuffers[mExternalForces.mBufferIndex].get());
		GPU::ExternalForceData data;
		for (UINT i = 0; i < (UINT)world->mExternalForces.size(); ++i)
		{
			data.type = world->mExternalForces[i].mType;
			data.vec = world->mExternalForces[i].mVec;
			upBuff->CopyData(i, data);
		}
		mExternalForces.mNumFramesDirty--;
	}

	// Update rigid body links.
	if (mSpeckRigidBodyLink.mNumFramesDirty > 0)
	{
		auto upBuff = static_cast<UploadBuffer<GPU::SpeckRigidBodyLink> *>(currentFrameResource->UploadBuffers[mSpeckRigidBodyLink.mBufferIndex].get());
		GPU::SpeckRigidBodyLink data;
		UINT counter = 0;
		mBiggestRigidBodySpeckNum = 0;
		for (UINT i = 0; i < (UINT)world->mSpeckRigidBodyData.size(); ++i)
		{
			// This arrangement of data will guarantee that there will be unseparated blocks of the same rigid body specs.
			SpeckRigidBodyData &rbd = world->mSpeckRigidBodyData[i];
			// Update biggest rigid body.
			if (mBiggestRigidBodySpeckNum < rbd.mLinks.size())
				mBiggestRigidBodySpeckNum = (UINT)rbd.mLinks.size();
			UINT speckLinksBlockStart = counter;
			for (UINT j = 0; j < (UINT)rbd.mLinks.size(); ++j)
			{
				data.posInRigidBody = rbd.mLinks[j].mPosInRigidBody;
				data.speckIndex = rbd.mLinks[j].mSpeckIndex;
				data.rbIndex = i;
				data.speckLinksBlockStart = speckLinksBlockStart;
				data.speckLinksBlockCount = (UINT)rbd.mLinks.size();
				upBuff->CopyData(counter++, data);
			}
		}
		mSpeckRigidBodyLink.mNumFramesDirty--;
		mSpeckRigidBodyLinksNum = counter;
	}

	// Update rigid body uploader
	if (mRigidBodyUploader.mNumFramesDirty > 0)
	{
		auto upBuff = static_cast<UploadBuffer<GPU::RigidBodyUploadData> *>(currentFrameResource->UploadBuffers[mRigidBodyUploader.mBufferIndex].get());
		GPU::RigidBodyUploadData data;
		UINT counter = 0;
		for (UINT i = 0; i < (UINT)world->mSpeckRigidBodyData.size(); ++i)
		{
			if (world->mSpeckRigidBodyData[i].mRBData.updateToGPU)
			{
				RigidBodyData &rbData = world->mSpeckRigidBodyData[i].mRBData;
				data.movementMode = rbData.movementMode;
				data.world = rbData.mWorld;
				upBuff->CopyData(i, data);
			}
		}
		mRigidBodyUploader.mNumFramesDirty--;

		// All frames have been updated, set all flags to false so that for the next update of some
		// rigid body, only that one will be updated.
		if (mRigidBodyUploader.mNumFramesDirty <= 0)
		{
			for (UINT i = 0; i < (UINT)world->mSpeckRigidBodyData.size(); ++i)
			{
				world->mSpeckRigidBodyData[i].mRBData.updateToGPU = false;
			}
		}
	}
}

void SpecksHandler::UpdateGPU()
{
	float newTime = MathHelper::FixTimeStep(GetEngineCore().GetTimer()->DeltaTime());
	float timeLerpSpeed = 0.01f;
	mDeltaTime = (1.0f - timeLerpSpeed) * mDeltaTime + timeLerpSpeed * newTime;
	float deltaTime = mDeltaTime * mTimeMultiplier;
	//deltaTime = 0.001f;
	deltaTime /= mSubstepsIterations;

	for (UINT i = 0; i < mSubstepsIterations; i++)
	{
		UpdateGPU_substep(deltaTime);
	}
}

void SpecksHandler::UpdateGPU_substep(float deltaTime)
{
	GraphicsDebuggerAnnotator gda(GetEngineCore().GetDirectXCore(), "SpecksHandlerUpdateGPU");
	UpdateCSPhases();
	auto device = GetEngineCore().GetDirectXCore().GetDevice();
	auto cmdList = GetEngineCore().GetDirectXCore().GetCommandList();
	auto world = static_cast<SpeckWorld *>(&GetWorld());
	auto staticColliderBuffer = static_cast<UploadBufferBase *>(mCurrentFrameResource->UploadBuffers[mStaticColliders.mBufferIndex].get());
	auto staticColliderFaceBuffer = static_cast<UploadBufferBase *>(mCurrentFrameResource->UploadBuffers[mStaticColliderFaces.mBufferIndex].get());
	auto staticColliderEdgeBuffer = static_cast<UploadBufferBase *>(mCurrentFrameResource->UploadBuffers[mStaticColliderEdges.mBufferIndex].get());
	auto externalForcesBuffer = static_cast<UploadBufferBase *>(mCurrentFrameResource->UploadBuffers[mExternalForces.mBufferIndex].get());
	auto speckRigidBodyLinkBuffer = static_cast<UploadBufferBase *>(mCurrentFrameResource->UploadBuffers[mSpeckRigidBodyLink.mBufferIndex].get());
	auto rigidBodyUploader = static_cast<UploadBufferBase *>(mCurrentFrameResource->UploadBuffers[mRigidBodyUploader.mBufferIndex].get());
	auto upBuff5 = static_cast<UploadBufferBase *>(mCurrentFrameResource->UploadBuffers[mSpecks.mBufferIndex].get());
	ID3D12Resource *writeToResource = mCurrentFrameResource->Buffers[mSpecksRender.mBufferIndex].first.Get();
	ID3D12Resource *readFromResource = upBuff5->Resource();

	// Update the specks
	cmdList->SetComputeRootSignature(mRootSignature.Get());
	// Set the constant buffer.
	byte val[4 * gNumConstVals];
	memcpy(&val[0 * 4], &mParticleNum, sizeof(UINT));
	memcpy(&val[1 * 4], &mHashTableSize, sizeof(UINT));

	memcpy(&val[2 * 4], &mSpeckRadius, sizeof(float));
	memcpy(&val[3 * 4], &mCellSize, sizeof(float));

	UINT numStaticColliders = (UINT)world->mStaticColliders.size();
	memcpy(&val[4 * 4], &numStaticColliders, sizeof(UINT));
	UINT numExternalForces = (UINT)world->mExternalForces.size();
	memcpy(&val[5 * 4], &numExternalForces, sizeof(UINT));
	memcpy(&val[6 * 4], &mSpeckRigidBodyLinksNum, sizeof(UINT));
	memcpy(&val[7 * 4], &deltaTime, sizeof(float));
	memcpy(&val[8 * 4], &mOmega, sizeof(float));
	memcpy(&val[9 * 4], &mSpecksRender.mInitializeSpecksStartIndex, sizeof(UINT));
	UINT phaseIteration = 0;
	memcpy(&val[10 * 4], &phaseIteration, sizeof(UINT));
	UINT numPhaseIterations = 1;
	memcpy(&val[11 * 4], &numPhaseIterations, sizeof(UINT));
	cmdList->SetComputeRoot32BitConstants(0, gNumConstVals, reinterpret_cast<void*>(&val), 0);

	// Bind the input and output buffers.
	cmdList->SetComputeRootShaderResourceView(1, readFromResource->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(2, writeToResource->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(3, staticColliderBuffer->Resource()->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(4, staticColliderFaceBuffer->Resource()->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(5, staticColliderEdgeBuffer->Resource()->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(6, externalForcesBuffer->Resource()->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(7, speckRigidBodyLinkBuffer->Resource()->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(8, rigidBodyUploader->Resource()->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(9, mSpecksBuffer.first.Get()->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(10, mGridCellsBuffer.first.Get()->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(11, mContactConstraintsBuffer.first.Get()->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(12, mSpeckCollisionSpacesBuffer.first.Get()->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(13, mRigidBodiesBuffer.first.Get()->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(14, mSpeckRigidBodyLinkCacheBuffer.first.Get()->GetGPUVirtualAddress());

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(writeToResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	for (UINT i = 0; i < mCS_phasesCount; i++)
	{
		if (phasesCSTG[i].GetTotalCount() == 0)
			continue;

#if defined(_DEBUG) || defined(DEBUG)
		GraphicsDebuggerAnnotator gda(GetEngineCore().GetDirectXCore(), phasesCSTG[i].mName);
#endif

		for (UINT j = 0; j < phasesCSTG[i].mRepeat; j++)
		{
			// Update the iteration value.
			memcpy(&val[10 * 4], &j, sizeof(UINT));
			memcpy(&val[11 * 4], &phasesCSTG[i].mRepeat, sizeof(UINT));
			cmdList->SetComputeRoot32BitConstants(0, gNumConstVals, reinterpret_cast<void*>(&val), 0);

			cmdList->SetPipelineState(mPSOs[i].Get());
			// Begin with current phase
			cmdList->Dispatch(phasesCSTG[i].mX, phasesCSTG[i].mY, phasesCSTG[i].mZ);

			if (phasesCSTG[i].mUseBarrierOnSpecksBuffer)
				cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(mSpecksBuffer.first.Get()));
			if (phasesCSTG[i].mUseBarrierOnGridCellsBuffer)
				cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(mGridCellsBuffer.first.Get()));
			if (phasesCSTG[i].mUseBarrierOnConstraintsBuffer)
				cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(mContactConstraintsBuffer.first.Get()));
			if (phasesCSTG[i].mUseBarrierOnSpeckCollisionSpacesBuffer)
				cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(mSpeckCollisionSpacesBuffer.first.Get()));
			if (phasesCSTG[i].mUseBarrierOnRigidBodiesBuffer)
				cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(mRigidBodiesBuffer.first.Get()));
			if (phasesCSTG[i].mUseSpeckRigidBodyLinkCacheBuffer)
				cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(mSpeckRigidBodyLinkCacheBuffer.first.Get()));
		}
	}
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(writeToResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Just one update per flag, so set it to false.
	mSpecksRender.mInitializeSpecksStartIndex = INT_MAX;
}

void SpecksHandler::InvalidateSpecksRenderBuffers(UINT startIndex)
{
	if (startIndex < mSpecksRender.mInitializeSpecksStartIndex)
		mSpecksRender.mInitializeSpecksStartIndex = startIndex;
}
