
#include <SpeckEngineDefinitions.h>
#include <EngineCore.h>
#include <World.h>
#include <App.h>
#include "SkeletonTestingState.h"
#include "SkinnedSkeletonTestingState.h"
#include "ManySkeletonsTestingState.h"
#include "JointTestingState.h"
#include "JointShowcaseState.h"

using namespace Speck;
using namespace std;

int Test0(HINSTANCE hInstance)
{
	unique_ptr<EngineCore> ec;
	unique_ptr<World> w;
	unique_ptr<App> app;
	CreateSpeckApp(hInstance, 0.25f, 100, 1000, &ec, &w, &app);
	if (!app->Initialize())
		return 1;

	int result = app->Run(make_unique<JointShowcaseState>(*ec));
	return result;
}

int Test1(HINSTANCE hInstance)
{
	unique_ptr<EngineCore> ec;
	unique_ptr<World> w;
	unique_ptr<App> app;
	CreateSpeckApp(hInstance, 0.25f, 100, 1000, &ec, &w, &app);
	if (!app->Initialize())
		return 1;

	int result = app->Run(make_unique<JointTestingState>(*ec));
	return result;
}

int Test2(HINSTANCE hInstance)
{
	unique_ptr<EngineCore> ec;
	unique_ptr<World> w;
	unique_ptr<App> app;
	CreateSpeckApp(hInstance, 0.5f, 100, 1000, &ec, &w, &app);
	if (!app->Initialize())
		return 1;

	int result = app->Run(make_unique<SkeletonTestingState>(*ec));
	return result;
}

int Test3(HINSTANCE hInstance)
{
	unique_ptr<EngineCore> ec;
	unique_ptr<World> w;
	unique_ptr<App> app;
	CreateSpeckApp(hInstance, 0.5f, 100, 1000, &ec, &w, &app);
	if (!app->Initialize())
		return 1;

	int result = app->Run(make_unique<ManySkeletonsTestingState>(*ec));
	return result;
}

int Test4(HINSTANCE hInstance)
{
	unique_ptr<EngineCore> ec;
	unique_ptr<World> w;
	unique_ptr<App> app;
	CreateSpeckApp(hInstance, 0.5f, 100, 1000, &ec, &w, &app);
	if (!app->Initialize())
		return 1;

	int result = app->Run(make_unique<SkinnedSkeletonTestingState>(*ec));
	return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		return Test4(hInstance);
	}
	catch (Exception &ex)
	{
		MessageBox(nullptr, ex.Message(), L"Unhandled exception", MB_OK);
		return 1;
	}
}
