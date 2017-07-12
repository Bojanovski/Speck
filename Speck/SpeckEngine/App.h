
#ifndef GAME_STATES_MANAGER_H
#define GAME_STATES_MANAGER_H

#include "SpeckEngineDefinitions.h"
#include "EngineUser.h"
#include "WorldUser.h"
#include "Command.h"

namespace Speck
{
	class App;
	class AppStatesManager;

	//
	//	Abstract state that uses the App.
	//
	class AppState
	{
		friend class AppStatesManager;

	public:
		DLL_EXPORT AppState();
		DLL_EXPORT virtual ~AppState();

		// Make these inaccessible.
		AppState(const AppState &s) = delete;
		AppState &operator=(const AppState &s) = delete;

	protected:
		DLL_EXPORT virtual void Initialize() = 0;

		DLL_EXPORT App &GetApp();
		DLL_EXPORT App const &GetApp() const;

		DLL_EXPORT World &GetWorld();
		DLL_EXPORT World const &GetWorld() const;

		DLL_EXPORT EngineCore &GetEngineCore();
		DLL_EXPORT EngineCore const &GetEngineCore() const;

	private:
		virtual void Update(float dt) = 0;
		virtual void Draw(float dt) = 0;
		virtual void OnResize() = 0;

	private:
		AppStatesManager const *mOwner;
	};

	//
	//	Manager that is contained in the App.
	//
	class AppStatesManager
	{
		friend class App;
		friend class AppState;

	public:
		AppStatesManager(App *owner);
		~AppStatesManager();

		// Make these inaccessible.
		AppStatesManager(const AppStatesManager &as) = delete;
		AppStatesManager &operator=(const AppStatesManager &as) = delete;

		void Initialize();
		void Update(float dt);
		void Draw(float dt);
		void OnResize();

		App &GetApp() { return *mOwner; }
		App const &GetApp() const { return *mOwner; }

		EngineCore &GetEngineCore();
		EngineCore const &GetEngineCore() const;

	private:
		void PushState(std::unique_ptr<AppState> appState);
		void PopState();

	private:
		std::vector<std::unique_ptr<AppState>> mAppStatesStack;
		App *mOwner;
	};

	//
	//	Abstract App.
	//
	class App : public EngineUser, public WorldUser
	{
	public:
		App(EngineCore &ec, World &w);
		virtual ~App();
		App(const App& v) = delete;
		App& operator=(const App& v) = delete;

		virtual int Run(std::unique_ptr<AppState> initialState) = 0;
		virtual bool Initialize() = 0;
		virtual int ExecuteCommand(const AppCommand &command, CommandResult *result = 0);
		AppStatesManager	&GetAppStatesManager() { return mAppStatesManager; }

	protected:
		// A structure that manages app states.
		AppStatesManager mAppStatesManager;
	};
}
#endif