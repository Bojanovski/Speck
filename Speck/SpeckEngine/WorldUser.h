
#ifndef WORLD_USER_H
#define WORLD_USER_H

namespace Speck
{
	class World;

	class WorldUser
	{
	public:
		WorldUser(World &w)
			: mWorld(w)
		{
		}
		~WorldUser() {	}

		// A descriptive interface.
		World &GetWorld() { return mWorld; }
		World const &GetWorld() const { return mWorld; }

	private:
		// A reference to the world.
		World &mWorld;
	};
}

#endif
