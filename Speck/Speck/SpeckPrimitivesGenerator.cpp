
#include "SpeckPrimitivesGenerator.h"
#include <World.h>

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

int SpeckPrimitivesGenerator::GenerateBox(int nx, int ny, int nz, const char *materialName, bool useSkin, const XMFLOAT3 & position, const XMFLOAT3 & rotation)
{
	XMMATRIX mat = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z) * XMMatrixTranslation(position.x, position.y, position.z);
	XMFLOAT4X4 transform;
	XMStoreFloat4x4(&transform, mat);
	return GenerateBox(nx, ny, nz, materialName, useSkin, transform);
}

int SpeckPrimitivesGenerator::GenerateBox(int nx, int ny, int nz, const char * materialName, bool useSkin, const XMFLOAT4X4 &transform)
{
	XMMATRIX mat = XMLoadFloat4x4(&transform);
	float width, height, depth;
	WorldCommands::AddSpecksCommandResult commandResult;

	// add the specks
	WorldCommands::AddSpecksCommand asrbc;
	asrbc.speckType = WorldCommands::SpeckType::RigidBody;
	asrbc.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbc.speckMass = mSpeckMass;
	GenerateCubicStructure(nx, ny, nz, &width, &height, &depth, &asrbc.newSpecks, mat);
	GetWorld().ExecuteCommand(asrbc, &commandResult);
	int specksAdded = (int)asrbc.newSpecks.size();
	
	// add the mesh skin
	if (useSkin) 
	{
		XMVECTOR s, r, t;
		XMMatrixDecompose(&s, &r, &t, mat);
		WorldCommands::AddRenderItemCommand aric;
		aric.geometryName = "shapeGeo";
		aric.materialName = materialName;
		aric.meshName = "box";
		aric.type = WorldCommands::RenderItemType::SpeckRigidBody;
		aric.speckRigidBodyRenderItem.rigidBodyIndex = commandResult.rigidBodyIndex;
		aric.speckRigidBodyRenderItem.localTransform.MakeIdentity();
		aric.staticRenderItem.worldTransform.mS = XMFLOAT3(width + 2.0f * mSpeckRadius, height + 2.0f * mSpeckRadius, depth + 2.0f * mSpeckRadius);
		XMStoreFloat4(&aric.staticRenderItem.worldTransform.mR, r);
		GetWorld().ExecuteCommand(aric);
	}

	return specksAdded;
}

