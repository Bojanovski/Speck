//***************************************************************************************
// GeometryGenerator.h by Frank Luna (C) 2011 All Rights Reserved.
//   
// Defines a static class for procedurally generating the geometry of 
// common mathematical objects.
//
// All triangles are generated "outward" facing.  If you want "inward" 
// facing triangles (for example, if you want to place the camera inside
// a sphere to simulate a sky), you will need to:
//   1. Change the Direct3D cull mode or manually reverse the winding order.
//   2. Invert the normal.
//   3. Update the texture coordinates and tangent vectors.
//***************************************************************************************

#ifndef GEOMETRY_GENERATOR_H
#define GEOMETRY_GENERATOR_H

#include "SpeckEngineDefinitions.h"
#include "DirectXHeaders.h"

namespace Speck
{
	class GeometryGenerator
	{
	public:

		using uint16 = std::uint16_t;
		using uint32 = std::uint32_t;

		struct StaticVertex
		{
			StaticVertex() {}
			StaticVertex(
				const DirectX::XMFLOAT3& p,
				const DirectX::XMFLOAT3& n,
				const DirectX::XMFLOAT3& t,
				const DirectX::XMFLOAT2& uv) :
				Position(p),
				Normal(n),
				TangentU(t),
				TexC(uv) 
			{}
			StaticVertex(
				float px, float py, float pz,
				float nx, float ny, float nz,
				float tx, float ty, float tz,
				float u, float v) :
				Position(px, py, pz),
				Normal(nx, ny, nz),
				TangentU(tx, ty, tz),
				TexC(u, v) {}

			DirectX::XMFLOAT3 Position;
			DirectX::XMFLOAT3 Normal;
			DirectX::XMFLOAT3 TangentU;
			DirectX::XMFLOAT2 TexC;
			
			static void GetInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC> *inputLayout);
		};

		struct SkinnedVertex
		{
			SkinnedVertex() {}
			SkinnedVertex(
				const DirectX::XMFLOAT4 p[MAX_BONES_PER_VERTEX],
				const DirectX::XMFLOAT3 n[MAX_BONES_PER_VERTEX],
				const DirectX::XMFLOAT3 t[MAX_BONES_PER_VERTEX],
				const DirectX::XMFLOAT2& uv,
				const byte bi[])
				: TexC(uv)
			{
				memcpy(Position, p, sizeof(Position));
				memcpy(Normal, n, sizeof(Normal));
				memcpy(TangentU, t, sizeof(TangentU));
				memcpy(BoneIndices, bi, sizeof(BoneIndices));
			}

			DirectX::XMFLOAT4 Position[MAX_BONES_PER_VERTEX];
			DirectX::XMFLOAT3 Normal[MAX_BONES_PER_VERTEX];
			DirectX::XMFLOAT3 TangentU[MAX_BONES_PER_VERTEX];
			DirectX::XMFLOAT2 TexC;
			int BoneIndices[MAX_BONES_PER_VERTEX];
			
			static void GetInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC> *inputLayout);
		};

		struct StaticMeshData
		{
			std::vector<StaticVertex> Vertices;
			std::vector<uint32> Indices32;

			std::vector<uint16>& GetIndices16()
			{
				if (mIndices16.empty())
				{
					mIndices16.resize(Indices32.size());
					for (size_t i = 0; i < Indices32.size(); ++i)
						mIndices16[i] = static_cast<uint16>(Indices32[i]);
				}

				return mIndices16;
			}

			DirectX::BoundingBox CalculateBounds();

		private:
			std::vector<uint16> mIndices16;
		};

		struct SkinnedMeshData
		{
			std::vector<SkinnedVertex> Vertices;
			std::vector<uint32> Indices32;

			std::vector<uint16>& GetIndices16()
			{
				if (mIndices16.empty())
				{
					mIndices16.resize(Indices32.size());
					for (size_t i = 0; i < Indices32.size(); ++i)
						mIndices16[i] = static_cast<uint16>(Indices32[i]);
				}

				return mIndices16;
			}

		private:
			std::vector<uint16> mIndices16;
		};

		///<summary>
		/// Creates a box centered at the origin with the given dimensions, where each
		/// face has m rows and n columns of vertices.
		///</summary>
		StaticMeshData CreateBox(float width, float height, float depth, uint32 numSubdivisions);

		///<summary>
		/// Creates a sphere centered at the origin with the given radius.  The
		/// slices and stacks parameters control the degree of tessellation.
		///</summary>
		StaticMeshData CreateSphere(float radius, uint32 sliceCount, uint32 stackCount);

		///<summary>
		/// Creates a geosphere centered at the origin with the given radius.  The
		/// depth controls the level of tessellation.
		///</summary>
		StaticMeshData CreateGeosphere(float radius, uint32 numSubdivisions);

		///<summary>
		/// Creates a cylinder parallel to the y-axis, and centered about the origin.  
		/// The bottom and top radius can vary to form various cone shapes rather than true
		// cylinders.  The slices and stacks parameters control the degree of tessellation.
		///</summary>
		StaticMeshData CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);

		///<summary>
		/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
		/// at the origin with the specified width and depth.
		///</summary>
		StaticMeshData CreateGrid(float width, float depth, uint32 m, uint32 n);

		///<summary>
		/// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
		///</summary>
		StaticMeshData CreateQuad(float x, float y, float w, float h, float depth);

	private:
		void Subdivide(StaticMeshData& meshData);
		StaticVertex MidPoint(const StaticVertex& v0, const StaticVertex& v1);
		void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, StaticMeshData& meshData);
		void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, StaticMeshData& meshData);
	};
}

#endif
