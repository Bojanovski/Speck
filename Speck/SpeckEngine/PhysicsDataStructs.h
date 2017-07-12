
#ifndef PHYSICS_DATA_STRUCTS_H
#define PHYSICS_DATA_STRUCTS_H

#include "SpeckEngineDefinitions.h"

namespace Speck
{
	struct StaticCollider
	{
		int mID;
		DirectX::XMFLOAT4X4 mWorld;
		DirectX::XMFLOAT4X4 mInvWorld;
	};

	struct ExternalForces
	{
		enum Types
		{
			Acceleration = FORCE_TYPE_ACCELERATION,
			NoType
		};

		int mID;
		DirectX::XMFLOAT3 mVec;
		Types mType;
	};

	struct SpeckRigidBodyLink
	{
		// Index of the speck that is part of this rigid body.
		UINT mSpeckIndex;
		// Position of the speck in the local space of the rigid body.
		DirectX::XMFLOAT3 mPosInRigidBody;
	};

	struct RigidBodyData
	{
		bool updateToGPU;
		UINT movementMode;
		DirectX::XMFLOAT4X4 mWorld;
	};

	struct SpeckRigidBodyData
	{
		std::vector<SpeckRigidBodyLink> mLinks;
		RigidBodyData mRBData;
	};

	struct SpeckData
	{
		// Position of the speck
		DirectX::XMFLOAT3 mPosition;
		// Type of the speck is coded into this variable
		UINT mCode;
		// Mass of this speck
		float mMass;
		// Friction coefficient
		float mFrictionCoefficient;
		// Special parameters that depend on speck type
		float mParam[SPECK_SPECIAL_PARAM_N];
		// Used for rendering
		UINT mMaterialIndex;
	};
}

#endif
