
#ifndef WORLD_H
#define WORLD_H

#include "SpeckEngineDefinitions.h"

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
		virtual void Update(App* app) = 0;
		virtual void PreDrawUpdate(App* app) = 0;
		virtual void Draw(App* app, UINT stage) = 0;
	};
}

#endif
