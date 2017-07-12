
#ifndef MAIN_STATE_H
#define MAIN_STATE_H

#include <SpeckEngineDefinitions.h>
#include <App.h>
#include <FPCameraController.h>
#include "HumanoidSkeleton.h"

class MainState : public Speck::AppState
{
public:
	MainState(Speck::EngineCore &mEC);
	~MainState() override;

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
