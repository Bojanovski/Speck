//-------------------------------------------------------------------------------------
//		Commands used for handling the world. This includes but is not limited
//		to adding render items, removing and updating them.
//-------------------------------------------------------------------------------------

#ifndef WORLD_COMMANDS_H
#define WORLD_COMMANDS_H

#include "Command.h"
#include "SpeckEngineDefinitions.h"
#include "MathHelper.h"
#include "PhysicsDataStructs.h"
#include "Transform.h"

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
			int ID = -1;
			std::string geometryName;
			// Transform of the object in the world.
			Transform transform = Transform::Identity();
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

		//
		// Physics
		//
		struct AddStaticColliderCommand : Command
		{
			// Transform of the static collider in the world.
			Transform transform = Transform::Identity();
			DLL_EXPORT AddStaticColliderCommand();
			int GetID() const { return ID; }
		private:
			mutable int ID;
			static int IDCounter;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, void *ptOut) const override;
		};

		struct AddExternalForceCommand : Command
		{
			// Vector value of the external force. Use of this data is based on force type.
			DirectX::XMFLOAT3 vector;
			// Type of the external force.
			ExternalForces::Types type;
			DLL_EXPORT AddExternalForceCommand();
			int GetID() const { return ID; }
		private:
			mutable int ID;
			static int IDCounter;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, void *ptOut) const override;
		};

		struct NewSpeck
		{
			// Initial world space position of the speck.
			DirectX::XMFLOAT3 position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			// Initial velocity of the speck.
			DirectX::XMFLOAT3 velocity = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			float mass = 1.0f;
		};

		enum SpeckType { Normal, RigidBody, Fluid };

		struct AddSpecksCommand : Command
		{
			// List of specks to be added to the engine.
			std::vector<NewSpeck> newSpecks;
			// Type of the new specks.
			SpeckType speckType;
			// Friction coefficient
			float frictionCoefficient = 0.5f;
			// Cohesion coefficient
			float cohesionCoefficient = 0.03f;
			// Viscosity coefficient
			float viscosityCoefficient = 0.03f;
			// Speck mass
			float speckMass = 1.0f;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, void *ptOut) const override;

		private:
			void AddRigidBody(void *ptIn) const;
		};
	}
}

#endif
