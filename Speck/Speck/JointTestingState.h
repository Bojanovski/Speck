
#ifndef JOINT_TESTING_STATE_H
#define JOINT_TESTING_STATE_H

#include <SpeckEngineDefinitions.h>
#include <App.h>
#include <FPCameraController.h>

class JointTestingState : public Speck::AppState
{
public:
	JointTestingState(Speck::EngineCore &mEC);
	~JointTestingState() override;

protected:
	void Initialize() override;

private:
	void Update(float dt) override;
	void Draw(float dt) override;
	void OnResize() override;
	void BuildOffScreenViews();

private:
	Speck::FPCameraController mCC;

};

#endif
