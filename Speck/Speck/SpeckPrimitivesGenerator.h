
#ifndef SPECK_PRIMITIVES_GENERATOR_H
#define SPECK_PRIMITIVES_GENERATOR_H

#include <SpeckEngineDefinitions.h>
#include <WorldUser.h>

class SpeckPrimitivesGenerator : public Speck::WorldUser
{
public:
	SpeckPrimitivesGenerator(Speck::World &world);
	~SpeckPrimitivesGenerator();

	int GenerateBox(int nx, int ny, int nz, const char *materialName, const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &rotation);
	int GenerateBox(int nx, int ny, int nz, const char *materialName, const DirectX::XMFLOAT4X4 &transform);

	enum ConstrainedRigidBodyPairType { HingeJoint, AsymetricHingeJoint, BallAndSocket, StiffJoint};
	int GeneratreConstrainedRigidBodyPair(ConstrainedRigidBodyPairType type, const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &rotation);

private:
	float mSpeckRadius;
	float mSpeckMass;
	float mSpeckFrictionCoefficient;
};

#endif
