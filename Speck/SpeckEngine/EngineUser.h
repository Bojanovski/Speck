
#ifndef ENGINE_USER_H
#define ENGINE_USER_H

namespace Speck
{
	class EngineCore;

	class EngineUser
	{
	public:
		EngineUser(EngineCore &ec)
			: mEC(ec)
		{

		}
		~EngineUser() {	}

		// A descriptive interface.
		EngineCore &GetEngineCore() { return mEC; }
		EngineCore const &GetEngineCore() const { return mEC; }

	private:
		// A reference to the engine core data structure.
		EngineCore &mEC;
	};
}

#endif
