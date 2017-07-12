
#ifndef SHARED_STRUCTURES_H
#define SHARED_STRUCTURES_H

struct InstanceData
{
	float3		Position;
	uint		MaterialIndex;
};

struct MaterialData
{
	float4   DiffuseAlbedo;
	float3   FresnelR0;
	float    Roughness;
	float4x4 MatTransform;
};

struct RigidBodyData
{
	// World transform of the rigid body.
	float4x4 world;
	// Center of mass of all specks in the rigid body.
	float3 c;
};

#endif
