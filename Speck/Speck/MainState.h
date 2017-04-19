

#include <SpeckEngineDefinitions.h>
#include <App.h>
#include <FPCameraController.h>

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
};