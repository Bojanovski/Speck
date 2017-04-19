
#ifndef COMMAND_H
#define COMMAND_H

#include "SpeckEngineDefinitions.h"

namespace Speck
{
	// 
	// Double-dispatch visitor design pattern used for interfacing with the engine.
	//
	struct Command
	{
		friend class App;

	protected:
		virtual int Execute(void *ptIn, void *ptOut) const = 0;
	};
}

#endif
