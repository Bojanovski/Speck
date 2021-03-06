
#include "SkeletonTestingState.h"
#include <EngineCore.h>
#include <ProcessAndSystemData.h>
#include <AppCommands.h>
#include <WorldCommands.h>
#include <World.h>
#include <SetLookAtCameraController.h>
#include "SpeckPrimitivesGenerator.h"
#include "StaticPrimitivesGenerator.h"

using namespace std;
using namespace DirectX;
using namespace Speck;

SkeletonTestingState::SkeletonTestingState(EngineCore &mEC)
	: AppState(),
	mCC(mEC)
{
}

SkeletonTestingState::~SkeletonTestingState()
{
	mHumanoidSkeleton.reset();
}

void SkeletonTestingState::Initialize()
{
	AppCommands::LimitFrameTimeCommand lftc;
	lftc.minFrameTime = 1.0f / 60.0f;
	GetApp().ExecuteCommand(lftc);

	WorldCommands::SetSpecksSolverParametersCommand sspc;
	sspc.substepsIterations = 2;
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

	cmc_pbr.materialName = "pbrMatTest2";
	//cmc_pbr.albedoTexName = lrc_albedo.name;
	//cmc_pbr.normalTexName = lrc_normal.name;
	//cmc_pbr.heightTexName = lrc_height.name;
	//cmc_pbr.metalnessTexName = lrc_metalness.name;
	//cmc_pbr.roughnessTexName = lrc_rough.name;
	//cmc_pbr.aoTexName = lrc_ao.name;
	cmc_pbr.DiffuseAlbedo = XMFLOAT4(0.3f, 0.9f, 0.5f, 1.0f);
	cmc_pbr.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	cmc_pbr.Roughness = 1.0f;
	GetApp().ExecuteCommand(cmc_pbr);

	//
	// Generate the world
	//
	StaticPrimitivesGenerator staticPrimGen(GetWorld());
	staticPrimGen.GenerateBox({ 70.0f, 5.0f, 70.0f }, "pbrMatTest", { 0.0f, -3.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }); // ground
	staticPrimGen.GenerateBox({ 5.0f, 200.0f, 60.0f }, "pbrMatTest", { 25.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
	staticPrimGen.GenerateBox({ 60.0f, 200.0f, 5.0f }, "pbrMatTest", { 0.0f, -0.0f, 15.0f }, { 0.0f, 0.0f, 0.0f });
	staticPrimGen.GenerateBox({ 5.0f, 200.0f, 60.0f }, "pbrMatTest", { -25.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
	staticPrimGen.GenerateBox({ 60.0f, 100.0f, 50.0f }, "pbrMatTest", { 0.0f, 0.0f, -40.0f }, { 0.0f, 0.0f, 0.0f });

	WorldCommands::AddSpecksCommand asrbc;
	asrbc.speckType = WorldCommands::SpeckType::Fluid;
	asrbc.frictionCoefficient = 0.01f;
	asrbc.fluid.cohesionCoefficient = 0.6f;
	asrbc.fluid.viscosityCoefficient = 0.7f;
	asrbc.speckMass = 0.5f;
	asrbc.newSpecks.resize(16000);
	int n = (int)pow(asrbc.newSpecks.size(), 1.0f / 3.0f);
	int nPow3 = n*n*n;
	float width = 15.0f;
	float height = 15.0f;
	float depth = 15.0f;
	float x = -0.5f*width - 3.0f;
	float y = -0.5f*height + 16.0f;
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
			}
		}
	}
	GetWorld().ExecuteCommand(asrbc);

	// Add some rigd bodies
	SpeckPrimitivesGenerator speckPrimGen(GetWorld());
	speckPrimGen.SetSpeckMass(2.0f);
	speckPrimGen.SetSpeckFrictionCoefficient(0.01f);
	speckPrimGen.GenerateBox(4, 4, 4, "pbrMatTest2", false, { 8.0f, 6.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
	speckPrimGen.GenerateBox(6, 2, 2, "pbrMatTest2", false, { 8.0f, 9.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });

	// Add an env map
	WorldCommands::AddEnviromentMapCommand cmd4;
	cmd4.name = "firstEnvMap";
	cmd4.pos = { 0.0f, 0.0f, 10.0f };
	cmd4.width = 1024;
	cmd4.height = 1024;
	GetWorld().ExecuteCommand(cmd4);

	// add gravity
	WorldCommands::AddExternalForceCommand efc;
	efc.vector = XMFLOAT3(0.0f, -10.0f, 0.0f);
	efc.type = ExternalForces::Types::Acceleration;
	GetWorld().ExecuteCommand(efc);

	mHumanoidSkeleton = make_unique<HumanoidSkeleton>(&mFBXSceneManager, GetWorld());
	mHumanoidSkeleton->Initialize(L"Data/Animations/knight_dancing.fbx", L"Data/Animations/knight.json", &GetApp(), false);

	WorldCommands::SetTimeMultiplierCommand stmc;
	stmc.timeMultiplierConstant = 2.0f;
	GetWorld().ExecuteCommand(stmc);

	// position camera
	SetLookAtCameraController slacc(XMFLOAT3(2.0f, 32.0f, -38.0f), XMFLOAT3(2.0f, 10.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
	AppCommands::UpdateCameraCommand ucc;
	ucc.ccPt = &slacc;
	GetApp().ExecuteCommand(ucc);
}

void SkeletonTestingState::Update(float dt)
{
	static float t = 0.0f;

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

	t += dt;

	if (t < 20.1f && t > 0.0f)
	{
		mHumanoidSkeleton->UpdateAnimation(t*1.0f);
	}
	else if (t > 20.1f)
	{
		mHumanoidSkeleton->StartSimulation();
	}
}

void SkeletonTestingState::Draw(float dt)
{
}

void SkeletonTestingState::OnResize()
{
}

void SkeletonTestingState::BuildOffScreenViews()
{
}
