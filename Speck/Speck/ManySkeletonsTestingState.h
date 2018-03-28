
#ifndef MANY_SKELETONS_TESTING_STATE_H
#define MANY_SKELETONS_TESTING_STATE_H

#include <SpeckEngineDefinitions.h>
#include <App.h>
#include <FPCameraController.h>
#include "HumanoidSkeleton.h"
#include "FBXSceneManager.h"

class ManySkeletonsTestingState : public Speck::AppState
{
public:
	ManySkeletonsTestingState(Speck::EngineCore &mEC);
	~ManySkeletonsTestingState() override;

protected:
	void Initialize() override;

private:
	void Update(float dt) override;
	void Draw(float dt) override;
	void OnResize() override;
	void BuildOffScreenViews();

private:
	Speck::FPCameraController mCC;
	std::vector<std::unique_ptr<HumanoidSkeleton>> mHumanoidSkeletons;
	FBXSceneManager mFBXSceneManager;

};

#endif
