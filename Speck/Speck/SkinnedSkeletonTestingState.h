
#ifndef SKINNED_SKELETON_TESTING_STATE_H
#define SKINNED_SKELETON_TESTING_STATE_H

#include <SpeckEngineDefinitions.h>
#include <App.h>
#include <FPCameraController.h>
#include "HumanoidSkeleton.h"
#include "FBXSceneManager.h"

class SkinnedSkeletonTestingState : public Speck::AppState
{
public:
	SkinnedSkeletonTestingState(Speck::EngineCore &mEC);
	~SkinnedSkeletonTestingState() override;

protected:
	void Initialize() override;

private:
	void Update(float dt) override;
	void Draw(float dt) override;
	void OnResize() override;
	void BuildOffScreenViews();

private:
	Speck::FPCameraController mCC;
	std::unique_ptr<HumanoidSkeleton> mHumanoidSkeleton;
	FBXSceneManager mFBXSceneManager;
};

#endif
