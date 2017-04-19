//-------------------------------------------------------------------------------------
//		Commands used for handling the world. This includes but is not limited
//		to adding render items, removing and updating them.
//-------------------------------------------------------------------------------------

#ifndef WORLD_COMMANDS_H
#define WORLD_COMMANDS_H

#include "Command.h"
#include "SpeckEngineDefinitions.h"
#include "MathHelper.h"

namespace Speck
{
	namespace WorldCommands
	{
		struct CreatePSOGroup : Command
		{
			std::string PSOGroupName;
			std::string vsName;
			std::string psName;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, void *ptOut) const override;
		};

		struct AddRenderItemCommand : Command
		{
			std::string geometryName;
			DirectX::XMFLOAT4X4 world = MathHelper::Identity4x4();
			std::string meshName;
			std::string materialName;
			DirectX::XMFLOAT4X4 texTransform = MathHelper::Identity4x4();
			std::string PSOGroupName;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, void *ptOut) const override;
		};

		struct AddEnviromentMapCommand : Command
		{
			std::string name;
			DirectX::XMFLOAT3 pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			UINT width = 512;
			UINT height = 512;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, void *ptOut) const override;
		};
	}
}

#endif
