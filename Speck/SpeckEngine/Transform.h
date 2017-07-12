
#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "SpeckEngineDefinitions.h"

namespace Speck
{
	struct Transform
	{
		// Translation component of the transform object.
		DirectX::XMFLOAT3 mT;
		// Orientation component of the transform object.
		DirectX::XMFLOAT4 mR;
		// Scale component of the transform object.
		DirectX::XMFLOAT3 mS;

		// Constructs the matrix to transform from the local space to the world space.
		DLL_EXPORT DirectX::XMMATRIX GetWorldMatrix() const;

		// Constructs the transpose matrix to transform from the local space to the world space.
		DLL_EXPORT DirectX::XMMATRIX GetTransposeWorldMatrix() const;

		// Constructs the matrix to transform from the world to the item local space.
		DLL_EXPORT DirectX::XMMATRIX GetInverseWorldMatrix() const;

		// Initializes the values of the translation, rotation and the scale
		// components so that they construct an identity matrix.
		DLL_EXPORT void MakeIdentity();

		// Store the transformation matrix to destinaton.
		DLL_EXPORT void Store(DirectX::XMFLOAT4X4 *dest) const;

		// Store the transpose transformation matrix to destinaton.
		DLL_EXPORT void StoreTranspose(DirectX::XMFLOAT4X4 *dest) const;

		// Store the inverse transformation matrix to destinaton.
		DLL_EXPORT void StoreInverse(DirectX::XMFLOAT4X4 *dest) const;

		// Creates a transform with the translation, rotation and the scale
		// components set so that they construct an identity matrix.
		DLL_EXPORT static Transform Identity();
	};
}

#endif

