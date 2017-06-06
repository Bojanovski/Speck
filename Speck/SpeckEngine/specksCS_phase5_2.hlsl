
#include "specksCS_Root.hlsl"

// Safe normalization function.
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509630(v=vs.85).aspx
float3 NormalizeSafe(float3 vec)
{
	float len = length(vec);
	if (len == 0.0f)
	{
		return float3(0.0f, 0.0f, 0.0f);
	}
	else
	{
		return vec / len;
	}
}

float2 NormalizeSafe(float2 vec)
{
	float len = length(vec);
	if (len == 0.0f)
	{
		return float2(0.0f, 0.0f);
	}
	else
	{
		return vec / len;
	}
}

float3x3 GetOuterProduct(float3 a, float3 b)
{
	float3x3 ret;
	// first row
	ret._m00 = a.x * b.x;
	ret._m01 = a.x * b.y;
	ret._m02 = a.x * b.z;
	// second row
	ret._m10 = a.y * b.x;
	ret._m11 = a.y * b.y;
	ret._m12 = a.y * b.z;
	// third row
	ret._m20 = a.z * b.x;
	ret._m21 = a.z * b.y;
	ret._m22 = a.z * b.z;
	return ret;
}

float2x2 GetOuterProduct(float2 a, float2 b)
{
	float2x2 ret;
	// first row
	ret._m00 = a.x * b.x;
	ret._m01 = a.x * b.y;
	// second row
	ret._m10 = a.y * b.x;
	ret._m11 = a.y * b.y;
	return ret;
}

float3x3 Identity3x3()
{
	float3x3 ret;
	ret[0] = float3(1.0f, 0.0f, 0.0f);
	ret[1] = float3(0.0f, 1.0f, 0.0f);
	ret[2] = float3(0.0f, 0.0f, 1.0f);
	return ret;
}

float2x2 Identity2x2()
{
	float2x2 ret;
	ret[0] = float2(1.0f, 0.0f);
	ret[1] = float2(0.0f, 1.0f);
	return ret;
}

float3x3 Inverse(float3x3 M)
{
	// From https://en.wikipedia.org/wiki/Invertible_matrix#Inversion_of_3_.C3.97_3_matrices
	float a = M._m00;
	float b = M._m01;
	float c = M._m02;

	float d = M._m10;
	float e = M._m11;
	float f = M._m12;

	float g = M._m20;
	float h = M._m21;
	float i = M._m22;

	float A = (e*i - f*h);
	float D = -(b*i - c*h);
	float G = (b*f - c*e);

	float B = -(d*i - f*g);
	float E = (a*i - c*g);
	float H = -(a*f - c*d);

	float C = (d*h - e*g);
	float F = -(a*h - b*g);
	float I = (a*e - b*d);

	float detM = a*A + b*B + c*C;
	float invDetM = 1.0f / detM;
	float3x3 ret;
	ret[0] = invDetM * float3(A, D, G);
	ret[1] = invDetM * float3(B, E, H);
	ret[2] = invDetM * float3(C, F, I);
	return ret;
}

void QR_Decomposition(in float3x3 A, out float3x3 Q, out float3x3 QT)
{
	// https://en.wikipedia.org/wiki/QR_decomposition
	// Householder implementation
	float3x3 AT = transpose(A);
	float3 x3 = AT[0];
	float alpha = length(x3);
	float3 e3 = float3(1.0f, 0.0f, 0.0f);
	float3 u3 = x3 - alpha*e3;
	float3 v3 = NormalizeSafe(u3);
	float3x3 Q1 = Identity3x3() - 2.0f * GetOuterProduct(v3, v3);
	float3x3 Q1T = transpose(Q1);

	float3x3 Q1_mul_A = mul(Q1, A);
	float2x2 A_11; // minor of Q1_mul_A
	A_11._m00 = Q1_mul_A._m11;
	A_11._m01 = Q1_mul_A._m12;
	A_11._m10 = Q1_mul_A._m21;
	A_11._m11 = Q1_mul_A._m22;
	float2x2 A_11T = transpose(A_11);

	float2 x2 = A_11T[0];
	//float2 x2 = float2(A_11._m00, A_11._m10);
	alpha = length(x2);
	float2 e2 = float2(1.0f, 0.0f);
	float2 u2 = x2 - alpha*e2;
	float2 v2 = NormalizeSafe(u2);
	float2x2 Q2_small = Identity2x2() - 2.0f * GetOuterProduct(v2, v2);
	float3x3 Q2 = Identity3x3();
	Q2._m11 = Q2_small._m00;
	Q2._m12 = Q2_small._m01;
	Q2._m21 = Q2_small._m10;
	Q2._m22 = Q2_small._m11;
	float3x3 Q2T = transpose(Q2);

	Q = mul(Q1T, Q2T);
	QT = transpose(Q);
	//R = mul(QT, A);
}

