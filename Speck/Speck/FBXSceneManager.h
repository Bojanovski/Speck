#pragma once

#include <map>
#include <string>

namespace fbxsdk
{
	class FbxScene;
	class FbxManager;
	class FbxIOSettings;
}

class FBXSceneManager
{
public:
	FBXSceneManager();
	~FBXSceneManager();

	fbxsdk::FbxManager *GetFBXSDKManager() { return mSDKManager; }
	fbxsdk::FbxScene *GetScene(const wchar_t* filePath);
	fbxsdk::FbxScene *FixSceneSaveScene(const wchar_t* filePath);

private:
	fbxsdk::FbxManager *mSDKManager;
	fbxsdk::FbxIOSettings * mIOs;
	std::map<std::wstring, fbxsdk::FbxScene *> mScenes;
};
