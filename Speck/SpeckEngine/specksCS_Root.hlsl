
#ifndef SPECKS_CS_ROOT_HLSL
#define SPECKS_CS_ROOT_HLSL

#include "cpuGpuDefines.txt"
#include "sharedStructures.hlsl"
#include "MathHelper.hlsl"

#define FLT_MAX								3.402823466e+38f
#define COLLISION_DETECTION_MULTIPLIER		1.2f
#define PI 3.14159265359f
#define PI_DIV_2 (PI*0.5f)
#define	TWO_PI (PI*2.0f)

cbuffer cbSettings				: register(b0)
{
	uint gParticleNum;
	uint gHashTableSize;

	float gSpeckRadius;
	float gCellSize;

	uint gNumStaticColliders;
	uint gNumExternalForces;
	uint gNumSpeckRigidBodyLinks;

	float gDeltaTime;
	// Rate of successive over-relaxation (SOR).
	float gOmega;
	// This index marks the position of specks that will be set to the position written in gInstancesIn buffer.
	// Also their velocities will be set to zero.
	uint gInitializeSpecksStartIndex;
	// For repetitive phases this number represents current iteration.
	uint gPhaseIteration;
	// For repetitive phases this number represents total number of iterations.
	uint gNumPhaseIterations;
};

// Helper structure used to pass the information about specks to the device.
struct SpeckUploadData
{
	// Position of the speck
	float3 position;
	// Type of the speck is coded into this variable
	uint code;
	// Mass of this speck
	float mass;
	// Friction coefficient
	float frictionCoefficient;
	// Special parameters that depend on speck type
	float param[SPECK_SPECIAL_PARAM_N];
	// Used for rendering
	uint materialIndex;
};

// Information about a single speck on the device used for simulation
struct SpeckData
{
	float3 pos;
	float3 pos_predicted;
	float3 vel;
	float frictionCoefficient;
	float param[SPECK_SPECIAL_PARAM_N];
	float mass;
	float invMass;
	uint code;
};

// Contains index to the cell that contains the speck and indices to all the sourounding cells.
struct SpeckCollisionSpace
{
	uint cells[3*3*3];
	uint count;
};

// Information about a single spatial hash cell on the device.
struct SpatialHashingCellData
{
	uint specks[MAX_SPECKS_PER_CELL];	// array of specks
	uint count;							// number of specks
};

// Information about a single static collider on the device.
struct StaticColliderData
{
	float4x4 world;
	float4x4 invTransposeWorld;
	uint facesStartIndex;
	uint facesCount;
	uint edgesStartIndex;
	uint edgesCount;
};

// This can either be a plane or an edge used for an collision detection between a speck and a static collider.
struct StaticColliderElementData
{
	float3 vec[2];
};

struct SpeckContactConstraint
{
	uint speckIndex;
};

struct StaticColliderContactConstraint
{
	uint colliderID;
	float3 pos;
	float3 normal;
};

struct RigidBodyConstraint
{
	// Index of the rigid body with this constraint.
	uint rigidBodyIndex;
	// This value is also in SpeckRigidBodyLink structure making this a duplicate,
	// but this is to improve cache friendliness.
	float3 posInRigidBody;
};

struct SpeckConstraints
{
	// From other specks
	uint numSpeckContacts;
	SpeckContactConstraint speckContacts[NUM_SPECK_CONTACT_CONSTRAINTS_PER_SPECK];
	// From static colliders
	uint numStaticCollider;
	StaticColliderContactConstraint staticColliderContacts[NUM_STATIC_COLLIDERS_CONTACT_CONSTRAINTS_PER_SPECK];
	// From rigid body membership (only if this speck is part of some rigid body or bodies).
	uint numSpeckRigidBodies;
	RigidBodyConstraint speckRigidBodyConstraint[NUM_RIGID_BODY_CONSTRAINTS_PER_SPECK];
	// Used for fluid simulation
	float densityConstraintLambda;
	// Position delta that will be applied after a single solver iteration.
	float3 appliedDeltaPos;
	uint n;
};

struct ExternalForceData
{
	float3 vec;
	int type;
};

// Data element that links a speck with a rigid body 
// and defines it's relative position (in rigid body's local coordinate system).
struct SpeckRigidBodyLink
{
	uint speckIndex;
	uint rbIndex;
	// Denoted as ri in NVidia's whitepaper.
	float3 posInRigidBody;
	// Start of the block of speck links this rigid body is made of.
	uint speckLinksBlockStart;
	// Size of the block of speck links this rigid body is made of.
	uint speckLinksBlockCount;
};

// Cache for the structure above.
struct SpeckRigidBodyLinkCache
{
	// Center of mass multiplied by total mass of all specks in the rigid body.
	float3 cm;
	// Total mass of the rigid body.
	float m;
	// This is deformed shape’s covariance matrix A matrix from the NVidia Flex whitepaper.
	float3x3 A;
};

