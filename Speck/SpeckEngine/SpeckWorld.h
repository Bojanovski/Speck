
#ifndef SPECK_WORLD_H
#define SPECK_WORLD_H

#include "SpeckEngineDefinitions.h"
#include "World.h"
#include "FrameResource.h"
#include "PhysicsDataStructs.h"

namespace Speck
{
	struct PSOGroup;
	struct FrameResource;
	struct PassConstants;
	struct SpecksRenderItem;
	class CubeRenderTarget;
	class SpecksHandler;

	namespace AppCommands
	{
	}

	namespace WorldCommands
	{
		struct AddRenderItemCommand;
	}

	// World (or scene) containing everything visible.
	class SpeckWorld : public World
	{
		friend class SpeckApp;
		friend struct WorldCommands::AddRenderItemCommand;

	public:
		SpeckWorld(UINT maxRenderItemsCount);
		~SpeckWorld();
		SpeckWorld(const SpeckWorld& v) = delete;
		SpeckWorld& operator=(const SpeckWorld& v) = delete;
		PassConstants const *GetPassConstants() const { return &mMainPassCB; }
		virtual void Initialize(App* app) override;
		virtual int ExecuteCommand(const WorldCommand &command, CommandResult *result = 0) override;
		virtual void Update() override;
		virtual void PreDrawUpdate() override;
		virtual void Draw(UINT stage) override;

		SpecksRenderItem *GetSpeckInstancesRenderItem() { return mSpeckInstancesRenderItem; }
		UINT GetMaxRenderItemsCount() const { return mMaxRenderItemsCount; }
		SpecksHandler *GetSpecksHandler() { return mSpecksHandler.get(); }

	private:
		void Draw_Scene();				// used to draw scene normally
		UINT GetRenderItemFreeSpace();

	public:
		// Rendering
		std::unordered_map<std::string, std::unique_ptr<PSOGroup>> mPSOGroups;
		std::unordered_map<std::string, std::unique_ptr<CubeRenderTarget>> mCubeRenderTarget;

		// Specks handler used to update the specks in the world
		std::unique_ptr<SpecksHandler> mSpecksHandler;

		// Specks
		std::vector<SpeckData> mSpecks;

		// Rigid bodies
		std::vector<SpeckRigidBodyData> mSpeckRigidBodyData;

		// Collision
		std::vector<StaticCollider> mStaticColliders;

		// External forces
		std::vector<ExternalForces> mExternalForces;

	private:
		SpecksRenderItem *mSpeckInstancesRenderItem;
		// Maximal number of render items to exist concurrently on the scene.
		const UINT mMaxRenderItemsCount;
		// Stack representing available free spaces in the render item buffer.
		std::vector<UINT> mFreeSpacesRenderItemBuffer;

		bool mFrustumCullingEnabled = true;
		PassConstants mMainPassCB;
	};
}

#endif
