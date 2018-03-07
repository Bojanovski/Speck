
#ifndef STATIC_PRIMITIVES_GENERATOR_H
#define STATIC_PRIMITIVES_GENERATOR_H

#include <SpeckEngineDefinitions.h>

class StaticPrimitivesGenerator
{
public:
	StaticPrimitivesGenerator(Speck::World &world);
	~StaticPrimitivesGenerator();

	void GenerateBox(const DirectX::XMFLOAT3 &scale, const char *materialName, const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &rotation);

private:
	Speck::World &mWorld;
};

#endif
