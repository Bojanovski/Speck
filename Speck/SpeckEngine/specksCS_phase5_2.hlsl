
#include "specksCS_Root.hlsl"

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
	//[unroll(iterations)]
	for (uint k = 0; k < iterations; k++)
	{
		QR_Decomposition(Ak, Qk, QTk);
		//Qk = Identity3x3();
		//QTk = Identity3x3();
		eigenVectors = mul(eigenVectors, Qk); // is orthogonal
		Ak = mul(mul(QTk, Ak), Qk);
	}

	eigenValues = float3(Ak._m00, Ak._m11, Ak._m22);
}

float3x3 Get_Q_from_QS_decomposition(float3x3 A)
{
	float3x3 ATA = mul(transpose(A), A);

	float3 eigVal;
	float3x3 eigVec;
	GetEigendecomposition(ATA, QR_ALGORITHM_ITERATION_COUNT, eigVal, eigVec);

	float3x3 eigVecInv = transpose(eigVec);
	float3x3 lambdaSqrtInv = Identity3x3();
	lambdaSqrtInv._m00 = 1.0f / sqrt(eigVal[0]);
	lambdaSqrtInv._m11 = 1.0f / sqrt(eigVal[1]);
	lambdaSqrtInv._m22 = 1.0f / sqrt(eigVal[2]);
	float3x3 SInv = mul(mul(eigVec, lambdaSqrtInv), eigVecInv);
	float3x3 Q = mul(A, SInv);
	return Q;
}

// This phase is repeated so that full parallel reduction can be performed.
// This iteration calculates the A matrix of the speck and the last saves the world transform to the rigid body.
[numthreads(SPECK_RIGID_BODY_LINKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint linkIndex = dispatchThreadID.x;
	if (linkIndex >= gNumSpeckRigidBodyLinks)
		return; // early exit

	SpeckRigidBodyLink thisLink = gSpeckRigidBodyLinks[linkIndex];
	RigidBodyUploadData rbUploadData = gRigidBodyUploader[thisLink.rbIndex];
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
		float3 xi = thisLinkSpeck.pos_predicted;
		float3 ri = thisLink.posInRigidBody;
		A = GetOuterProduct(xi - c, ri);

		if (posInBlock == 0)
		{
			// If this is the first speck in the rigid body, add some virtual specks to prevent rank deficiency.
			float fac = 0.01f;
			A += fac * GetOuterProduct(normalize(gRigidBodies[thisLink.rbIndex].world[0].xyz), float3(1.0f, 0.0f, 0.0f));
			A += fac * GetOuterProduct(normalize(gRigidBodies[thisLink.rbIndex].world[1].xyz), float3(0.0f, 1.0f, 0.0f));
			A += fac * GetOuterProduct(normalize(gRigidBodies[thisLink.rbIndex].world[2].xyz), float3(0.0f, 0.0f, 1.0f));
		}

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

			if (rbUploadData.movementMode == RIGID_BODY_MOVEMENT_MODE_CPU)
				gRigidBodies[thisLink.rbIndex].world = rbUploadData.world;
			else // if (rbUploadData.movementMode == RIGID_BODY_MOVEMENT_MODE_GPU)
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
