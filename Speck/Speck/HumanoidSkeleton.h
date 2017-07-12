
#ifndef HUMANOID_SKELETON_H
#define HUMANOID_SKELETON_H

#include <SpeckEngineDefinitions.h>
#include <WorldUser.h>
#include <WorldCommands.h>
#include <AppCommands.h>

namespace fbxsdk
{
	class FbxNode;
	class FbxAnimLayer;
	class FbxScene;
	class FbxManager;
}

class HumanoidSkeleton : public Speck::WorldUser
{
public:
	HumanoidSkeleton(Speck::World &w);
	~HumanoidSkeleton();

	void Initialize(const wchar_t *fbxFilePath, Speck::App *pApp, bool saveFixed = false);
	void UpdateAnimation(float time);
	void StartSimulation();

private:
	void ProcessRoot(fbxsdk::FbxNode *node, fbxsdk::FbxAnimLayer *animLayer, Speck::WorldCommands::AddSpecksCommand *outCommand, DirectX::XMMATRIX *localOut);
	void ProcessHips(fbxsdk::FbxNode *node, fbxsdk::FbxAnimLayer *animLayer, Speck::WorldCommands::AddSpecksCommand *outCommand, DirectX::XMMATRIX *localOut);
	void ProcessLegPart(fbxsdk::FbxNode *node, fbxsdk::FbxAnimLayer *animLayer, Speck::WorldCommands::AddSpecksCommand *outCommand, DirectX::XMMATRIX *worldOut, DirectX::XMMATRIX *localOut);
	void ProcessSpinePart(fbxsdk::FbxNode *node, fbxsdk::FbxAnimLayer *animLayer, Speck::WorldCommands::AddSpecksCommand *outCommand, DirectX::XMMATRIX *worldOut, DirectX::XMMATRIX *localOut);
	void ProcessChest(fbxsdk::FbxNode *node, fbxsdk::FbxAnimLayer *animLayer, Speck::WorldCommands::AddSpecksCommand *outCommand, DirectX::XMMATRIX *worldOut, DirectX::XMMATRIX *localOut);
	void ProcessShoulder(fbxsdk::FbxNode *node, fbxsdk::FbxAnimLayer *animLayer, Speck::WorldCommands::AddSpecksCommand *outCommand, DirectX::XMMATRIX *worldOut, DirectX::XMMATRIX *localOut, bool right);
	void ProcessArmPart(fbxsdk::FbxNode *node, fbxsdk::FbxAnimLayer *animLayer, Speck::WorldCommands::AddSpecksCommand *outCommand, DirectX::XMMATRIX *worldOut, DirectX::XMMATRIX *localOut, bool right);
	void ProcessNeck(fbxsdk::FbxNode *node, fbxsdk::FbxAnimLayer *animLayer, Speck::WorldCommands::AddSpecksCommand *outCommand, DirectX::XMMATRIX *worldOut, DirectX::XMMATRIX *localOut);
	void ProcessHead(fbxsdk::FbxNode *node, fbxsdk::FbxAnimLayer *animLayer, Speck::WorldCommands::AddSpecksCommand *outCommand, DirectX::XMMATRIX *worldOut, DirectX::XMMATRIX *localOut);

	void CreateSpecksBody(Speck::App *pApp);
	void ProcessRenderSkin(fbxsdk::FbxNode *node, Speck::App *pApp);

private:
	fbxsdk::FbxScene *mScene;
	fbxsdk::FbxManager *mSDKManager;
	float mSpeckRadius;
	struct NodeAnimData { UINT index; DirectX::XMFLOAT4X4 localTransform; };
	std::unordered_map < std::string, NodeAnimData > mNodesAnimData;
	std::unordered_map < std::string, std::string > mNamesDictionary;
	enum { None, BindPose, Animating, Simulating } mState = None;
};

#endif
