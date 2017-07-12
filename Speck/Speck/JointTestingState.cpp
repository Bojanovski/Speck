
#include "JointTestingState.h"
#include <EngineCore.h>
#include <ProcessAndSystemData.h>
#include <AppCommands.h>
#include <WorldCommands.h>
#include <World.h>
#include <InputHandler.h>
#include <SetLookAtCameraController.h>

using namespace std;
using namespace DirectX;
using namespace Speck;

JointTestingState::JointTestingState(EngineCore &mEC)
	: AppState(),
	mCC(mEC)
{
}

JointTestingState::~JointTestingState()
{
}

void JointTestingState::Initialize()
{
	AppCommands::LimitFrameTimeCommand lftc;
	lftc.minFrameTime = 1.0f / 60.0f;
	GetApp().ExecuteCommand(lftc);

	WorldCommands::GetWorldPropertiesCommand gwp;
	WorldCommands::GetWorldPropertiesCommandResult gwpr;
	GetWorld().ExecuteCommand(gwp, &gwpr);
	float speckRadius = gwpr.speckRadius;

	WorldCommands::SetSpecksSolverParametersCommand sspc;
	sspc.substepsIterations = 3;
	GetWorld().ExecuteCommand(sspc);

	//
	// PBR testing
	//
	AppCommands::LoadResourceCommand lrc_albedo;
	lrc_albedo.name = "spaced-tiles1-albedo";
	lrc_albedo.path = L"Data/Textures/spaced-tiles1/spaced-tiles1-albedo.dds";
	lrc_albedo.resType = AppCommands::ResourceType::Texture;
	GetApp().ExecuteCommand(lrc_albedo);

	AppCommands::LoadResourceCommand lrc_normal;
	lrc_normal.name = "spaced-tiles1-normal";
	lrc_normal.path = L"Data/Textures/spaced-tiles1/spaced-tiles1-normal.dds";
	lrc_normal.resType = AppCommands::ResourceType::Texture;
	GetApp().ExecuteCommand(lrc_normal);

	AppCommands::LoadResourceCommand lrc_height;
	lrc_height.name = "spaced-tiles1-height";
	lrc_height.path = L"Data/Textures/spaced-tiles1/spaced-tiles1-height.dds";
	lrc_height.resType = AppCommands::ResourceType::Texture;
	GetApp().ExecuteCommand(lrc_height);

	AppCommands::LoadResourceCommand lrc_metalness;
	lrc_metalness.name = "spaced-tiles1-metalness";
	lrc_metalness.path = L"Data/Textures/spaced-tiles1/spaced-tiles1-metalness.dds";
	lrc_metalness.resType = AppCommands::ResourceType::Texture;
	GetApp().ExecuteCommand(lrc_metalness);

	AppCommands::LoadResourceCommand lrc_rough;
	lrc_rough.name = "spaced-tiles1-rough";
	lrc_rough.path = L"Data/Textures/spaced-tiles1/spaced-tiles1-rough.dds";
	lrc_rough.resType = AppCommands::ResourceType::Texture;
	GetApp().ExecuteCommand(lrc_rough);

	AppCommands::LoadResourceCommand lrc_ao;
	lrc_ao.name = "spaced-tiles1-ao";
	lrc_ao.path = L"Data/Textures/spaced-tiles1/spaced-tiles1-ao.dds";
	lrc_ao.resType = AppCommands::ResourceType::Texture;
	GetApp().ExecuteCommand(lrc_ao);

	AppCommands::CreateMaterialCommand cmc_pbr;
	cmc_pbr.materialName = "pbrMatTest";
	//cmc_pbr.albedoTexName = lrc_albedo.name;
	//cmc_pbr.normalTexName = lrc_normal.name;
	//cmc_pbr.heightTexName = lrc_height.name;
	//cmc_pbr.metalnessTexName = lrc_metalness.name;
	//cmc_pbr.roughnessTexName = lrc_rough.name;
	//cmc_pbr.aoTexName = lrc_ao.name;
	cmc_pbr.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	cmc_pbr.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	cmc_pbr.Roughness = 1.0f;
	GetApp().ExecuteCommand(cmc_pbr);

	//////////////////////////////////

	// Add an object
	WorldCommands::AddRenderItemCommand cmd3;
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(20.0f, 20.0f, 0.4f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "box";
	cmd3.type = WorldCommands::RenderItemType::Static;
	cmd3.staticRenderItem.worldTransform.mS = XMFLOAT3(70.0f, 5.0f, 70.0f);
	cmd3.staticRenderItem.worldTransform.mT = XMFLOAT3(0.0f, -1.0f, 0.0f);
	XMStoreFloat4(&cmd3.staticRenderItem.worldTransform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetWorld().ExecuteCommand(cmd3);
	// collision
	WorldCommands::AddStaticColliderCommand scc;
	scc.transform.mR = cmd3.staticRenderItem.worldTransform.mR;
	scc.transform.mT = cmd3.staticRenderItem.worldTransform.mT;
	scc.transform.mS = cmd3.staticRenderItem.worldTransform.mS;
	GetWorld().ExecuteCommand(scc);

	// Add an object
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(5.0f, 5.0f, 0.5f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "box";
	cmd3.staticRenderItem.worldTransform.mS = XMFLOAT3(5.0f, 20.0f, 60.0f);
	cmd3.staticRenderItem.worldTransform.mT = XMFLOAT3(20.0f, 0.0f, 0.0f);
	XMStoreFloat4(&cmd3.staticRenderItem.worldTransform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetWorld().ExecuteCommand(cmd3);
	// collision
	scc.transform.mR = cmd3.staticRenderItem.worldTransform.mR;
	scc.transform.mT = cmd3.staticRenderItem.worldTransform.mT;
	scc.transform.mS = cmd3.staticRenderItem.worldTransform.mS;
	GetWorld().ExecuteCommand(scc);

	// Add an object
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(5.0f, 5.0f, 0.5f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "box";
	cmd3.staticRenderItem.worldTransform.mS = XMFLOAT3(60.0f, 20.0f, 5.0f);
	cmd3.staticRenderItem.worldTransform.mT = XMFLOAT3(0.0f, -0.0f, 20.0f);
	XMStoreFloat4(&cmd3.staticRenderItem.worldTransform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetWorld().ExecuteCommand(cmd3);
	// collision
	scc.transform.mR = cmd3.staticRenderItem.worldTransform.mR;
	scc.transform.mT = cmd3.staticRenderItem.worldTransform.mT;
	scc.transform.mS = cmd3.staticRenderItem.worldTransform.mS;
	GetWorld().ExecuteCommand(scc);

	// Add an object
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(5.0f, 5.0f, 0.5f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "box";
	cmd3.staticRenderItem.worldTransform.mS = XMFLOAT3(5.0f, 20.0f, 60.0f);
	cmd3.staticRenderItem.worldTransform.mT = XMFLOAT3(-20.0f, 0.0f, 0.0f);
	XMStoreFloat4(&cmd3.staticRenderItem.worldTransform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetWorld().ExecuteCommand(cmd3);
	// collision
	scc.transform.mR = cmd3.staticRenderItem.worldTransform.mR;
	scc.transform.mT = cmd3.staticRenderItem.worldTransform.mT;
	scc.transform.mS = cmd3.staticRenderItem.worldTransform.mS;
	GetWorld().ExecuteCommand(scc);

	// Add an object
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(5.0f, 5.0f, 0.5f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "box";
	cmd3.staticRenderItem.worldTransform.mS = XMFLOAT3(60.0f, 100.0f, 50.0f);
	cmd3.staticRenderItem.worldTransform.mT = XMFLOAT3(0.0f, 0.0f, -40.0f);
	XMStoreFloat4(&cmd3.staticRenderItem.worldTransform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetWorld().ExecuteCommand(cmd3);
	// collision
	scc.transform.mR = cmd3.staticRenderItem.worldTransform.mR;
	scc.transform.mT = cmd3.staticRenderItem.worldTransform.mT;
	scc.transform.mS = cmd3.staticRenderItem.worldTransform.mS;
	GetWorld().ExecuteCommand(scc);

	for (int i = 5; i < 185; i += 2)
	{

		//
		// first pair
		//

		// first body
		WorldCommands::AddSpecksCommand asrbc;
		WorldCommands::AddSpecksCommandResult commandResult;
		asrbc.speckType = WorldCommands::SpeckType::RigidBody;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.speckMass = 1.0f;
		int nx = 2;
		int ny = 7;
		int nz = 2;
		float width = (nx - 1) * 2.0f * speckRadius;
		float height = (ny - 1) * 2.0f * speckRadius;
		float depth = (nz - 1) * 2.0f * speckRadius;
		float x = -0.5f*width;
		float y = -0.5f*height + 9.0f + i*5.0f;
		float z = -0.5f*depth;
		float dx = width / (nx - 1);
		float dy = height / (ny - 1);
		float dz = depth / (nz - 1);
		asrbc.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					// Position instanced along a 3D grid.
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc.newSpecks[index] = speck;
				}
			}
		}
		GetWorld().ExecuteCommand(asrbc, &commandResult);
		int rb1 = commandResult.rigidBodyIndex;

		// second body
		asrbc.speckType = WorldCommands::SpeckType::RigidBody;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.speckMass = 1.0f;
		nx = 2;
		ny = 7;
		nz = 2;
		width = (nx - 1) * 2.0f * speckRadius;
		height = (ny - 1) * 2.0f * speckRadius;
		depth = (nz - 1) * 2.0f * speckRadius;
		x = -0.5f*width;
		y = -0.5f*height + 5.0f + i*5.0f;
		z = -0.5f*depth;
		dx = width / (nx - 1);
		dy = height / (ny - 1);
		dz = depth / (nz - 1);
		asrbc.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					// Position instanced along a 3D grid.
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc.newSpecks[index] = speck;
				}
			}
		}

		WorldCommands::NewSpeck speck;
		XMVECTOR pos = XMVectorSet(-1.0f * speckRadius, 7.0f + i*5.0f, 1.0f * speckRadius, 1.0f);
		XMStoreFloat3(&speck.position, pos);
		asrbc.newSpecks.push_back(speck);
		pos = XMVectorSet(-1.0f * speckRadius, 7.0f + i*5.0f, -1.0f * speckRadius, 1.0f);
		XMStoreFloat3(&speck.position, pos);
		asrbc.newSpecks.push_back(speck);

		GetWorld().ExecuteCommand(asrbc, &commandResult);
		int rb2 = commandResult.rigidBodyIndex;

		// add joint
		asrbc.speckType = WorldCommands::SpeckType::RigidBodyJoint;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.speckMass = 1.0f;
		asrbc.newSpecks.resize(64);
		nx = 1;
		ny = 1;
		nz = 2;
		width = (nx - 1) * 2.0f * speckRadius;
		height = (ny - 1) * 2.0f * speckRadius;
		depth = (nz - 1) * 2.0f * speckRadius;
		x = -0.5f*width + 1.0f * speckRadius;
		y = -0.5f*height + 7.0f + i*5.0f;
		z = -0.5f*depth;
		dx = 0.0f;
		dy = 0.0f;
		dz = depth / (nz - 1);
		asrbc.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					// Position instanced along a 3D grid.
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc.newSpecks[index] = speck;
				}
			}
		}
		asrbc.rigidBodyJoint.rigidBodyIndex[0] = rb1;
		asrbc.rigidBodyJoint.rigidBodyIndex[1] = rb2;
		GetWorld().ExecuteCommand(asrbc);

		//
		// second pair
		//

		// first body
		asrbc.speckType = WorldCommands::SpeckType::RigidBody;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.speckMass = 1.0f;
		nx = 2;
		ny = 7;
		nz = 2;
		width = (nx - 1) * 2.0f * speckRadius;
		height = (ny - 1) * 2.0f * speckRadius;
		depth = (nz - 1) * 2.0f * speckRadius;
		x = -0.5f*width - 10.0f;
		y = -0.5f*height + 9.0f + i*5.0f;
		z = -0.5f*depth;
		dx = width / (nx - 1);
		dy = height / (ny - 1);
		dz = depth / (nz - 1);
		asrbc.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					// Position instanced along a 3D grid.
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc.newSpecks[index] = speck;
				}
			}
		}
		GetWorld().ExecuteCommand(asrbc, &commandResult);
		rb1 = commandResult.rigidBodyIndex;

		// second body
		asrbc.speckType = WorldCommands::SpeckType::RigidBody;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.speckMass = 1.0f;
		nx = 2;
		ny = 7;
		nz = 2;
		width = (nx - 1) * 2.0f * speckRadius;
		height = (ny - 1) * 2.0f * speckRadius;
		depth = (nz - 1) * 2.0f * speckRadius;
		x = -0.5f*width - 10.0f;
		y = -0.5f*height + 5.0f + i*5.0f;
		z = -0.5f*depth;
		dx = width / (nx - 1);
		dy = height / (ny - 1);
		dz = depth / (nz - 1);
		asrbc.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					// Position instanced along a 3D grid.
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc.newSpecks[index] = speck;
				}
			}
		}
		GetWorld().ExecuteCommand(asrbc, &commandResult);
		rb2 = commandResult.rigidBodyIndex;

		// add joint
		asrbc.speckType = WorldCommands::SpeckType::RigidBodyJoint;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.speckMass = 1.0f;
		asrbc.newSpecks.resize(64);
		nx = 1;
		ny = 1;
		nz = 1;
		width = (nx - 1) * 2.0f * speckRadius;
		height = (ny - 1) * 2.0f * speckRadius;
		depth = (nz - 1) * 2.0f * speckRadius;
		x = -0.5f*width - 10.0f;
		y = -0.5f*height + 7.0f + i*5.0f;
		z = -0.5f*depth;
		dx = 0.0f;
		dy = 0.0f;
		dz = 0.0f;
		asrbc.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					// Position instanced along a 3D grid.
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc.newSpecks[index] = speck;
				}
			}
		}
		asrbc.rigidBodyJoint.rigidBodyIndex[0] = rb1;
		asrbc.rigidBodyJoint.rigidBodyIndex[1] = rb2;
		GetWorld().ExecuteCommand(asrbc);

		//
		// third pair
		//

		// first body
		asrbc.speckType = WorldCommands::SpeckType::RigidBody;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.speckMass = 1.0f;
		nx = 2;
		ny = 7;
		nz = 2;
		width = (nx - 1) * 2.0f * speckRadius;
		height = (ny - 1) * 2.0f * speckRadius;
		depth = (nz - 1) * 2.0f * speckRadius;
		x = -0.5f*width + 10.0f;
		y = -0.5f*height + 9.0f + i*5.0f;
		z = -0.5f*depth;
		dx = width / (nx - 1);
		dy = height / (ny - 1);
		dz = depth / (nz - 1);
		asrbc.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					// Position instanced along a 3D grid.
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc.newSpecks[index] = speck;
				}
			}
		}
		GetWorld().ExecuteCommand(asrbc, &commandResult);
		rb1 = commandResult.rigidBodyIndex;

		// second body
		asrbc.speckType = WorldCommands::SpeckType::RigidBody;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.speckMass = 1.0f;
		nx = 2;
		ny = 7;
		nz = 2;
		width = (nx - 1) * 2.0f * speckRadius;
		height = (ny - 1) * 2.0f * speckRadius;
		depth = (nz - 1) * 2.0f * speckRadius;
		x = -0.5f*width + 10.0f;
		y = -0.5f*height + 5.0f + i*5.0f;
		z = -0.5f*depth;
		dx = width / (nx - 1);
		dy = height / (ny - 1);
		dz = depth / (nz - 1);
		asrbc.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					// Position instanced along a 3D grid.
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc.newSpecks[index] = speck;
				}
			}
		}
		GetWorld().ExecuteCommand(asrbc, &commandResult);
		rb2 = commandResult.rigidBodyIndex;

		// add joint
		asrbc.speckType = WorldCommands::SpeckType::RigidBodyJoint;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.speckMass = 1.0f;
		asrbc.newSpecks.resize(64);
		nx = 2;
		ny = 1;
		nz = 2;
		width = (nx - 1) * 2.0f * speckRadius;
		height = (ny - 1) * 2.0f * speckRadius;
		depth = (nz - 1) * 2.0f * speckRadius;
		x = -0.5f*width + 10.0f;
		y = -0.5f*height + 7.0f + i*5.0f;
		z = -0.5f*depth;
		dx = width / (nx - 1);
		dy = 0.0f;
		dz = depth / (nz - 1);
		asrbc.newSpecks.resize(nx * ny * nz);

		for (int i = 0; i < nx; ++i)
		{
			for (int j = 0; j < ny; ++j)
			{
				for (int k = 0; k < nz; ++k)
				{
					WorldCommands::NewSpeck speck;
					// Position instanced along a 3D grid.
					XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
					XMStoreFloat3(&speck.position, pos);
					int index = i*ny*nz + j*nz + k;
					asrbc.newSpecks[index] = speck;
				}
			}
		}
		asrbc.rigidBodyJoint.rigidBodyIndex[0] = rb1;
		asrbc.rigidBodyJoint.rigidBodyIndex[1] = rb2;
		GetWorld().ExecuteCommand(asrbc);
	}

	// add gravity
	WorldCommands::AddExternalForceCommand efc;
	efc.vector = XMFLOAT3(0.0f, -10.0f, 0.0f);
	efc.type = ExternalForces::Types::Acceleration;
	GetWorld().ExecuteCommand(efc);

	// modify time
	WorldCommands::SetTimeMultiplierCommand stmc;
	stmc.timeMultiplierConstant = 1.0f;
	GetWorld().ExecuteCommand(stmc);

	// position camera
	SetLookAtCameraController slacc(XMFLOAT3(2.0f, 28.0f, -30.0f), XMFLOAT3(2.0f, 10.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
	AppCommands::UpdateCameraCommand ucc;
	ucc.ccPt = &slacc;
	GetApp().ExecuteCommand(ucc);
}

void JointTestingState::Update(float dt)
{
	mCC.Update(dt);
	AppCommands::UpdateCameraCommand ucc;
	ucc.ccPt = &mCC;
	ucc.deltaTime = dt;
	GetApp().ExecuteCommand(ucc);

	AppCommands::SetWindowTitleCommand cmd;
	wstring fpsStr = to_wstring(GetEngineCore().GetProcessAndSystemData()->mFPS);
	wstring mspfStr = to_wstring(GetEngineCore().GetProcessAndSystemData()->m_mSPF);
	cmd.text = L"    fps: " + fpsStr + L"   spf: " + mspfStr;
	GetApp().ExecuteCommand(cmd);

	static float t = 0.0f;
	t += dt;

	auto ih = GetApp().GetEngineCore().GetInputHandler();
	float fPressedTime;
	ih->IsPressed(0x46, &fPressedTime); // F key

	static float anglularVelocity = 1.0f;
	static float angle = 0.0f;
	float pressed_1_Time;
	ih->IsPressed(0x31, &pressed_1_Time); // 1 key
	if (pressed_1_Time > 0.0f) anglularVelocity += dt * 2.0f;

	float pressed_2_Time;
	ih->IsPressed(0x32, &pressed_2_Time); // 2 key
	if (pressed_2_Time > 0.0f) anglularVelocity -= dt * 2.0f;

	angle += anglularVelocity * dt;

	//if (fPressedTime == 0.0f)
	//{
	//	WorldCommands::UpdateSpeckRigidBodyCommand command;
	//	command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;
	//	command.rigidBodyIndex = 0;
	//	XMStoreFloat3(&command.transform.mT, XMVectorSet(0.0f, 9.0f, 0.0f, 1.0f));
	//	XMStoreFloat4(&command.transform.mR, XMQuaternionRotationRollPitchYaw(angle, 0.0f, XM_PIDIV2));
	//	GetWorld().ExecuteCommand(command);

	//	command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;
	//	command.rigidBodyIndex = 2;
	//	XMStoreFloat3(&command.transform.mT, XMVectorSet(-10.0f, 9.0f, 0.0f, 1.0f));
	//	XMStoreFloat4(&command.transform.mR, XMQuaternionRotationRollPitchYaw(angle, 0.0f, XM_PIDIV2));
	//	GetWorld().ExecuteCommand(command);

	//	command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;
	//	command.rigidBodyIndex = 4;
	//	XMStoreFloat3(&command.transform.mT, XMVectorSet(10.0f, 9.0f, 0.0f, 1.0f));
	//	XMStoreFloat4(&command.transform.mR, XMQuaternionRotationRollPitchYaw(angle, 0.0f, XM_PIDIV2));
	//	GetWorld().ExecuteCommand(command);
	//}
	//else
	//{
	//	WorldCommands::UpdateSpeckRigidBodyCommand command;
	//	command.movementMode = WorldCommands::RigidBodyMovementMode::GPU;
	//	command.rigidBodyIndex = 0;
	//	GetWorld().ExecuteCommand(command);

	//	command.movementMode = WorldCommands::RigidBodyMovementMode::GPU;
	//	command.rigidBodyIndex = 2;
	//	GetWorld().ExecuteCommand(command);

	//	command.movementMode = WorldCommands::RigidBodyMovementMode::GPU;
	//	command.rigidBodyIndex = 4;
	//	GetWorld().ExecuteCommand(command);
	//}
}

void JointTestingState::Draw(float dt)
{
}

void JointTestingState::OnResize()
{
}

void JointTestingState::BuildOffScreenViews()
{
}