// Used for overwriting rigid body matrix calculated from simulation.
struct RigidBodyUploadData
{
	uint movementMode;
	// World transform of the rigid body.
	float4x4 world;
};

// Input specks
StructuredBuffer<SpeckUploadData> gInstancesIn							: register(t0);
RWStructuredBuffer<InstanceData> gInstancesOut							: register(u0);

// Static colliders buffer
StructuredBuffer<StaticColliderData> gStaticColliders					: register(t1);
StructuredBuffer<StaticColliderElementData> gStaticColliderFaces		: register(t2);
StructuredBuffer<StaticColliderElementData> gStaticColliderEdges		: register(t3);

// External forces buffer
StructuredBuffer<ExternalForceData> gExternalForces						: register(t4);

// Rigid body - speck links.
StructuredBuffer<SpeckRigidBodyLink> gSpeckRigidBodyLinks				: register(t5);

// Rigid body uploader structures.
StructuredBuffer<RigidBodyUploadData> gRigidBodyUploader				: register(t6);

// Specks, physics buffer
RWStructuredBuffer<SpeckData> gSpecks									: register(u1);

// Spatial hashing cells
RWStructuredBuffer<SpatialHashingCellData> gSPCells						: register(u2);

// Constraints read and write buffers
RWStructuredBuffer<SpeckConstraints> gSpecksConstraints					: register(u3);

// Specks collision space buffer.
RWStructuredBuffer<SpeckCollisionSpace> gSpeckCollisionSpaces			: register(u4);

// Rigid body data
RWStructuredBuffer<RigidBodyData> gRigidBodies							: register(u5);

// Speck rigid body links cache data
RWStructuredBuffer<SpeckRigidBodyLinkCache> gSpeckRigidBodyLinkCache	: register(u6);

//
// Utility functions
//
uint calcGridHash(int3 gridPos, uint numBuckets)
{
	gridPos += int3(100000, 100000, 100000);


	// From: Optimized Spatial Hashing for Collision Detection of Deformable Objects Article - December 2003
	const uint p1 = 73856093;   // some large primes 
	const uint p2 = 19349663;
	const uint p3 = 83492791;
	uint n = p1*gridPos.x ^ p2*gridPos.y ^ p3*gridPos.z;
	n %= numBuckets;
	return n;
}

float GetDistanceFromPlane(float3 pos, float3 pointOnPlane, float3 planeNormal)
{
	float3 dir = pos - pointOnPlane;
	float dist = dot(dir, planeNormal);
	return dist;
}

// Kernel for density estimation. (from [Muuller et al. 2003])
float W_poly6(float r, float h)
{
	if (0.0f <= r && r <= h)
		return 315.0f / (64.0f * PI * pow(abs(h), 9.0f)) * pow(h*h - r*r, 3.0f);
	else
		return 0.0f;
}

// Kernel for density estimation, gradient. (from [Muuller et al. 2003])
float W_poly6_d(float r, float h)
{
	if (0.0f <= r && r <= h)
		return -945.0f / (64.0f * PI * pow(h, 9.0f)) * r * pow(h*h - r*r, 3.0f);
	else
		return 0.0f;
}

// Kernel for density estimation. (from [Muuller et al. 2003])
float W_spiky(float r, float h)
{
	if (0.0f <= r && r <= h)
		return 15.0f / (PI * pow(h, 6.0f)) * pow(h - r, 3.0f);
	else
		return 0.0f;
}

// Kernel for density estimation, gradient. (from [Muuller et al. 2003])
float W_spiky_d(float r, float h)
{
	if (0.0f <= r && r <= h)
		return -45.0f / (PI * pow(h, 6.0f)) * pow(h - r, 2.0f);
	else
		return 0.0f;
}

// Kernel for density estimation, gradient. (from [Muuller et al. 2003])
float W_viscosity(float r, float h)
{
	if (0.0f <= r && r <= h)
		return 15.0f / (2.0f * PI * pow(h, 3.0f))
		* (-pow(r, 3.0f) / (2.0f * pow(h, 3.0f)) + (r*r) / (h*h) + abs(h) / (2.0f*r) - 1.0f);
	else
		return 0.0f;
}

// Spline fucntion used for simulating cohesion in fluids
// (from: Versatile Surface Tension and Adhesion for SPH Fluids)
float C_akinci(float r, float h)
{
	if (2.0f*r > h && r <= h)
	{
		return 32.0f / (PI * pow(abs(h), 9.0f))
			* (pow(h - r, 3.0f) * pow(r, 3.0f));
	}
	else if (r > 0.0f && 2.0f*r <= h)
	{
		return 32.0f / (PI * pow(abs(h), 9.0f))
			* (2.0f*pow(h - r, 3.0f) * pow(r, 3.0f) - pow(h, 6.0f)/64.0f);
	}
	else
	{
		return 0.0f;
	}
}

#endif
