
#include "ManySkeletonsTestingState.h"
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

ManySkeletonsTestingState::ManySkeletonsTestingState(EngineCore &mEC)
	: AppState(),
	mCC(mEC)
{
}

ManySkeletonsTestingState::~ManySkeletonsTestingState()
{
	mHumanoidSkeletons.clear();
}

void ManySkeletonsTestingState::Initialize()
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
	staticPrimGen.GenerateBox({ 250.0f, 10.0f, 250.0f }, "pbrMatTest", { 0.0f, -10.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }); // ground

	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			staticPrimGen.GenerateBox(
			{ 5.0f, 5.0f, 50.0f },
				"pbrMatTest",
			{ -75.0f + 30.0f * i + 15.0f * (j % 2), 20.0f + 30.0f * j + (j > 3)*(j - 3)*(j - 3)*15.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f }); // ground

		}
	}

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

	int ragdollCount = 0;
	for (int i = 0; i < 35; ++i)
	{
		unique_ptr<HumanoidSkeleton> humSkeleton1 = make_unique<HumanoidSkeleton>(&mFBXSceneManager, GetWorld());
		humSkeleton1->Initialize(L"Data/Animations/knight_idle.fbx", L"Data/Animations/knight.json", &GetApp(), false);
		humSkeleton1->SetWorldTransform(XMMatrixTranslation(25.0f, 200.0f + 30.0f * i, 0.0f));
		mHumanoidSkeletons.push_back(move(humSkeleton1));

		unique_ptr<HumanoidSkeleton> humSkeleton2 = make_unique<HumanoidSkeleton>(&mFBXSceneManager, GetWorld());
		humSkeleton2->Initialize(L"Data/Animations/knight_idle.fbx", L"Data/Animations/knight.json", &GetApp(), false);
		humSkeleton2->SetWorldTransform(XMMatrixTranslation(0.0f, 200.0f + 30.0f * i, 0.0f));
		mHumanoidSkeletons.push_back(move(humSkeleton2));

		unique_ptr<HumanoidSkeleton> humSkeleton3 = make_unique<HumanoidSkeleton>(&mFBXSceneManager, GetWorld());
		humSkeleton3->Initialize(L"Data/Animations/knight_idle.fbx", L"Data/Animations/knight.json", &GetApp(), false);
		humSkeleton3->SetWorldTransform(XMMatrixTranslation(-25.0f, 200.0f + 30.0f * i, 0.0f));
		mHumanoidSkeletons.push_back(move(humSkeleton3));
	
		ragdollCount += 3;
	}

	WorldCommands::SetTimeMultiplierCommand stmc;
	stmc.timeMultiplierConstant = 2.0f;
	GetWorld().ExecuteCommand(stmc);

	// position camera
	SetLookAtCameraController slacc(XMFLOAT3(0.0f, 150.0f, -150.0f), XMFLOAT3(0.0f, 50.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
	AppCommands::UpdateCameraCommand ucc;
	ucc.ccPt = &slacc;
	GetApp().ExecuteCommand(ucc);
}

void ManySkeletonsTestingState::Update(float dt)
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

	if (t > 1.0f)
	{
		for (auto &sk : mHumanoidSkeletons)
			sk->StartSimulation();
	}

	t += dt;
}

void ManySkeletonsTestingState::Draw(float dt)
{
}

void ManySkeletonsTestingState::OnResize()
{
}

void ManySkeletonsTestingState::BuildOffScreenViews()
{
}
