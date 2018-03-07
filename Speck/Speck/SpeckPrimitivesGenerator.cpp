
#include "SpeckPrimitivesGenerator.h"
#include <World.h>
#include <WorldCommands.h>

using namespace std;
using namespace DirectX;
using namespace Speck;

SpeckPrimitivesGenerator::SpeckPrimitivesGenerator(World &world)
	: WorldUser(world)
{
	WorldCommands::GetWorldPropertiesCommand gwp;
	WorldCommands::GetWorldPropertiesCommandResult gwpr;
	GetWorld().ExecuteCommand(gwp, &gwpr);
	mSpeckRadius = gwpr.speckRadius;
	mSpeckMass = 1.0f;
	mSpeckFrictionCoefficient = 0.6f;
}

SpeckPrimitivesGenerator::~SpeckPrimitivesGenerator()
{
}

int SpeckPrimitivesGenerator::GenerateBox(int nx, int ny, int nz, const char *materialName, const XMFLOAT3 & position, const XMFLOAT3 & rotation)
{
	XMMATRIX mat = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z) * XMMatrixTranslation(position.x, position.y, position.z);
	XMFLOAT4X4 transform;
	XMStoreFloat4x4(&transform, mat);
	return GenerateBox(nx, ny, nz, materialName, transform);
}

int SpeckPrimitivesGenerator::GenerateBox(int nx, int ny, int nz, const char * materialName, const XMFLOAT4X4 &transform)
{
	XMMATRIX mat = XMLoadFloat4x4(&transform);
	float dx, dy, dz, width, height, depth, x, y, z;
	WorldCommands::AddSpecksCommandResult commandResult;

	// add the specks
	WorldCommands::AddSpecksCommand asrbc;
	asrbc.speckType = WorldCommands::SpeckType::RigidBody;
	asrbc.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbc.speckMass = mSpeckMass;
	asrbc.newSpecks.resize(nx * ny * nz);
	width = (nx - 1) * 2.0f * mSpeckRadius;
	height = (ny - 1) * 2.0f * mSpeckRadius;
	depth = (nz - 1) * 2.0f * mSpeckRadius;
	x = -0.5f*width;
	y = -0.5f*height;
	z = -0.5f*depth;
	dx = 2.0f * mSpeckRadius;
	dy = 2.0f * mSpeckRadius;
	dz = 2.0f * mSpeckRadius;
	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				WorldCommands::NewSpeck speck;
				XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3Transform(pos, mat);
				XMStoreFloat3(&speck.position, pos);
				int index = i*ny*nz + j*nz + k;
				asrbc.newSpecks[index] = speck;
			}
		}
	}
	GetWorld().ExecuteCommand(asrbc, &commandResult);
	int specksAdded = (int)asrbc.newSpecks.size();

	// add the mesh skin
	WorldCommands::AddRenderItemCommand aric;
	aric.geometryName = "shapeGeo";
	aric.materialName = materialName;
	aric.meshName = "box";
	aric.type = WorldCommands::RenderItemType::SpeckRigidBody;
	aric.speckRigidBodyRenderItem.rigidBodyIndex = commandResult.rigidBodyIndex;
	aric.speckRigidBodyRenderItem.localTransform.MakeIdentity();
	aric.staticRenderItem.worldTransform.mS = XMFLOAT3(width + 2.0f * mSpeckRadius, height + 2.0f * mSpeckRadius, depth + 2.0f * mSpeckRadius);
	GetWorld().ExecuteCommand(aric);

	return specksAdded;
}

