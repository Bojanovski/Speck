
//-------------------------------------------------------------------------------------
// This header is ment to be included in every code file in project SpeckEngine.
// it will support useful definitions that will be shared througout the project.
//-------------------------------------------------------------------------------------

#ifndef SPECK_ENGINE_DEFINITIONS_H
#define SPECK_ENGINE_DEFINITIONS_H

#include <Windows.h>
#include <WindowsX.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <minwindef.h>		// For some common type renaming.
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <map>				// Slower, less memory.
#include <unordered_map>	// Faster, more memory.
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>

#include "Exceptions.h"

// Special file that holds defines that can be used both by shaders and c++ code.
#include "cpuGpuDefines.txt"

// Pointer handling
#define PDEL(pt) if (pt) {delete pt; pt = 0;}			// Safe delete pointer.
#define PDELARRAY(pt) if (pt) {delete[] pt; pt = 0;}	// Safe delete pointer array.

#define real float		// Real numbers could be float or double.

//
// Hard coded engine constants
//
#define MAX_LIGHTS 16				// maximal number of lights
#define NUM_FRAME_RESOURCES 3		// number of frames CPU can work on in advance before getting stopped by the belated GPU
#define SWAP_CHAIN_BUFFER_COUNT  2

//
// Macros
//
#define DLL_EXPORT __declspec(dllexport)

#define ERROR		0
#define WARNING		1
#define INFO		2
#define LOG(msg, code)									\
{														\
}

//
// Useful definitions
//
namespace Speck
{
	class App;
	class World;
	class EngineCore;
	DLL_EXPORT extern void CreateSpeckApp(HINSTANCE hInstance, UINT maxNumberOfMaterials, UINT maxInstancedObject, UINT maxSingleObjects, std::unique_ptr<EngineCore> *ec, std::unique_ptr<World> *world, std::unique_ptr<App> *app);
}

#endif
