//***************************************************************************************
// MathHelper.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "MathHelper.h"
#include <float.h>
#include <cmath>

using namespace std;
using namespace DirectX;
using namespace Speck;

const float MathHelper::Infinity = FLT_MAX;
const float MathHelper::Pi       = 3.1415926535f;

float MathHelper::AngleFromXY(float x, float y)
{
	float theta = 0.0f;
 
	// Quadrant I or IV
	if(x >= 0.0f) 
	{
		// If x = 0, then atanf(y/x) = +pi/2 if y > 0
		//                atanf(y/x) = -pi/2 if y < 0
		theta = atanf(y / x); // in [-pi/2, +pi/2]

		if(theta < 0.0f)
			theta += 2.0f*Pi; // in [0, 2*pi).
	}

	// Quadrant II or III
	else      
		theta = atanf(y/x) + Pi; // in [0, 2*pi).

	return theta;
}

XMVECTOR MathHelper::RandUnitVec3()
{
	XMVECTOR One  = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	// Keep trying until we get a point on/in the hemisphere.
	while(true)
	{
		// Generate random point in the cube [-1,1]^3.
		XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.

		if( XMVector3Greater( XMVector3LengthSq(v), One) )
			continue;

		return XMVector3Normalize(v);
	}
}

XMVECTOR MathHelper::RandHemisphereUnitVec3(XMVECTOR n)
{
	XMVECTOR One  = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	// Keep trying until we get a point on/in the hemisphere.
	while(true)
	{
		// Generate random point in the cube [-1,1]^3.
		XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.
		
		if( XMVector3Greater( XMVector3LengthSq(v), One) )
			continue;

		// Ignore points in the bottom hemisphere.
		if( XMVector3Less( XMVector3Dot(n, v), Zero ) )
			continue;

		return XMVector3Normalize(v);
	}
}

void MathHelper::QR_Decomposition3X3(CXMMATRIX A, XMMATRIX *Q, XMMATRIX *QT, XMMATRIX *R)
{
	// https://en.wikipedia.org/wiki/QR_decomposition
	// Householder implementation
	XMMATRIX AT = XMMatrixTranspose(A);
	XMVECTOR x = AT.r[0];
	float alpha = XMVectorGetX(XMVector3Length(x));
	XMVECTOR e1 = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR u = x - alpha*e1;
	XMVECTOR v = XMVector3Normalize(u);

	if (XMVectorGetX(XMVector3Length(u)) == 0.0f)
		v = XMVectorZero();


	XMMATRIX Q1 = XMMatrixIdentity() - 2.0f * GetOuterProduct3X3(v, v);
	XMMATRIX Q1T = XMMatrixTranspose(Q1);

	XMMATRIX Q1_mul_A = XMMatrixMultiply3X3(Q1, A);
	XMMATRIX A_11;
	A_11.r[0].m128_f32[0] = Q1_mul_A.r[1].m128_f32[1];
	A_11.r[0].m128_f32[1] = Q1_mul_A.r[1].m128_f32[2];
	A_11.r[1].m128_f32[0] = Q1_mul_A.r[2].m128_f32[1];
	A_11.r[1].m128_f32[1] = Q1_mul_A.r[2].m128_f32[2];
	XMMATRIX A_11T = XMMatrixTranspose(A_11);

	x = A_11T.r[0];
	alpha = XMVectorGetX(XMVector2Length(x));
	e1 = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	u = x - alpha*e1;
	v = XMVector2Normalize(u);


	if (XMVectorGetX(XMVector2Length(u)) == 0.0f)
		v = XMVectorZero();


	XMMATRIX Q2_small = XMMatrixIdentity() - 2.0f * GetOuterProduct2X2(v, v);
	XMMATRIX Q2 = XMMatrixIdentity();
	Q2.r[1].m128_f32[1] = Q2_small.r[0].m128_f32[0];
	Q2.r[1].m128_f32[2] = Q2_small.r[0].m128_f32[1];
	Q2.r[2].m128_f32[1] = Q2_small.r[1].m128_f32[0];
	Q2.r[2].m128_f32[2] = Q2_small.r[1].m128_f32[1];
	XMMATRIX Q2T = XMMatrixTranspose(Q2);

	*Q = XMMatrixMultiply3X3(Q1T, Q2T);
	*QT = XMMatrixMultiply3X3(Q2, Q1);
	*R = XMMatrixMultiply3X3(*QT, A);
}

void MathHelper::GetEigendecompositionSymmetric3X3(CXMMATRIX A, UINT iterations, XMVECTOR *eigenValues, XMMATRIX *eigenVectors)
{
	*eigenVectors = XMMatrixIdentity();
	XMMATRIX Qk;
	XMMATRIX QTk;
	XMMATRIX Rk;
	XMMATRIX Ak = A;
	for (UINT k = 0; k < iterations; k++)
	{
		MathHelper::QR_Decomposition3X3(Ak, &Qk, &QTk, &Rk);
		*eigenVectors *= Qk;
		Ak = XMMatrixMultiply3X3(XMMatrixMultiply3X3(QTk, Ak), Qk);
	}

	*eigenValues = XMVectorSet(
		Ak.r[0].m128_f32[0],
		Ak.r[1].m128_f32[1], 
		Ak.r[2].m128_f32[2], 
		0.0f);
}

XMVECTOR MathHelper::GetEigenvaluesSymmetric3X3(CXMMATRIX A)
{
	// From https://en.wikipedia.org/wiki/Eigenvalue_algorithm
	// Given a real symmetric 3x3 matrix A, compute the eigenvalues
	float p1 = A.r[0].m128_f32[1] * A.r[0].m128_f32[1] + A.r[0].m128_f32[2] * A.r[0].m128_f32[2] + A.r[1].m128_f32[2] * A.r[1].m128_f32[2];
	float eig1, eig2, eig3;
	if (p1 == 0.0f)
	{
		// A is diagonal.
		eig1 = A.r[0].m128_f32[0];
		eig2 = A.r[1].m128_f32[1];
		eig3 = A.r[2].m128_f32[2];
	}
	else
	{
		float q = GetTrace3X3(A) / 3.0f;
		float p2 = powf(A.r[0].m128_f32[0] - q, 2.0f) + powf(A.r[1].m128_f32[1] - q, 2.0f) + powf(A.r[2].m128_f32[2] - q, 2.0f) + 2.0f * p1;
		float p = sqrt(p2 / 6.0f);
		XMMATRIX B = (1.0f / p) * (A - q * XMMatrixIdentity()); // I is the identity matrix
		float r = GetDeterminant3X3(B) * 0.5f;

		// In exact arithmetic for a symmetric matrix - 1 <= r <= 1
		// but computation error can leave it slightly outside this range.
		float phi;
		if (r <= -1.0f)
			phi = XM_PI / 3.0f;
		else if (r >= 1.0f)
			phi = 0.0f;
		else
			phi = acos(r) / 3.0f;

		// the eigenvalues satisfy eig3 <= eig2 <= eig1
		eig1 = q + 2.0f * p * cos(phi);
		eig3 = q + 2.0f * p * cos(phi + (2.0f * XM_PI / 3.0f));
		eig2 = 3.0f * q - eig1 - eig3;    // since trace(A) = eig1 + eig2 + eig3
	}
	return XMVectorSet(eig1, eig2, eig3, 0.0f);
}