int SpeckPrimitivesGenerator::GeneratreConstrainedRigidBodyPair(ConstrainedRigidBodyPairType type, const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &rotation)
{
	XMMATRIX mat = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z) * XMMatrixTranslation(position.x, position.y, position.z);
	int nx, ny, nz;
	float dx, dy, dz, width, height, depth, x, y, z;

	// first body
	WorldCommands::AddSpecksCommand asrbc1;
	WorldCommands::AddSpecksCommandResult asrbc1cr;
	asrbc1.speckType = WorldCommands::SpeckType::RigidBody;
	asrbc1.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbc1.speckMass = mSpeckMass;

	// second body
	WorldCommands::AddSpecksCommand asrbc2;
	WorldCommands::AddSpecksCommandResult asrbc2cr;
	asrbc2.speckType = WorldCommands::SpeckType::RigidBody;
	asrbc2.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbc2.speckMass = mSpeckMass;

	// third body (optional)
	WorldCommands::AddSpecksCommand asrbc3;
	WorldCommands::AddSpecksCommandResult asrbc3cr;
	asrbc3.speckType = WorldCommands::SpeckType::RigidBody;
	asrbc3.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbc3.speckMass = mSpeckMass;

	// joint
	WorldCommands::AddSpecksCommand asrbcJoint1;
	WorldCommands::AddSpecksCommandResult asrbcJoint1cr;
	asrbcJoint1.speckType = WorldCommands::SpeckType::RigidBodyJoint;
	asrbcJoint1.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbcJoint1.speckMass = mSpeckMass;

	// second joint (optional)
	WorldCommands::AddSpecksCommand asrbcJoint2;
	WorldCommands::AddSpecksCommandResult asrbcJoint2cr;
	asrbcJoint2.speckType = WorldCommands::SpeckType::RigidBodyJoint;
	asrbcJoint2.frictionCoefficient = mSpeckFrictionCoefficient;
	asrbcJoint2.speckMass = mSpeckMass;

	// rigid body index pointers
	UINT *rbIndexPt[] = { nullptr, nullptr, nullptr, nullptr };

	switch (type)
	{
	case SpeckPrimitivesGenerator::HingeJoint:
	{
		// first rigid body
		nx = 2;
		ny = 4;
		nz = 2;
		width = (nx - 1) * 2.0f * mSpeckRadius;
		height = (ny - 1) * 2.0f * mSpeckRadius;
		depth = (nz - 1) * 2.0f * mSpeckRadius;
		x = -0.5f*width;
		y = -0.5f*height + (0.5f*height + sqrt(3.0f)*mSpeckRadius);
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

		// second rigid body
		nx = 2;
		ny = 4;
		nz = 2;
		width = (nx - 1) * 2.0f * mSpeckRadius;
		height = (ny - 1) * 2.0f * mSpeckRadius;
		depth = (nz - 1) * 2.0f * mSpeckRadius;
		x = -0.5f*width;
		y = -0.5f*height - (0.5f*height + sqrt(3.0f)*mSpeckRadius);
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
		asrbcJoint1.newSpecks.resize(nx * ny * nz);

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
					asrbcJoint1.newSpecks[index] = speck;
				}
			}
		}

		rbIndexPt[0] = &asrbc1cr.rigidBodyIndex;
		rbIndexPt[1] = &asrbc2cr.rigidBodyIndex;
	}
	break;

	case SpeckPrimitivesGenerator::AsymetricHingeJoint:
	{
		// first rigid body
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

		// second rigid body
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
		asrbcJoint1.newSpecks.resize(nx * ny * nz);

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
					asrbcJoint1.newSpecks[index] = speck;
				}
			}
		}

		rbIndexPt[0] = &asrbc1cr.rigidBodyIndex;
		rbIndexPt[1] = &asrbc2cr.rigidBodyIndex;
	}
	break;

	case SpeckPrimitivesGenerator::BallAndSocket:
	{
		// first rigid body
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

		// second rigid body
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
		asrbcJoint1.newSpecks.resize(nx * ny * nz);

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
					asrbcJoint1.newSpecks[index] = speck;
				}
			}
		}

		rbIndexPt[0] = &asrbc1cr.rigidBodyIndex;
		rbIndexPt[1] = &asrbc2cr.rigidBodyIndex;
	}
	break;

	case SpeckPrimitivesGenerator::StiffJoint:
	{
		// first rigid body
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

		// second rigid body
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
		asrbcJoint1.newSpecks.resize(nx * ny * nz);

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
					asrbcJoint1.newSpecks[index] = speck;
				}
			}
		}

		rbIndexPt[0] = &asrbc1cr.rigidBodyIndex;
		rbIndexPt[1] = &asrbc2cr.rigidBodyIndex;
	}
	break;

	case SpeckPrimitivesGenerator::UniversalJoint:
	{
		// first rigid body
		nx = 2;
		ny = 4;
		nz = 2;
		width = (nx - 1) * 2.0f * mSpeckRadius;
		height = (ny - 1) * 2.0f * mSpeckRadius;
		depth = (nz - 1) * 2.0f * mSpeckRadius;
		x = -0.5f*width;
		y = -0.5f*height + (0.5f*height + 2.0f*mSpeckRadius - mSpeckRadius);
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
					if (j == 0 ) continue;
					WorldCommands::NewSpeck speck;
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					pos = XMVector3Transform(pos, mat);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc1.newSpecks.push_back(speck);
				}
			}
		}

		// second rigid body
		nx = 2;
		ny = 4;
		nz = 2;
		width = (nx - 1) * 2.0f * mSpeckRadius;
		height = (ny - 1) * 2.0f * mSpeckRadius;
		depth = (nz - 1) * 2.0f * mSpeckRadius;
		x = -0.5f*width;
		y = -0.5f*height - (0.5f*height + 2.0f*mSpeckRadius - mSpeckRadius);
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
					if (j == (ny - 1) ) continue;
					WorldCommands::NewSpeck speck;
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					pos = XMVector3Transform(pos, mat);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc2.newSpecks.push_back(speck);
				}
			}
		}

		// add the first joint specks
		nx = 2;
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
		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					//if (i == 1) continue;
					WorldCommands::NewSpeck speck;
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					pos.m128_f32[0] *= (1.0f + sqrt(3.0f)*mSpeckRadius);
					pos = XMVector3Transform(pos, mat);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbcJoint1.newSpecks.push_back(speck);
				}
			}
		}

		// add the speck rigid body in the middle
		nx = 1;
		ny = 2;
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
		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					//if (j == 1) continue;
					WorldCommands::NewSpeck speck;
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					pos.m128_f32[1] *= (1.0f + sqrt(3.0f) * mSpeckRadius);
					pos = XMVector3Transform(pos, mat);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc3.newSpecks.push_back(speck);
				}
			}
		}

		// add the second joint specks
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
		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					//if (k == 1) continue;
					WorldCommands::NewSpeck speck;
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					pos.m128_f32[2] *= (1.0f + sqrt(3.0f)*mSpeckRadius);
					pos = XMVector3Transform(pos, mat);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbcJoint2.newSpecks.push_back(speck);
				}
			}
		}

		rbIndexPt[0] = &asrbc1cr.rigidBodyIndex;
		rbIndexPt[1] = &asrbc3cr.rigidBodyIndex;
		rbIndexPt[2] = &asrbc3cr.rigidBodyIndex;
		rbIndexPt[3] = &asrbc2cr.rigidBodyIndex;
	}

	default:
		break;
	}

	int specksAdded = 0;
	
	if (asrbc1.newSpecks.size() > 0)
	{
		GetWorld().ExecuteCommand(asrbc1, &asrbc1cr);
		specksAdded += (int)asrbc1.newSpecks.size();
	}

	if (asrbc2.newSpecks.size() > 0)
	{
		GetWorld().ExecuteCommand(asrbc2, &asrbc2cr);
		specksAdded += (int)asrbc2.newSpecks.size();
	}

	if (asrbc3.newSpecks.size() > 0)
	{
		GetWorld().ExecuteCommand(asrbc3, &asrbc3cr);
		specksAdded += (int)asrbc3.newSpecks.size();
	}

	if (asrbcJoint1.newSpecks.size() > 0)
	{
		asrbcJoint1.rigidBodyJoint.rigidBodyIndex[0] = *rbIndexPt[0];
		asrbcJoint1.rigidBodyJoint.rigidBodyIndex[1] = *rbIndexPt[1];
		asrbcJoint1.rigidBodyJoint.calculateSDFGradient = (asrbcJoint1.newSpecks.size() > 2);
		GetWorld().ExecuteCommand(asrbcJoint1, &asrbcJoint1cr);
		specksAdded += (int)asrbcJoint1.newSpecks.size();
	}

	if (asrbcJoint2.newSpecks.size() > 0)
	{
		asrbcJoint2.rigidBodyJoint.rigidBodyIndex[0] = *rbIndexPt[2];
		asrbcJoint2.rigidBodyJoint.rigidBodyIndex[1] = *rbIndexPt[3];
		asrbcJoint2.rigidBodyJoint.calculateSDFGradient = (asrbcJoint2.newSpecks.size() > 2);
		GetWorld().ExecuteCommand(asrbcJoint2, &asrbcJoint2cr);
		specksAdded += (int)asrbcJoint2.newSpecks.size();
	}

	return specksAdded;
}

