
#include "MainState.h"
#include <EngineCore.h>
#include <ProcessAndSystemData.h>
#include <AppCommands.h>
#include <WorldCommands.h>

using namespace std;
using namespace DirectX;
using namespace Speck;

MainState::MainState(EngineCore &mEC)
	: AppState(),
	mCC(mEC)
{
}

MainState::~MainState()
{
}

void MainState::Initialize()
{
	GetEngineCore().SetCameraController(&mCC);

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
	cmc_pbr.albedoTexName = lrc_albedo.name;
	cmc_pbr.normalTexName = lrc_normal.name;
	cmc_pbr.heightTexName = lrc_height.name;
	cmc_pbr.metalnessTexName = lrc_metalness.name;
	cmc_pbr.roughnessTexName = lrc_rough.name;
	cmc_pbr.aoTexName = lrc_ao.name;
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
	cmd3.PSOGroupName = "single";
	cmd3.transform.mS = XMFLOAT3(30.0f, 5.0f, 30.0f);
	cmd3.transform.mT = XMFLOAT3(0.0f, -2.0f, 0.0f);
	XMStoreFloat4(&cmd3.transform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetApp().ExecuteCommand(cmd3);
	// collision
	WorldCommands::AddStaticColliderCommand scc;
	scc.transform.mR = cmd3.transform.mR;
	scc.transform.mT = cmd3.transform.mT;
	scc.transform.mS = cmd3.transform.mS;
	GetApp().ExecuteCommand(scc);

	// Add an object
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(5.0f, 5.0f, 0.5f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "box";
	cmd3.PSOGroupName = "single";
	cmd3.transform.mS = XMFLOAT3(5.0f, 20.0f, 20.0f);
	cmd3.transform.mT = XMFLOAT3(10.0f, 0.0f, 0.0f);
	XMStoreFloat4(&cmd3.transform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetApp().ExecuteCommand(cmd3);
	// collision
	scc.transform.mR = cmd3.transform.mR;
	scc.transform.mT = cmd3.transform.mT;
	scc.transform.mS = cmd3.transform.mS;
	GetApp().ExecuteCommand(scc);

	// Add an object
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(5.0f, 5.0f, 0.5f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "box";
	cmd3.PSOGroupName = "single";
	cmd3.transform.mS = XMFLOAT3(20.0f, 20.0f, 5.0f);
	cmd3.transform.mT = XMFLOAT3(0.0f, 0.0f, 7.0f);
	XMStoreFloat4(&cmd3.transform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetApp().ExecuteCommand(cmd3);
	// collision
	scc.transform.mR = cmd3.transform.mR;
	scc.transform.mT = cmd3.transform.mT;
	scc.transform.mS = cmd3.transform.mS;
	GetApp().ExecuteCommand(scc);

	// Add an object
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(5.0f, 5.0f, 0.5f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "box";
	cmd3.PSOGroupName = "single";
	cmd3.transform.mS = XMFLOAT3(5.0f, 10.0f, 20.0f);
	cmd3.transform.mT = XMFLOAT3(-9.0f, -2.0f, 0.0f);
	XMStoreFloat4(&cmd3.transform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetApp().ExecuteCommand(cmd3);
	// collision
	scc.transform.mR = cmd3.transform.mR;
	scc.transform.mT = cmd3.transform.mT;
	scc.transform.mS = cmd3.transform.mS;
	GetApp().ExecuteCommand(scc);

	// Add an object
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(5.0f, 5.0f, 0.5f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "box";
	cmd3.PSOGroupName = "single";
	cmd3.transform.mS = XMFLOAT3(20.0f, 10.0f, 5.0f);
	cmd3.transform.mT = XMFLOAT3(0.0f, -2.0f, -8.0f);
	XMStoreFloat4(&cmd3.transform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetApp().ExecuteCommand(cmd3);
	// collision
	scc.transform.mR = cmd3.transform.mR;
	scc.transform.mT = cmd3.transform.mT;
	scc.transform.mS = cmd3.transform.mS;
	GetApp().ExecuteCommand(scc);


	WorldCommands::AddSpecksCommand asrbc;
	asrbc.speckType = WorldCommands::SpeckType::Fluid;
	asrbc.frictionCoefficient = 0.001f;
	asrbc.cohesionCoefficient = 0.03f;
	asrbc.viscosityCoefficient = 0.03f;
	asrbc.speckMass = 1.0f;
	asrbc.newSpecks.resize(32000);
	int n = (int)pow(asrbc.newSpecks.size(), 1.0f / 3.0f);
	int nPow3 = n*n*n;
	float width		= 6.4f;
	float height	= 6.4f;
	float depth		= 6.4f;
	float x = -0.5f*width - 3.0f;
	float y = -0.5f*height + 5.0f;
	float z = -0.5f*depth - 1.0f;
	float dx = width / (n - 1);
	float dy = height / (n - 1);
	float dz = depth / (n - 1);
	for (int k = 0; k < n; ++k)
	{
		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < n; ++j)
			{
				int index = k*n*n + i*n + j;
				// Position instanced along a 3D grid.
				asrbc.newSpecks[index].position = XMFLOAT3(x + j*dx + 0.1f*sin(i*0.1f), y + i*dy, z + k*dz + 0.1f*cos(i*0.1f));
				asrbc.newSpecks[index].mass = 1.0f;
			}
		}
	}
	//GetApp().ExecuteCommand(asrbc);



	asrbc.speckType = WorldCommands::SpeckType::RigidBody;
	asrbc.frictionCoefficient = 0.6f;
	asrbc.newSpecks.resize(125);
	n = (int)pow(asrbc.newSpecks.size(), 1.0f / 3.0f);
	nPow3 = n*n*n;
	width = 0.8f;
	height = 0.8f;
	depth = 0.8f;
	x = -0.5f*width + 0.4f;
	y = -0.5f*height + 1.0f;
	z = -0.5f*depth + 0.0f;
	dx = width / (n - 1);
	dy = height / (n - 1);
	dz = depth / (n - 1);
	for (int k = 0; k < n; ++k)
	{
		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < n; ++j)
			{
				int index = k*n*n + i*n + j;
				// Position instanced along a 3D grid.
				asrbc.newSpecks[index].position = XMFLOAT3(x + j*dx /*+ sin(i*0.1f)*/, y + i*dy, z + k*dz /*+ cos(i*0.1f)*/);
				asrbc.newSpecks[index].mass = 1.0f;
			}
		}
	}
	GetApp().ExecuteCommand(asrbc);


	asrbc.speckType = WorldCommands::SpeckType::RigidBody;
	asrbc.frictionCoefficient = 0.1f;
	asrbc.newSpecks.resize(64);
	n = (int)pow(asrbc.newSpecks.size(), 1.0f / 3.0f);
	nPow3 = n*n*n;
	width		= 0.6f;
	height		= 0.6f;
	depth		= 0.6f;
	x = -0.5f*width + 0.0f;
	y = -0.5f*height + 8.0f;
	z = -0.5f*depth + 0.1f;
	dx = width / (n - 1);
	dy = height / (n - 1);
	dz = depth / (n - 1);
	for (int k = 0; k < n; ++k)
	{
		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < n; ++j)
			{
				int index = k*n*n + i*n + j;
				// Position instanced along a 3D grid.
				asrbc.newSpecks[index].position = XMFLOAT3(x + j*dx /*+ 0.1f*sin(i*0.1f)*/, y + i*dy, z + k*dz /*+ 0.1f*cos(i*0.1f)*/);
				asrbc.newSpecks[index].mass = 1.0f;
			}
		}
	}
	GetApp().ExecuteCommand(asrbc);


	//asrbc.speckType = WorldCommands::SpeckType::RigidBody;
	//asrbc.newSpecks.resize(8);
	//asrbc.newSpecks[0].position = XMFLOAT3(0.0f, 5.0f, 0.0f);
	//asrbc.newSpecks[0].mass = 1.0f;
	//asrbc.newSpecks[1].position = XMFLOAT3(0.2f, 5.0f, 0.0f);
	//asrbc.newSpecks[1].mass = 1.0f;
	//asrbc.newSpecks[2].position = XMFLOAT3(0.0f, 5.0f, 0.2f);
	//asrbc.newSpecks[2].mass = 1.0f;
	//asrbc.newSpecks[3].position = XMFLOAT3(0.0f, 5.2f, 0.0f);
	//asrbc.newSpecks[3].mass = 1.0f;
	//asrbc.newSpecks[4].position = XMFLOAT3(0.0f, 5.4f, 0.0f);
	//asrbc.newSpecks[4].mass = 1.0f;
	//asrbc.newSpecks[5].position = XMFLOAT3(0.0f, 5.6f, 0.0f);
	//asrbc.newSpecks[5].mass = 1.0f;
	//asrbc.newSpecks[6].position = XMFLOAT3(0.0f, 5.8f, 0.0f);
	//asrbc.newSpecks[6].mass = 1.0f;
	//asrbc.newSpecks[7].position = XMFLOAT3(0.0f, 6.0f, 0.0f);
	//asrbc.newSpecks[7].mass = 1.0f;
	//GetApp().ExecuteCommand(asrbc);


	//asrbc.frictionCoefficient = 0.0f;
	//asrbc.newSpecks.resize(2);
	//asrbc.newSpecks[0].position = XMFLOAT3(0.0f, 1.0f, 0.0f);
	//asrbc.newSpecks[0].mass = 1.0f;
	//asrbc.newSpecks[1].position = XMFLOAT3(0.0f, 2.0f, 0.0f);
	//asrbc.newSpecks[1].mass = 1.0f;
	//GetApp().ExecuteCommand(asrbc);


	// Add an env map
	WorldCommands::AddEnviromentMapCommand cmd4;
	cmd4.name = "firstEnvMap";
	cmd4.pos = { 0.0f, 0.0f, 10.0f };
	cmd4.width = 1024;
	cmd4.height = 1024;
	GetApp().ExecuteCommand(cmd4);

	// add gravity
	WorldCommands::AddExternalForceCommand efc;
	efc.vector = XMFLOAT3(0.0f, -10.0f, 0.0f);
	efc.type = ExternalForces::Types::Acceleration;
	GetApp().ExecuteCommand(efc);
}

void MainState::Update(float dt)
{
	mCC.Update(dt);
	GetEngineCore().GetCamera().Update(dt, &mCC);

	AppCommands::SetWindowTitleCommand cmd;
	wstring fpsStr = to_wstring(GetEngineCore().GetProcessAndSystemData()->mFPS);
	wstring mspfStr = to_wstring(GetEngineCore().GetProcessAndSystemData()->m_mSPF);
	cmd.text = L"    fps: " + fpsStr + L"   spf: " + mspfStr;
	GetApp().ExecuteCommand(cmd);

	static float t = 0.0f;
	t += dt;
	if (t > 6.0f)
	{
		t = -100000.0f;

		//// Add an env map
		//WorldCommands::AddEnviromentMapCommand cmd4;
		//cmd4.name = "secondEnvMap";
		//cmd4.pos = { 0.0f, 10.0f, 0.0f };
		//cmd4.width = 1024;
		//cmd4.height = 1024;
		////GetApp().ExecuteCommand(cmd4);


		//// Add speck
		//WorldCommands::AddSpecksCommand asc;
		//asc.newSpecks.resize(1);
		//GetApp().ExecuteCommand(asc);




		WorldCommands::AddSpecksCommand asrbc;
		asrbc.speckType = WorldCommands::SpeckType::Fluid;
		asrbc.frictionCoefficient = 0.001f;
		asrbc.newSpecks.resize(32000);
		int n = (int)pow(asrbc.newSpecks.size(), 1.0f / 3.0f);
		int nPow3 = n*n*n;
		float width = 6.4f;
		float height = 6.4f;
		float depth = 6.4f;
		float x = -0.5f*width - 3.0f;
		float y = -0.5f*height + 5.0f;
		float z = -0.5f*depth - 1.0f;
		float dx = width / (n - 1);
		float dy = height / (n - 1);
		float dz = depth / (n - 1);
		//for (int k = 0; k < n; ++k)
		//{
		//	for (int i = 0; i < n; ++i)
		//	{
		//		for (int j = 0; j < n; ++j)
		//		{
		//			int index = k*n*n + i*n + j;
		//			// Position instanced along a 3D grid.
		//			asrbc.newSpecks[index].position = XMFLOAT3(x + j*dx + 0.1f*sin(i*0.1f), y + i*dy, z + k*dz + 0.1f*cos(i*0.1f));
		//			asrbc.newSpecks[index].mass = 1.0f;
		//		}
		//	}
		//}
		//GetApp().ExecuteCommand(asrbc);



		asrbc.speckType = WorldCommands::SpeckType::RigidBody;
		asrbc.frictionCoefficient = 0.6f;
		asrbc.newSpecks.resize(125);
		n = (int)pow(asrbc.newSpecks.size(), 1.0f / 3.0f);
		nPow3 = n*n*n;
		width = 0.8f;
		height = 0.8f;
		depth = 0.8f;
		x = -0.5f*width + 0.4f;
		y = -0.5f*height + 10.0f;
		z = -0.5f*depth + 0.0f;
		dx = width / (n - 1);
		dy = height / (n - 1);
		dz = depth / (n - 1);
		for (int k = 0; k < n; ++k)
		{
			for (int i = 0; i < n; ++i)
			{
				for (int j = 0; j < n; ++j)
				{
					int index = k*n*n + i*n + j;
					// Position instanced along a 3D grid.
					asrbc.newSpecks[index].position = XMFLOAT3(x + j*dx /*+ sin(i*0.1f)*/, y + i*dy, z + k*dz /*+ cos(i*0.1f)*/);
					asrbc.newSpecks[index].mass = 1.0f;
				}
			}
		}
		//GetApp().ExecuteCommand(asrbc);




	}

}

void MainState::Draw(float dt)
{
}

void MainState::OnResize()
{
}

void MainState::BuildOffScreenViews()
{
}
