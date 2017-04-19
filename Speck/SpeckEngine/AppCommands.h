//-------------------------------------------------------------------------------------
//		Commands used for handling the main window and resource management
//		among other things.
//-------------------------------------------------------------------------------------

#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#include "Command.h"
#include "SpeckEngineDefinitions.h"

namespace Speck
{
	namespace AppCommands
	{
		struct SetWindowTitleCommand : Command
		{
			std::wstring text;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, void *ptOut) const override;
		};

		enum struct ResourceType { Texture, Shader, Geometry };

		struct LoadResourceCommand : Command
		{
			std::string name;
			std::wstring path;
			ResourceType resType;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, void *ptOut) const override;
		};

		struct CreateMaterialCommand : Command
		{
			std::string materialName;

			// Basic constants
			DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
			DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
			float Roughness = 0.25f;

			// Textures
			std::string albedoTexName;
			std::string normalTexName;
			std::string heightTexName;
			std::string metalnessTexName;
			std::string roughnessTexName;
			std::string aoTexName;

		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, void *ptOut) const override;
		};
	}
}

#endif
