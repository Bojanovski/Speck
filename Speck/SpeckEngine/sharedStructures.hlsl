
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

#endif
