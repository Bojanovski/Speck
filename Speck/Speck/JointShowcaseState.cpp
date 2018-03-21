
#include "JointShowcaseState.h"
#include <EngineCore.h>
#include <ProcessAndSystemData.h>
#include <AppCommands.h>
#include <WorldCommands.h>
#include <World.h>
#include <InputHandler.h>
#include <SetLookAtCameraController.h>
#include "SpeckPrimitivesGenerator.h"
#include "StaticPrimitivesGenerator.h"

using namespace std;
using namespace DirectX;
using namespace Speck;

JointShowcaseState::JointShowcaseState(EngineCore &mEC)
	: AppState(),
	mCC(mEC)
{
}

JointShowcaseState::~JointShowcaseState()
{
}

#pragma optimize("", off)
void JointShowcaseState::Initialize()
{
	AppCommands::LimitFrameTimeCommand lftc;
	lftc.minFrameTime = 1.0f / 60.0f;
	GetApp().ExecuteCommand(lftc);

	WorldCommands::GetWorldPropertiesCommand gwp;
	WorldCommands::GetWorldPropertiesCommandResult gwpr;
	GetWorld().ExecuteCommand(gwp, &gwpr);
	float speckRadius = gwpr.speckRadius;

	WorldCommands::SetSpecksSolverParametersCommand sspc;
	sspc.substepsIterations = 4;
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

	//
	// Generate the world
	//
	StaticPrimitivesGenerator staticPrimGen(GetWorld());
	staticPrimGen.GenerateBox({ 270.0f, 5.0f, 270.0f }, "pbrMatTest", { 0.0f, -4.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }); // ground

	SpeckPrimitivesGenerator speckPrimGen(GetWorld());
	speckPrimGen.GeneratreConstrainedRigidBodyPair(SpeckPrimitivesGenerator::ConstrainedRigidBodyPairType::StiffJoint, { -20.0f, 10.0f, 0.0f }, { 0.0f, 0.0f, XM_PIDIV2 });
	speckPrimGen.GeneratreConstrainedRigidBodyPair(SpeckPrimitivesGenerator::ConstrainedRigidBodyPairType::HingeJoint, { -10.0f, 10.0f, 0.0f }, { 0.0f, 0.0f, XM_PIDIV2 });
	speckPrimGen.GeneratreConstrainedRigidBodyPair(SpeckPrimitivesGenerator::ConstrainedRigidBodyPairType::AsymetricHingeJoint, { 0.0f, 10.0f, 0.0f }, { 0.0f, 0.0f, XM_PIDIV2 });
	speckPrimGen.GeneratreConstrainedRigidBodyPair(SpeckPrimitivesGenerator::ConstrainedRigidBodyPairType::BallAndSocket, { 10.0f, 10.0f, 0.0f }, { 0.0f, 0.0f, XM_PIDIV2 });
	speckPrimGen.GeneratreConstrainedRigidBodyPair(SpeckPrimitivesGenerator::ConstrainedRigidBodyPairType::UniversalJoint, { 20.0f, 10.0f, 0.0f }, { 0.0f, 0.0f, XM_PIDIV2 });

	//speckPrimGen.GenerateBox(1, 4, 4, "pbrMatTest", false, { -5.0f, 20.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
	//speckPrimGen.GenerateBox(1, 1, 4, "pbrMatTest", false, { 0.0f, 20.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
	//speckPrimGen.GenerateBox(4, 4, 1, "pbrMatTest", false, { 5.0f, 20.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });

	// add gravity
	WorldCommands::AddExternalForceCommand efc;
	efc.vector = XMFLOAT3(0.0f, -10.0f, 0.0f);
	efc.type = ExternalForces::Types::Acceleration;
	GetWorld().ExecuteCommand(efc);

	// modify the time
	WorldCommands::SetTimeMultiplierCommand stmc;
	stmc.timeMultiplierConstant = 1.0f;
	GetWorld().ExecuteCommand(stmc);

	// position the camera
	SetLookAtCameraController slacc(XMFLOAT3(0.0f, 28.0f, -30.0f), XMFLOAT3(0.0f, 10.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
	AppCommands::UpdateCameraCommand ucc;
	ucc.ccPt = &slacc;
	GetApp().ExecuteCommand(ucc);

	// set title
	AppCommands::SetWindowTitleCommand cmd;
	cmd.text = L"Press 1, 2 & 3 to change the angular velocity. Press F to release.";
	GetApp().ExecuteCommand(cmd);
}
#pragma optimize("", on)

void JointShowcaseState::Update(float dt)
{
	mCC.Update(dt);
	AppCommands::UpdateCameraCommand ucc;
	ucc.ccPt = &mCC;
	ucc.deltaTime = dt;
	GetApp().ExecuteCommand(ucc);

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
	if (pressed_2_Time > 0.0f) anglularVelocity = 0.0f;

	float pressed_3_Time;
	ih->IsPressed(0x33, &pressed_3_Time); // 3 key
	if (pressed_3_Time > 0.0f) anglularVelocity -= dt * 2.0f;

	angle += anglularVelocity * dt;

	if (fPressedTime == 0.0f)
	{
		WorldCommands::UpdateSpeckRigidBodyCommand command;
		command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;
		command.rigidBodyIndex = 0;
		XMStoreFloat3(&command.transform.mT, XMVectorSet(-20.0f, 10.0f, 0.0f, 1.0f));
		XMStoreFloat4(&command.transform.mR, XMQuaternionRotationRollPitchYaw(angle, 0, 0));
		GetWorld().ExecuteCommand(command);

		command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;
		command.rigidBodyIndex = 2;
		XMStoreFloat3(&command.transform.mT, XMVectorSet(-10.0f, 10.0f, 0.0f, 1.0f));
		XMStoreFloat4(&command.transform.mR, XMQuaternionRotationRollPitchYaw(angle, 0, 0));
		GetWorld().ExecuteCommand(command);

		command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;
		command.rigidBodyIndex = 4;
		XMStoreFloat3(&command.transform.mT, XMVectorSet(0.0f, 10.0f, 0.0f, 1.0f));
		XMStoreFloat4(&command.transform.mR, XMQuaternionRotationRollPitchYaw(angle, 0, 0));
		GetWorld().ExecuteCommand(command);

		command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;
		command.rigidBodyIndex = 6;
		XMStoreFloat3(&command.transform.mT, XMVectorSet(10.0f, 10.0f, 0.0f, 1.0f));
		XMStoreFloat4(&command.transform.mR, XMQuaternionRotationRollPitchYaw(angle, 0, 0));
		GetWorld().ExecuteCommand(command);

		command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;
		command.rigidBodyIndex = 8;
		XMStoreFloat3(&command.transform.mT, XMVectorSet(20.0f, 10.0f, 0.0f, 1.0f));
		XMStoreFloat4(&command.transform.mR, XMQuaternionRotationRollPitchYaw(angle, 0, 0));
		GetWorld().ExecuteCommand(command);
	}
	else
	{
		WorldCommands::UpdateSpeckRigidBodyCommand command;
		command.movementMode = WorldCommands::RigidBodyMovementMode::GPU;
		command.rigidBodyIndex = 0;
		GetWorld().ExecuteCommand(command);

		command.movementMode = WorldCommands::RigidBodyMovementMode::GPU;
		command.rigidBodyIndex = 2;
		GetWorld().ExecuteCommand(command);

		command.movementMode = WorldCommands::RigidBodyMovementMode::GPU;
		command.rigidBodyIndex = 4;
		GetWorld().ExecuteCommand(command);

		command.movementMode = WorldCommands::RigidBodyMovementMode::GPU;
		command.rigidBodyIndex = 6;
		GetWorld().ExecuteCommand(command);

		command.movementMode = WorldCommands::RigidBodyMovementMode::GPU;
		command.rigidBodyIndex = 8;
		GetWorld().ExecuteCommand(command);
	}
}

void JointShowcaseState::Draw(float dt)
{
}

void JointShowcaseState::OnResize()
{
}

void JointShowcaseState::BuildOffScreenViews()
{
}
