
#ifndef JOINT_SHOWCASE_STATE_H
#define JOINT_SHOWCASE_STATE_H

#include <SpeckEngineDefinitions.h>
#include <App.h>
#include <FPCameraController.h>

class JointShowcaseState : public Speck::AppState
{
public:
	JointShowcaseState(Speck::EngineCore &mEC);
	~JointShowcaseState() override;

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
