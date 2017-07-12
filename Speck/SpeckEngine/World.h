
#ifndef WORLD_H
#define WORLD_H

#include "SpeckEngineDefinitions.h"
#include "Command.h"

namespace Speck
{
	class App;

	// World (or scene) containing everything visible.
	class World
	{
	public:
		World();
		virtual ~World();
		World(const World& v) = delete;
		World& operator=(const World& v) = delete;

		virtual void Initialize(App* app) = 0;
		virtual int ExecuteCommand(const WorldCommand &command, CommandResult *result = 0);
		virtual void Update() = 0;
		virtual void PreDrawUpdate() = 0;
		virtual void Draw(UINT stage) = 0;

	protected:
		App *mApp;
	};
}

#endif
