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
		struct GetWorldPropertiesCommandResult : CommandResult
		{
			float speckRadius;
		};

		struct GetWorldPropertiesCommand : WorldCommand
		{
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct CreatePSOGroupCommand : WorldCommand
		{
			std::string PSOGroupName = "";
			std::string vsName = "";
			std::string psName = "";
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		enum RenderItemType { Static, SpeckRigidBody, SpeckSkeletalBody };

		struct AddRenderItemCommand : WorldCommand
		{
			int ID = -1;
			std::string geometryName = "";
			RenderItemType type;
			std::string meshName = "";
			std::string materialName = "";
			DirectX::XMFLOAT4X4 texTransform = MathHelper::Identity4x4();
			// Name of the PSO group that will be used instead of the one determined by the 
			// type of the render item.
			std::string PSOGroupNameOverride = "";
			// Special properties
			union {
				struct
				{
					// Transform of the object in the world.
					Transform worldTransform;
				} staticRenderItem;
				struct
				{
					// Transform of the object in the local space of the rigid body.
					Transform localTransform;
					// Index of the respective speck rigid body.
					UINT rigidBodyIndex;
				} speckRigidBodyRenderItem;
				struct
				{
					//...
				} speckSkeletalBodyRenderItem;
			};
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct AddEnviromentMapCommand : WorldCommand
		{
			std::string name = "";
			DirectX::XMFLOAT3 pos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			UINT width = 512;
			UINT height = 512;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct SetPSOGroupVisibilityCommand : WorldCommand
		{
			std::string PSOGroupName = "";
			bool visible = true;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		//
		// Physics
		//
		struct AddStaticColliderCommand : WorldCommand
		{
			// Transform of the static collider in the world.
			Transform transform = Transform::Identity();
			DLL_EXPORT AddStaticColliderCommand();
			int GetID() const { return ID; }
		private:
			mutable int ID;
			static int IDCounter;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct AddExternalForceCommand : WorldCommand
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
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct NewSpeck
		{
			// Initial world space position of the speck.
			DirectX::XMFLOAT3 position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			// Initial velocity of the speck.
			DirectX::XMFLOAT3 velocity = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		};

		enum SpeckType { Normal, RigidBody, RigidBodyJoint, Fluid };

		struct AddSpecksCommandResult : CommandResult
		{
			UINT rigidBodyIndex;
		};

		struct AddSpecksCommand : WorldCommand
		{
			// List of specks to be added to the engine.
			std::vector<NewSpeck> newSpecks;

			// Type of the new specks.
			SpeckType speckType;

			// Friction coefficient
			float frictionCoefficient = 0.5f;

			// Speck mass
			float speckMass = 1.0f;

			// Special properties
			union {
				struct
				{
					// Cohesion coefficient
					float cohesionCoefficient;

					// Viscosity coefficient
					float viscosityCoefficient;
				} fluid;
				struct
				{
					// Indices of the two connected rigid bodies
					UINT rigidBodyIndex[2];

					// Calculate signed distance field gradient for collision resolution
					bool calculateSDFGradient;
				} rigidBodyJoint;
			};
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;

		private:
			void AddRigidBody(void *ptIn, CommandResult *result) const;
			void AddRigidBodyJoint(void *ptIn, CommandResult *result) const;
		};

		enum RigidBodyMovementMode { CPU, GPU };

		struct UpdateSpeckRigidBodyCommand : WorldCommand
		{
			RigidBodyMovementMode movementMode = RigidBodyMovementMode::GPU;
			UINT rigidBodyIndex;
			Transform transform = Transform::Identity();
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct SetTimeMultiplierCommand : WorldCommand
		{
			float timeMultiplierConstant = 1.0f;
		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};

		struct SetSpecksSolverParametersCommand : WorldCommand
		{
			UINT stabilizationIteraions = UINT_MAX;
			UINT solverIterations = UINT_MAX;
			UINT substepsIterations = UINT_MAX;

		protected:
			DLL_EXPORT virtual int Execute(void *ptIn, CommandResult *result) const override;
		};
	}
}

#endif