int SpeckPrimitivesGenerator::GeneratreConstrainedRigidBodyPair(ConstrainedRigidBodyPairType type, const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &rotation)
{
	XMMATRIX mat = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z) * XMMatrixTranslation(position.x, position.y, position.z);
	int nx, ny, nz;
	float dx, dy, dz, width, height, depth, x, y, z;
	WorldCommands::AddSpecksCommandResult commandResult;

	// first body
	WorldCommands::AddSpecksCommand asrbc1;
	asrbc1.speckType = WorldCommands::SpeckType::RigidBody;
	asrbc1.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbc1.speckMass = mSpeckMass;
	nx = 2;
	ny = 4;
	nz = 2;
	width = (nx - 1) * 2.0f * mSpeckRadius;
	height = (ny - 1) * 2.0f * mSpeckRadius;
	depth = (nz - 1) * 2.0f * mSpeckRadius;
	x = -0.5f*width;
	y = -0.5f*height + (0.5f*height + 2.0f*mSpeckRadius);
	z = -0.5f*depth;
	dx = 2.0f * mSpeckRadius;
	dy = 2.0f * mSpeckRadius;
	dz = 2.0f * mSpeckRadius;
	asrbc1.newSpecks.resize(nx * ny * nz);
	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				WorldCommands::NewSpeck speck;
				XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3Transform(pos, mat);
				XMStoreFloat3(&speck.position, pos);
				int index = i*ny*nz + j*nz + k;
				asrbc1.newSpecks[index] = speck;
			}
		}
	}

	// second body
	WorldCommands::AddSpecksCommand asrbc2;
	asrbc2.speckType = WorldCommands::SpeckType::RigidBody;
	asrbc2.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbc2.speckMass = mSpeckMass;
	nx = 2;
	ny = 4;
	nz = 2;
	width = (nx - 1) * 2.0f * mSpeckRadius;
	height = (ny - 1) * 2.0f * mSpeckRadius;
	depth = (nz - 1) * 2.0f * mSpeckRadius;
	x = -0.5f*width;
	y = -0.5f*height - (0.5f*height + 2.0f*mSpeckRadius);
	z = -0.5f*depth;
	dx = 2.0f * mSpeckRadius;
	dy = 2.0f * mSpeckRadius;
	dz = 2.0f * mSpeckRadius;
	asrbc2.newSpecks.resize(nx * ny * nz);
	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				WorldCommands::NewSpeck speck;
				XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3Transform(pos, mat);
				XMStoreFloat3(&speck.position, pos);
				int index = i*ny*nz + j*nz + k;
				asrbc2.newSpecks[index] = speck;
			}
		}
	}

	// Joint
	WorldCommands::AddSpecksCommand asrbcJoint;
	asrbcJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
	asrbcJoint.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbcJoint.speckMass = mSpeckMass;
	switch (type)
	{
	case SpeckPrimitivesGenerator::HingeJoint:
	{
		// add the joint specks
		nx = 1;
		ny = 1;
		nz = 2;
		width = (nx - 1) * 2.0f * mSpeckRadius;
		height = (ny - 1) * 2.0f * mSpeckRadius;
		depth = (nz - 1) * 2.0f * mSpeckRadius;
		x = -0.5f*width;
		y = -0.5f*height;
		z = -0.5f*depth;
		dx = 2.0f * mSpeckRadius;
		dy = 2.0f * mSpeckRadius;
		dz = 2.0f * mSpeckRadius;
		asrbcJoint.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					pos = XMVector3Transform(pos, mat);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbcJoint.newSpecks[index] = speck;
				}
			}
		}
	}
		break;
	case SpeckPrimitivesGenerator::AsymetricHingeJoint:
	{
		// add the rotation limiting specks in the middle
		WorldCommands::NewSpeck speck;
		XMVECTOR pos = XMVectorSet(-1.0f * mSpeckRadius, 0.0f, 1.0f * mSpeckRadius, 1.0f);
		pos = XMVector3Transform(pos, mat);
		XMStoreFloat3(&speck.position, pos);
		asrbc2.newSpecks.push_back(speck); // first one
		pos = XMVectorSet(-1.0f * mSpeckRadius, 0.0f, -1.0f * mSpeckRadius, 1.0f);
		pos = XMVector3Transform(pos, mat);
		XMStoreFloat3(&speck.position, pos);
		asrbc2.newSpecks.push_back(speck); // second one

		// add the joint specks
		nx = 1;
		ny = 1;
		nz = 2;
		width = (nx - 1) * 2.0f * mSpeckRadius;
		height = (ny - 1) * 2.0f * mSpeckRadius;
		depth = (nz - 1) * 2.0f * mSpeckRadius;
		x = -0.5f*width + 1.0f * mSpeckRadius;
		y = -0.5f*height;
		z = -0.5f*depth;
		dx = 2.0f * mSpeckRadius;
		dy = 2.0f * mSpeckRadius;
		dz = 2.0f * mSpeckRadius;
		asrbcJoint.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					pos = XMVector3Transform(pos, mat);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbcJoint.newSpecks[index] = speck;
				}
			}
		}
	}
	break;
	case SpeckPrimitivesGenerator::BallAndSocket:
	{
		// add the joint specks
		nx = 1;
		ny = 1;
		nz = 1;
		width = (nx - 1) * 2.0f * mSpeckRadius;
		height = (ny - 1) * 2.0f * mSpeckRadius;
		depth = (nz - 1) * 2.0f * mSpeckRadius;
		x = -0.5f*width;
		y = -0.5f*height;
		z = -0.5f*depth;
		dx = 2.0f * mSpeckRadius;
		dy = 2.0f * mSpeckRadius;
		dz = 2.0f * mSpeckRadius;
		asrbcJoint.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					pos = XMVector3Transform(pos, mat);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbcJoint.newSpecks[index] = speck;
				}
			}
		}
	}
		break;
	case SpeckPrimitivesGenerator::StiffJoint:
	{
		// add the joint specks
		nx = 2;
		ny = 1;
		nz = 2;
		width = (nx - 1) * 2.0f * mSpeckRadius;
		height = (ny - 1) * 2.0f * mSpeckRadius;
		depth = (nz - 1) * 2.0f * mSpeckRadius;
		x = -0.5f*width;
		y = -0.5f*height;
		z = -0.5f*depth;
		dx = 2.0f * mSpeckRadius;
		dy = 2.0f * mSpeckRadius;
		dz = 2.0f * mSpeckRadius;
		asrbcJoint.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					pos = XMVector3Transform(pos, mat);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbcJoint.newSpecks[index] = speck;
				}
			}
		}
	}
		break;
	default:
		break;
	}

	int specksAdded = 0;
	GetWorld().ExecuteCommand(asrbc1, &commandResult);
	int rb1 = commandResult.rigidBodyIndex;
	specksAdded += (int)asrbc1.newSpecks.size();

	GetWorld().ExecuteCommand(asrbc2, &commandResult);
	int rb2 = commandResult.rigidBodyIndex;
	specksAdded += (int)asrbc2.newSpecks.size();

	if (asrbcJoint.newSpecks.size() > 0)
	{
		asrbcJoint.rigidBodyJoint.rigidBodyIndex[0] = rb1;
		asrbcJoint.rigidBodyJoint.rigidBodyIndex[1] = rb2;
		GetWorld().ExecuteCommand(asrbcJoint);
		specksAdded += (int)asrbcJoint.newSpecks.size();
	}

	return specksAdded;
}
