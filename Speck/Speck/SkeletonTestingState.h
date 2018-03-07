
#ifndef SKELETON_TESTING_STATE_H
#define SKELETON_TESTING_STATE_H

#include <SpeckEngineDefinitions.h>
#include <App.h>
#include <FPCameraController.h>
#include "HumanoidSkeleton.h"

class SkeletonTestingState : public Speck::AppState
{
public:
	SkeletonTestingState(Speck::EngineCore &mEC);
	~SkeletonTestingState() override;

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

};

#endif
