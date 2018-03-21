
#ifndef SPECK_PRIMITIVES_GENERATOR_H
#define SPECK_PRIMITIVES_GENERATOR_H

#include <SpeckEngineDefinitions.h>
#include <WorldUser.h>
#include <WorldCommands.h>

class SpeckPrimitivesGenerator : public Speck::WorldUser
{
public:
	SpeckPrimitivesGenerator(Speck::World &world);
	~SpeckPrimitivesGenerator();

	void SetSpeckMass(float newSpeckMass) { mSpeckMass = newSpeckMass; }
	void SetSpeckFrictionCoefficient(float newSpeckFrictionCoefficient) { mSpeckFrictionCoefficient = newSpeckFrictionCoefficient; }

	int GenerateBox(int nx, int ny, int nz, const char *materialName, bool useSkin, const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &rotation);
	int GenerateBox(int nx, int ny, int nz, const char *materialName, bool useSkin, const DirectX::XMFLOAT4X4 &transform);

	enum ConstrainedRigidBodyPairType { HingeJoint, AsymetricHingeJoint, BallAndSocket, StiffJoint, UniversalJoint};
	int GeneratreConstrainedRigidBodyPair(ConstrainedRigidBodyPairType type, const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &rotation);

private:
	void GenerateCubicStructure(int nx, int ny, int nz, std::vector<Speck::WorldCommands::NewSpeck> *outStructure, DirectX::CXMMATRIX transformation);
	void GenerateCubicStructure(int nx, int ny, int nz, float *outWidth, float *outHeight, float *outDepth, std::vector<Speck::WorldCommands::NewSpeck> *outStructure, DirectX::CXMMATRIX transformation);

	float mSpeckRadius;
	float mSpeckMass;
	float mSpeckFrictionCoefficient;
};

#endif
