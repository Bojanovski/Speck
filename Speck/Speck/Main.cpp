
#include <SpeckEngineDefinitions.h>
#include <EngineCore.h>
#include <World.h>
#include <App.h>
#include "MainState.h"
#include "JointTestingState.h"

using namespace Speck;
using namespace std;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		unique_ptr<EngineCore> ec;
		unique_ptr<World> w;
		unique_ptr<App> app;
		CreateSpeckApp(hInstance, 0.25f, 100, 1000, &ec, &w, &app);
		if (!app->Initialize())
			return 0;

		int lol = app->Run(make_unique<JointTestingState>(*ec));


		app.reset();
		w.reset();
		ec.reset();

		return lol;

	}
	catch ( Exception &ex)
	{
		MessageBox(nullptr, ex.Message(), L"Unhandled exception", MB_OK);
		return 0;
	}
}