// QR algorithm https://en.wikipedia.org/wiki/QR_algorithm
// A must be symmetric.
void GetEigendecomposition(in float3x3 A, in const uint iterations, out float3 eigenValues, out float3x3 eigenVectors)
{
	eigenVectors = Identity3x3();
	float3x3 Qk;
	float3x3 QTk;
	float3x3 Rk;
	float3x3 Ak = A;
	[unroll(iterations)]
	for (uint k = 0; k < iterations; k++)
	{
		QR_Decomposition(Ak, Qk, QTk);
		//Qk = Identity3x3();
		//QTk = Identity3x3();
		eigenVectors = mul(eigenVectors, Qk);
		Ak = mul(mul(QTk, Ak), Qk);
	}

	eigenValues = float3(Ak._m00, Ak._m11, Ak._m22);
}

float3x3 Get_Q_from_QS_decomposition(float3x3 A)
{
	float3x3 ATA = mul(transpose(A), A);

	float3 eigVal;
	float3x3 eigVec;
	GetEigendecomposition(ATA, 100, eigVal, eigVec);

	float3x3 eigVecInv = Inverse(eigVec);
	float3x3 lambdaSqrt = Identity3x3();
	lambdaSqrt._m00 = sqrt(eigVal[0]);
	lambdaSqrt._m11 = sqrt(eigVal[1]);
	lambdaSqrt._m22 = sqrt(eigVal[2]);

	float3x3 S = mul(mul(eigVec, lambdaSqrt), eigVecInv);
	float3x3 SInv = Inverse(S);
	float3x3 Q = mul(A, SInv);
	return Q;
}

// This phase is repeated so that full parallel reduction can be performed.
// First iteration calculates the A matrix of the speck and the last saves the world transform to the rigid body.
[numthreads(SPECK_RIGID_BODY_LINKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint linkIndex = dispatchThreadID.x;
	if (linkIndex >= gNumSpeckRigidBodyLinks)
		return; // early exit

	SpeckRigidBodyLink thisLink = gSpeckRigidBodyLinks[linkIndex];
	SpeckData thisLinkSpeck = gSpecks[thisLink.speckIndex];
	uint posInBlock = linkIndex - thisLink.speckLinksBlockStart;
	// Not all 'nodes' calculate the sum, only some of them.
	// useOnlyEvery denotes how many are skipped.
	uint useOnlyEvery = 1 << (gPhaseIteration);
	float3 c = gRigidBodies[thisLink.rbIndex].c;
	float3x3 A;
	if (gPhaseIteration == 0)
	{ 
		// first iteration, calculate A per link (particle)
		SpeckData thisLinkSpeck = gSpecks[thisLink.speckIndex];
		float3 xi = thisLinkSpeck.pos_predicted;
		float3 ri = thisLink.posInRigidBody;
		A = GetOuterProduct(xi - c, ri);
		// Save the data
		gSpeckRigidBodyLinkCache[linkIndex].A = A;
	}
	else if (posInBlock % useOnlyEvery == 0)
	{ 
		// rest of the iterations, summation
		A = gSpeckRigidBodyLinkCache[linkIndex].A;

		uint offset = 1 << (gPhaseIteration - 1);
		uint otherIndex = linkIndex + offset;
		if (otherIndex - thisLink.speckLinksBlockStart < thisLink.speckLinksBlockCount)
		{
			A += gSpeckRigidBodyLinkCache[otherIndex].A;
		}

		// Save the data
		gSpeckRigidBodyLinkCache[linkIndex].A = A;

		// If last iteration -> update the data in the rigid body structure
		if (gPhaseIteration == gNumPhaseIterations - 1)
		{
			float3x3 Q = Get_Q_from_QS_decomposition(A);
			float4x4 world;
			world[0] = float4(Q[0][0], Q[1][0], Q[2][0], 0.0f);
			world[1] = float4(Q[0][1], Q[1][1], Q[2][1], 0.0f);
			world[2] = float4(Q[0][2], Q[1][2], Q[2][2], 0.0f);
			world[3] = float4(c, 1.0f);
			gRigidBodies[thisLink.rbIndex].world = world;
		}
	}

	// If last iteration -> add the constraint
	if (gPhaseIteration == gNumPhaseIterations - 1)
	{
		// Multiple links can point to the same speck, so use atomics.
		uint posToWrite;
		InterlockedAdd(gSpecksConstraints[thisLink.speckIndex].numSpeckRigidBodies, 1, posToWrite);
		if (posToWrite < NUM_RIGID_BODY_CONSTRAINTS_PER_SPECK)
		{
			RigidBodyConstraint rbc;
			rbc.posInRigidBody = thisLink.posInRigidBody;
			// It would be great if we could put the transformation matrix directly in this structure,
			// but at this point it is calculated only in one thread which might not have executed yet.
			rbc.rigidBodyIndex = thisLink.rbIndex;
			gSpecksConstraints[thisLink.speckIndex].speckRigidBodyConstraint[posToWrite] = rbc;
		}
	}
}
