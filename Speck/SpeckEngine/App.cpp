
#include "App.h"

using namespace std;
using namespace Speck;

AppState::AppState()
{

}

AppState::~AppState()
{

}

App &AppState::GetApp()
{ 
	return (App &)mOwner->GetApp();
}

App const &AppState::GetApp() const
{
	return  (App const &)mOwner->GetApp();
}

EngineCore &AppState::GetEngineCore()
{
	return  (EngineCore &)mOwner->GetEngineCore();
}

EngineCore const &AppState::GetEngineCore() const
{
	return  (EngineCore const &)mOwner->GetEngineCore();
}

AppStatesManager::AppStatesManager(App *owner)
	: mOwner(owner)
{

}

AppStatesManager::~AppStatesManager()
{

}

void AppStatesManager::Initialize()
{
	for (auto &appState : mAppStatesStack)
		appState->Initialize();
}

void AppStatesManager::Update(float dt)
{
	for (auto &appState : mAppStatesStack)
		appState->Update(dt);
}

void AppStatesManager::Draw(float dt)
{
	for (auto &appState : mAppStatesStack)
		appState->Draw(dt);
}

void AppStatesManager::OnResize()
{
	for (auto &appState : mAppStatesStack)
		appState->OnResize();
}

void AppStatesManager::PushState(unique_ptr<AppState> appState)
{
	appState->mOwner = this;
	appState->Initialize();
	appState->OnResize();
	mAppStatesStack.push_back(move(appState));
}

void AppStatesManager::PopState()
{
	mAppStatesStack.pop_back();
}

EngineCore &AppStatesManager::GetEngineCore()
{
	return mOwner->GetEngineCore();
}

EngineCore const &AppStatesManager::GetEngineCore() const
{
	return mOwner->GetEngineCore();
}

App::App(EngineCore &ec, World &w)
	: EngineUser(ec),
	WorldUser(w),
	mAppStatesManager(this)
{

}

App::~App()
{
}

int App::Run(unique_ptr<AppState> initialState)
{
	mAppStatesManager.PushState(move(initialState));
	return 0;
}

bool App::Initialize()
{
	return true;
}

int App::ExecuteCommand(const Command &command)
{
	return command.Execute(this, 0);
}
