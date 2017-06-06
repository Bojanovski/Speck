//***************************************************************************************
// MathHelper.h by Frank Luna (C) 2011 All Rights Reserved.
//
// Helper math class. Reworked for use in Speck engine.
//***************************************************************************************

#ifndef MATH_HELPER_H
#define MATH_HELPER_H

#include <Windows.h>
#include <DirectXMath.h>
#include <cstdint>

namespace Speck
{
	class MathHelper
	{
	public:
		// Returns random float in [0, 1).
		static float RandF()
		{
			return (float)(rand()) / (float)RAND_MAX;
		}

		// Returns random float in [a, b).
		static float RandF(float a, float b)
		{
			return a + RandF()*(b - a);
		}

		static int Rand(int a, int b)
		{
			return a + rand() % ((b - a) + 1);
		}

		template<typename T>
		static T Min(const T& a, const T& b)
		{
			return a < b ? a : b;
		}

		template<typename T>
		static T Max(const T& a, const T& b)
		{
			return a > b ? a : b;
		}

		template<typename T>
		static T Lerp(const T& a, const T& b, float t)
		{
			return a + (b - a)*t;
		}

		template<typename T>
		static T Clamp(const T& x, const T& low, const T& high)
		{
			return x < low ? low : (x > high ? high : x);
		}

		// Returns the polar angle of the point (x,y) in [0, 2*PI).
		static float AngleFromXY(float x, float y);

		static DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi)
		{
			return DirectX::XMVectorSet(
				radius*sinf(phi)*cosf(theta),
				radius*cosf(phi),
				radius*sinf(phi)*sinf(theta),
				1.0f);
		}

		static DirectX::XMMATRIX InverseTranspose(DirectX::CXMMATRIX M)
		{
			// Inverse-transpose is just applied to normals.  So zero out 
			// translation row so that it doesn't get into our inverse-transpose
			// calculation--we don't want the inverse-transpose of the translation.
			DirectX::XMMATRIX A = M;
			A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

			DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(A);
			return DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, A));
		}

		static DirectX::XMFLOAT4X4 Identity4x4()
		{
			static DirectX::XMFLOAT4X4 I(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f);

			return I;
		}

		static DirectX::XMVECTOR RandUnitVec3();
		static DirectX::XMVECTOR RandHemisphereUnitVec3(DirectX::XMVECTOR n);

		static DirectX::XMMATRIX GetOuterProduct3X3(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b);
		static DirectX::XMMATRIX GetOuterProduct2X2(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b);
		
		inline static DirectX::XMMATRIX XMMatrixMultiply3X3(DirectX::FXMMATRIX M1, DirectX::CXMMATRIX M2)
		{
			DirectX::XMMATRIX m1 = M1;
			// set last column and last row to null
			m1.r[0].m128_f32[3] = 0.0f;
			m1.r[1].m128_f32[3] = 0.0f;
			m1.r[2].m128_f32[3] = 0.0f;
			m1.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			DirectX::XMMATRIX m2 = M2;
			// set last column and last row to null
			m2.r[0].m128_f32[3] = 0.0f;
			m2.r[1].m128_f32[3] = 0.0f;
			m2.r[2].m128_f32[3] = 0.0f;
			m2.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			return DirectX::XMMatrixMultiply(m1, m2);
		}
		inline static float GetTrace3X3(DirectX::CXMMATRIX A)
		{
			return A.r[0].m128_f32[0] + A.r[1].m128_f32[1] + A.r[2].m128_f32[2];
		}
		inline static float GetDeterminant3X3(DirectX::CXMMATRIX A)
		{
			// From https://en.wikipedia.org/wiki/Determinant
			float a = A.r[0].m128_f32[0];
			float b = A.r[0].m128_f32[1];
			float c = A.r[0].m128_f32[2];
			float d = A.r[1].m128_f32[0];
			float e = A.r[1].m128_f32[1];
			float f = A.r[1].m128_f32[2];
			float g = A.r[2].m128_f32[0];
			float h = A.r[2].m128_f32[1];
			float i = A.r[2].m128_f32[2];
			return a*e*i + b*f*g + c*d*h - c*e*g - b*d*i - a*f*h;
		}

		static void QR_Decomposition3X3(DirectX::CXMMATRIX A, DirectX::XMMATRIX *Q, DirectX::XMMATRIX *QT, DirectX::XMMATRIX *R);
		// QR Algorithm, based on iterative QR decomposition.
		// Eigenvectors are proper only when matrix A is symmetric.
		static void GetEigendecompositionSymmetric3X3(DirectX::CXMMATRIX A, UINT iterations, DirectX::XMVECTOR *eigenValues, DirectX::XMMATRIX *eigenVectors);
		static DirectX::XMVECTOR GetEigenvaluesSymmetric3X3(DirectX::CXMMATRIX A);
		static DirectX::XMMATRIX GetInverse3X3(DirectX::CXMMATRIX M);

		static const float Infinity;
		static const float Pi;

		inline static float FixTimeStep(float timeStep)
		{
			return min(timeStep, 1.0f / 60.0f);
		}
	};
}

#endif
