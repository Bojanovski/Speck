
#ifndef SPECK_WORLD_H
#define SPECK_WORLD_H

#include "SpeckEngineDefinitions.h"
#include "World.h"
#include "FrameResource.h"

namespace Speck
{
	struct PSOGroup;
	struct FrameResource;
	struct PassConstants;
	class CubeRenderTarget;

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
		virtual void Update(App* app) override;
		virtual void Draw(App *app, UINT stage) override;

	private:
		void Draw_Scene(App* app);				// used to draw scene normally

	public:
		std::unordered_map<std::string, std::unique_ptr<PSOGroup>> mPSOGroups;
		std::unordered_map<std::string, std::unique_ptr<CubeRenderTarget>> mCubeRenderTarget;

	private:
		UINT mMaxInstancedObject;
		UINT mMaxSingleObjects;
		bool mFrustumCullingEnabled = true;
		PassConstants mMainPassCB;
	};
}

#endif
