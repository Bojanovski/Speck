
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

	// World (or scene) containing everything visible.
	class SpeckWorld : public World
	{
		friend class SpeckApp;

	public:
		SpeckWorld(UINT maxInstancedObject, UINT maxSingleObjects);
		~SpeckWorld();
		SpeckWorld(const SpeckWorld& v) = delete;
		SpeckWorld& operator=(const SpeckWorld& v) = delete;
		PassConstants const *GetPassConstants() const { return &mMainPassCB; }
		virtual void Initialize(App* app) override;
		virtual void Update(App* app) override;
		virtual void PreDrawUpdate(App* app) override;
		virtual void Draw(App *app, UINT stage) override;

		SpecksRenderItem *GetSpeckInstancesRenderItem() { return mSpeckInstancesRenderItem; }
		UINT GetMaxSingleObjects() const { return mMaxSingleObjects; }
		SpecksHandler *GetSpecksHandler() { return mSpecksHandler.get(); }

	private:
		void Draw_Scene(App* app);				// used to draw scene normally

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
		const UINT mMaxSingleObjects;
		bool mFrustumCullingEnabled = true;
		PassConstants mMainPassCB;
	};
}

#endif
