
#ifndef RENDER_ITEM_H
#define RENDER_ITEM_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"
#include "GenericShaderStructures.h"
#include "FrameResource.h"
#include "Transform.h"

namespace Speck
{
	class App;
	class SpecksHandler;
	class DirectXCore;

	// Lightweight structure stores parameters to draw a shape.  This will
	// vary from app-to-app.
	struct RenderItem
	{
		RenderItem() = default;
		RenderItem(const RenderItem& rhs) = delete;
		virtual void UpdateBufferCPU(App *app, FrameResource *currentFrameResource) {}
		virtual void UpdateBufferGPU(App *app, FrameResource *currentFrameResource) {}
		virtual void Render(App *app, FrameResource* frameResource, const DirectX::BoundingFrustum &camFrustum) = 0;

	protected:
		DirectXCore &GetDirectXCore(App *app) const;// { return app->GetEngineCore().GetDirectXCore(); }

	public:
		MeshGeometry* mGeo = nullptr;

		// Primitive topology.
		D3D12_PRIMITIVE_TOPOLOGY mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		// DrawIndexedInstanced parameters.
		UINT mIndexCount = 0;
		UINT mInstanceCount = 0;
		UINT mStartIndexLocation = 0;
		int mBaseVertexLocation = 0;
	};

	struct SpecksRenderItem : public RenderItem
	{
		SpecksRenderItem() = default;
		SpecksRenderItem(const SpecksRenderItem& rhs) = delete;
		void Render(App *app, FrameResource* frameResource, const DirectX::BoundingFrustum &camFrustum) override;

		// Buffer in frame resource from where this instanced data will be drawn.
		UINT mBufferIndex;
	};

	struct TexturedRenderItem : public RenderItem
	{
		TexturedRenderItem() = default;
		TexturedRenderItem(const TexturedRenderItem& rhs) = delete;

		// Texture transform for this item
		DirectX::XMFLOAT4X4 mTexTransform = MathHelper::Identity4x4();
		// Material for this item
		PBRMaterial *mMat = nullptr;
	};


	struct StaticRenderItem : public TexturedRenderItem
	{
		StaticRenderItem() = default;
		StaticRenderItem(const StaticRenderItem& rhs) = delete;
		void UpdateBufferCPU(App *app, FrameResource *currentFrameResource) override;
		void Render(App *app, FrameResource* frameResource, const DirectX::BoundingFrustum &camFrustum) override;

		// Dirty flag indicating the object data has changed and we need to update the constant buffer.
		// Because we have an object cbuffer for each FrameResource, we have to apply the
		// update to each FrameResource.  Thus, when we modify obect data we should set 
		// NumFramesDirty = NUM_FRAME_RESOURCES so that each frame resource gets the update.
		int mNumFramesDirty = NUM_FRAME_RESOURCES;

		// World transform of the render item
		Transform mW;
		DirectX::BoundingBox const *mBounds;

		// Index into GPU constant buffer corresponding to the RenderItemConstants for this render item.
		UINT mRenderItemBufferIndex = -1;
	};

	// This body transformation will be determined by its respective speck rigid body.
	struct SpeckRigidBodyRenderItem : public TexturedRenderItem
	{
		SpeckRigidBodyRenderItem() = default;
		SpeckRigidBodyRenderItem(const StaticRenderItem& rhs) = delete;
		void UpdateBufferCPU(App *app, FrameResource *currentFrameResource) override;
		void Render(App *app, FrameResource* frameResource, const DirectX::BoundingFrustum &camFrustum) override;

		// Dirty flag indicating the object data has changed and we need to update the constant buffer.
		// Because we have an object cbuffer for each FrameResource, we have to apply the
		// update to each FrameResource.  Thus, when we modify obect data we should set 
		// NumFramesDirty = NUM_FRAME_RESOURCES so that each frame resource gets the update.
		int mNumFramesDirty = NUM_FRAME_RESOURCES;

		// Local transform of the render item.
		Transform mL;
		// Index to the speck rigid body buffer on the GPU.
		UINT mSpeckRigidBodyIndex = -1;

		// Index into GPU constant buffer corresponding to this render item.
		UINT mRenderItemBufferIndex = -1;
	};

	// This body bone transformations will be determined by its respective speck rigid bodies.
	struct SpeckSkeletalBodyRenderItem : public TexturedRenderItem
	{
		SpeckSkeletalBodyRenderItem() = default;
		SpeckSkeletalBodyRenderItem(const StaticRenderItem& rhs) = delete;
		void UpdateBufferCPU(App *app, FrameResource *currentFrameResource) override;
		void Render(App *app, FrameResource* frameResource, const DirectX::BoundingFrustum &camFrustum) override;

		// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
		//UINT mObjCBIndex = -1;
	};
}

#endif
