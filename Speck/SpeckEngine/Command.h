
#ifndef COMMAND_H
#define COMMAND_H

#include "SpeckEngineDefinitions.h"

namespace Speck
{
	//
	// Command result from the execution.
	//
	struct CommandResult
	{
		virtual ~CommandResult() = 0 {};
	};

	// 
	// Double-dispatch visitor design pattern used for interfacing with the engine.
	//
	struct AppCommand
	{
		friend class App;

	protected:
		virtual int Execute(void *ptIn, CommandResult *ptOut) const = 0;
	};

	struct WorldCommand
	{
		friend class World;

	protected:
		virtual int Execute(void *ptIn, CommandResult *ptOut) const = 0;
	};
}

#endif