void SpeckPrimitivesGenerator::GenerateCubicStructure(int nx, int ny, int nz, vector<Speck::WorldCommands::NewSpeck>* outStructure, CXMMATRIX transformation)
{
	float width, height, depth;
	GenerateCubicStructure(nx, ny, nz, &width, &height, &depth, outStructure, transformation);
}

void SpeckPrimitivesGenerator::GenerateCubicStructure(int nx, int ny, int nz, float *outWidth, float *outHeight, float *outDepth, vector<WorldCommands::NewSpeck>* outStructure, CXMMATRIX transformation)
{
	outStructure->clear();
	*outWidth = (nx - 1) * 2.0f * mSpeckRadius;
	*outHeight = (ny - 1) * 2.0f * mSpeckRadius;
	*outDepth = (nz - 1) * 2.0f * mSpeckRadius;
	float x = -0.5f*(*outWidth);
	float y = -0.5f*(*outHeight);
	float z = -0.5f*(*outDepth);
	float dx = 2.0f * mSpeckRadius;
	float dy = 2.0f * mSpeckRadius;
	float dz = 2.0f * mSpeckRadius;
	outStructure->resize(nx * ny * nz);

	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				WorldCommands::NewSpeck speck;
				XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3Transform(pos, transformation);
				XMStoreFloat3(&speck.position, pos);
				int index = i*ny*nz + j*nz + k;
				(*outStructure)[index] = speck;
			}
		}
	}
}
