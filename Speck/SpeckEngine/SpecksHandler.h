
#ifndef SPECKS_HANDLER_H
#define SPECKS_HANDLER_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"
#include "EngineUser.h"
#include "WorldUser.h"

namespace Speck
{
	struct FrameResource;

	class SpecksHandler : public EngineUser, public WorldUser
	{
	public:
		SpecksHandler(EngineCore &ec, World &world, std::vector<std::unique_ptr<FrameResource>> *frameResources, UINT particleNum, UINT stabilizationIteraions, UINT solverIterations);
		~SpecksHandler();

		// CPU related update
		void UpdateCPU(FrameResource *currentFrameResource);
		// GPU related update
		void UpdateGPU();
		void InvalidateSpecksRenderBuffers(UINT startIndex);
		void InvalidateSpecksBuffers() { mSpecks.mNumFramesDirty = NUM_FRAME_RESOURCES; }
		void InvalidateStaticCollidersBuffers() { mStaticColliders.mNumFramesDirty = NUM_FRAME_RESOURCES; }
		void InvalidateExternalForcesBuffers() { mExternalForces.mNumFramesDirty = NUM_FRAME_RESOURCES; }
		void InvalidateRigidBodyLinksBuffers() { mSpeckRigidBodyLink.mNumFramesDirty = NUM_FRAME_RESOURCES; }
		void AddParticles(UINT num);
		UINT GetSpecksBufferIndex() { return mSpecksRender.mBufferIndex; }
		
		static float GetSpeckRadius() { return mSpeckRadius; }
		static void BuildStaticMembers(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList, float speckRadius);
		static void ReleaseStaticMembers();

	private:
		void BuildBuffers(std::vector<std::unique_ptr<FrameResource>> *frameResources);
		void UpdateCSPhases();

	private:
		FrameResource *mPreviousFrameResource;
		FrameResource *mCurrentFrameResource;

		// List of specks used for simulation.
		ResourcePair mSpecksBuffer;
		// List of grid cells used for collision detection.
		ResourcePair mGridCellsBuffer;
		// List of constraints.
		ResourcePair mContactConstraintsBuffer;
		// List of speck collision spaces.
		ResourcePair mSpeckCollisionSpacesBuffer;
		// List of rigid bodies.
		ResourcePair mRigidBodiesBuffer;
		// List of speck rigid body links cache.
		ResourcePair mSpeckRigidBodyLinkCacheBuffer;

		static Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		static const UINT mCS_phasesCount = 12;
		static Microsoft::WRL::ComPtr<ID3DBlob> mCS_phases[mCS_phasesCount];
		static Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSOs[mCS_phasesCount];
		static std::vector<UINT> mPrimes;
		// Dimensions of specks and cells.
		static float mSpeckRadius;
		static float mCellSize;

		// Compute shader thread group for each phase.
		struct : ComputeShaderThreadGroups
		{
			// Resource barriers
			bool mUseBarrierOnSpecksBuffer = false;
			bool mUseBarrierOnGridCellsBuffer = false;
			bool mUseBarrierOnConstraintsBuffer = false;
			bool mUseBarrierOnSpeckCollisionSpacesBuffer = false;
			bool mUseBarrierOnRigidBodiesBuffer = false;
			bool mUseSpeckRigidBodyLinkCacheBuffer = false;
#if defined(_DEBUG) || defined(DEBUG)
			std::string mName = "unknown";
#endif
		} phasesCSTG[mCS_phasesCount];

		// Number of particles in the simulation.
		UINT mParticleNum;
		// Number of speck rigid body links in the simulation.
		UINT mSpeckRigidBodyLinksNum;
		// Number of specks in a rigid body with the highest number of specks.
		UINT mBiggestRigidBodySpeckNum;
		// How many buckets (cells) will there be in the simulation.
		UINT mHashTableSize;
		// Used for stabilization pass (fixing initial values).
		const UINT mStabilizationIteraions;
		// Used for main constraint resolution pass.
		const UINT mSolverIterations;
		// Rate of successive over-relaxation (SOR).
		const float mOmega;
		// Time will be interpolated between frames to prevent sudden 
		// changes in integration and hopping of the specks.
		float mDeltaTime;

		// Buffers
		struct BufferStruct
		{
			UINT mBufferIndex;							// Frame resource buffer index
			int mNumFramesDirty = NUM_FRAME_RESOURCES;	// When data changes mNumFramesDirty gets set to this initial value.
		};

		//Index of the buffer used for rendering.
		struct
		{
			UINT mBufferIndex;							// Frame resource buffer index
			UINT mInitializeSpecksStartIndex = 0;		// Marks the start index of the part of the array that will be updated.
		} mSpecksRender;

		//
		// Upload buffer indices.
		//
		BufferStruct mSpecks;
		BufferStruct mStaticColliders;
		BufferStruct mStaticColliderFaces;
		BufferStruct mStaticColliderEdges;
		BufferStruct mExternalForces;
		BufferStruct mSpeckRigidBodyLink;
	};
}

#endif