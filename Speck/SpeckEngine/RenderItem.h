
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

	// Lightweight structure stores parameters to draw a shape.  This will
	// vary from app-to-app.
	struct RenderItem
	{
		RenderItem() = default;
		RenderItem(const RenderItem& rhs) = delete;
		virtual void UpdateBufferCPU(App *app, FrameResource *currentFrameResource) {}
		virtual void UpdateBufferGPU(App *app, FrameResource *currentFrameResource) {}
		virtual void Render(App *app, FrameResource* frameResource, const DirectX::BoundingFrustum &camFrustum) = 0;

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

	struct SingleRenderItem : public RenderItem
	{
		SingleRenderItem() = default;
		SingleRenderItem(const SingleRenderItem& rhs) = delete;
		void UpdateBufferCPU(App *app, FrameResource *currentFrameResource) override;
		void Render(App *app, FrameResource* frameResource, const DirectX::BoundingFrustum &camFrustum) override;

		// Dirty flag indicating the object data has changed and we need to update the constant buffer.
		// Because we have an object cbuffer for each FrameResource, we have to apply the
		// update to each FrameResource.  Thus, when we modify obect data we should set 
		// NumFramesDirty = NUM_FRAME_RESOURCES so that each frame resource gets the update.
		int mNumFramesDirty = NUM_FRAME_RESOURCES;

		Transform mT;
		DirectX::XMFLOAT4X4 mTexTransform = MathHelper::Identity4x4();
		DirectX::BoundingBox const *mBounds;

		// Material of this item
		PBRMaterial *mMat = nullptr;

		// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
		UINT mObjCBIndex = -1;
	};
}

#endif
