//-------------------------------------------------------------------------------------
//		Commands used for handling the main window and resource management
//		among other things.
//-------------------------------------------------------------------------------------

#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#include "Command.h"
#include "SpeckEngineDefinitions.h"
#include "Camera.h"
#include "CameraController.h"

namespace Speck
{
	namespace AppCommands
	{
		struct LimitFrameTimeCommand : AppCommand
		{
			// If frames per second count is too high, frame time will be too low and
			// sometimes that can yield unwanted behaviour. This number sets each frame
			// to the minimum in length if needed.
			float minFrameTime = 0.0f;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct UpdateCameraCommand : AppCommand
		{
			CameraController *ccPt;
			float deltaTime = 0.0f;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct SetWindowTitleCommand : AppCommand
		{
			std::wstring text = L"";
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		enum struct ResourceType { Texture, Shader, Geometry };

		struct LoadResourceCommand : AppCommand
		{
			std::string name = "";
			std::wstring path = L"";
			ResourceType resType;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct CreateMaterialCommand : AppCommand
		{
			std::string materialName;

			// Basic constants
			DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
			DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
			float Roughness = 0.25f;

			// Textures
			std::string albedoTexName = "";
			std::string normalTexName = "";
			std::string heightTexName = "";
			std::string metalnessTexName = "";
			std::string roughnessTexName = "";
			std::string aoTexName = "";

		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct StaticMeshVertex
		{
			DirectX::XMFLOAT3 Position;
			DirectX::XMFLOAT3 Normal;
			DirectX::XMFLOAT3 TangentU;
			DirectX::XMFLOAT2 TexC;
		};

		struct CreateStaticGeometryCommand : AppCommand
		{
			std::string geometryName = "";
			std::string meshName = "";
			std::vector<StaticMeshVertex> vertices;
			std::vector<std::uint32_t> indices;

		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct SkinnedMeshVertex
		{
			DirectX::XMFLOAT4 Position[MAX_BONES_PER_VERTEX];
			DirectX::XMFLOAT3 Normal[MAX_BONES_PER_VERTEX];
			DirectX::XMFLOAT3 TangentU[MAX_BONES_PER_VERTEX];
			DirectX::XMFLOAT2 TexC;
			int BoneIndices[MAX_BONES_PER_VERTEX];
		};

		struct CreateSkinnedGeometryCommand : AppCommand
		{
			std::string geometryName = "";
			std::string meshName = "";
			std::vector<SkinnedMeshVertex> vertices;
			std::vector<std::uint32_t> indices;

		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

	}
}

#endif
