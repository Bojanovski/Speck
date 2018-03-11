
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

	void Initialize(const wchar_t *fbxFilePath, const wchar_t *speckStructureJSON, Speck::App *pApp, bool saveFixed = false);
	void UpdateAnimation(float time);
	void StartSimulation();

private:
	void ProcessBone(fbxsdk::FbxNode *node, Speck::WorldCommands::AddSpecksCommand *outCommand, const std::string &boneName);
	void CreateSpecksBody(Speck::App *pApp);
	void ProcessRenderSkin(fbxsdk::FbxNode *node, Speck::App *pApp);

private:
	fbxsdk::FbxScene *mScene;
	fbxsdk::FbxManager *mSDKManager;
	float mSpeckRadius;
	std::string mSpeckBodyStructureJSONFile;
	struct NodeAnimData 
	{
		UINT index = -1;
		UINT numberOfSpecks; 
		DirectX::XMFLOAT3 centerOfMass; 
		DirectX::XMFLOAT3 inverseScale; 
		DirectX::XMFLOAT4 inverseRotation;
		DirectX::XMFLOAT4X4 bindPoseWorldTransform;
	};
	std::unordered_map < std::string, NodeAnimData > mNodesAnimData;
	enum { None, BindPose, Animating, Simulating } mState = None;
};

#endif
