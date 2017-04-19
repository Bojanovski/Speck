
#ifndef RENDER_ITEM_H
#define RENDER_ITEM_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"
#include "GenericShaderStructures.h"
#include "FrameResource.h"

namespace Speck
{
	class App;

	// Lightweight structure stores parameters to draw a shape.  This will
	// vary from app-to-app.
	struct RenderItem
	{
		RenderItem() = default;
		RenderItem(const RenderItem& rhs) = delete;
		virtual void UpdateBuffer(FrameResource *currentFrameResource) = 0;
		virtual void Render(App *app, FrameResource* frameResource, DirectX::FXMMATRIX invView) = 0;

		// Dirty flag indicating the object data has changed and we need to update the constant buffer.
		// Because we have an object cbuffer for each FrameResource, we have to apply the
		// update to each FrameResource.  Thus, when we modify obect data we should set 
		// NumFramesDirty = NUM_FRAME_RESOURCES so that each frame resource gets the update.
		int mNumFramesDirty = NUM_FRAME_RESOURCES;

		MeshGeometry* mGeo = nullptr;

		// Primitive topology.
		D3D12_PRIMITIVE_TOPOLOGY mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		// DrawIndexedInstanced parameters.
		UINT mIndexCount = 0;
		UINT mInstanceCount = 0;
		UINT mStartIndexLocation = 0;
		int mBaseVertexLocation = 0;
	};

	struct InstancedRenderItem : public RenderItem
	{
		InstancedRenderItem() = default;
		InstancedRenderItem(const InstancedRenderItem& rhs) = delete;
		void UpdateBuffer(FrameResource *currentFrameResource) override;
		void Render(App *app, FrameResource* frameResource, DirectX::FXMMATRIX invView) override;

		std::vector<SpeckInstanceData> mInstances;
	};

	struct SingleRenderItem : public RenderItem
	{
		SingleRenderItem() = default;
		SingleRenderItem(const SingleRenderItem& rhs) = delete;
		void UpdateBuffer(FrameResource *currentFrameResource) override;
		void Render(App *app, FrameResource* frameResource, DirectX::FXMMATRIX invView) override;

		// World matrix of the shape that describes the object's local space
		// relative to the world space, which defines the position, orientation,
		// and scale of the object in the world.
		DirectX::XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 mTexTransform = MathHelper::Identity4x4();
		DirectX::BoundingBox mBounds;

		// Material of this item
		PBRMaterial *mMat = nullptr;

		// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
		UINT mObjCBIndex = -1;
	};
}

#endif
