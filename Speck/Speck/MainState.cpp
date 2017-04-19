
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

	// Create material
	AppCommands::CreateMaterialCommand cmd1;
	cmd1.materialName = "testMat1";
	cmd1.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 0.5f, 1.0f);
	cmd1.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	cmd1.Roughness = 1.0f;
	GetApp().ExecuteCommand(cmd1);

	cmd1.materialName = "testMat2";
	cmd1.DiffuseAlbedo = XMFLOAT4(1.0f, 0.5f, 1.0f, 1.0f);
	cmd1.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	cmd1.Roughness = 1.0f;
	GetApp().ExecuteCommand(cmd1);

	cmd1.materialName = "testMat3";
	cmd1.DiffuseAlbedo = XMFLOAT4(0.5f, 1.0f, 1.0f, 1.0f);
	cmd1.FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	cmd1.Roughness = 1.0f;
	GetApp().ExecuteCommand(cmd1);

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

	// Create PSO group
	WorldCommands::CreatePSOGroup cmd2;
	cmd2.PSOGroupName = "single";
	cmd2.vsName = "standardVS";
	cmd2.psName = "standardPS";
	GetApp().ExecuteCommand(cmd2);

	// Add an object
	WorldCommands::AddRenderItemCommand cmd3;
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(4.0f, 2.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	cmd3.geometryName = "shapeGeo";
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = "sphere";
	cmd3.PSOGroupName = "single";
	GetApp().ExecuteCommand(cmd3);

	// Add an env map
	WorldCommands::AddEnviromentMapCommand cmd4;
	cmd4.name = "firstEnvMap";
	cmd4.pos = { 0.0f, 0.0f, 10.0f };
	cmd4.width = 1024;
	cmd4.height = 1024;
	GetApp().ExecuteCommand(cmd4);
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
	if (t > 5.0f)
	{
		t = -100000.0f;

		// Add an env map
		WorldCommands::AddEnviromentMapCommand cmd4;
		cmd4.name = "secondEnvMap";
		cmd4.pos = { 0.0f, 10.0f, 0.0f };
		cmd4.width = 1024;
		cmd4.height = 1024;
		//GetApp().ExecuteCommand(cmd4);
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
