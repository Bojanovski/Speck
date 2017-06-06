
#include "specksCS_Root.hlsl"

float3 OrthogonalProjection(float3 vec, float3 n)
{
	return vec - dot(vec, n)*n;
}

void ProcessStaticColliders(
	in uint speckIndex, in SpeckData thisSpeck, in float dynamicFrictionMi, in float staticFrictionMi,
	out float3 totalDeltaP, out uint n)
{
	uint i;
	for (i = 0; i < gSpecksConstraints[speckIndex].numStaticCollider; ++i)
	{
		// interpenetration
		float w1 = thisSpeck.invMass;
		float w2 = 0.0f;
		float w = w1 + w2;
		float3 p1 = thisSpeck.pos_predicted;
		StaticColliderContactConstraint sccc = gSpecksConstraints[speckIndex].staticColliderContacts[i];
		float penetrationDepth = dot(p1 - sccc.pos, sccc.normal) - gSpeckRadius;
		// This is inequality constraint, so clamp every positive value of s to zero.
		if (penetrationDepth < 0.0f)
		{
			// penetration
			float s = (penetrationDepth) / w;
			float3 grad_p1_C = sccc.normal;
			float3 deltaP = -w1 * s * grad_p1_C;
			totalDeltaP += deltaP;
			++n;

			// friction
			float3 x1Vel = thisSpeck.pos_predicted - thisSpeck.pos;
			float3 tangentialVelocity = OrthogonalProjection(x1Vel, sccc.normal);
			float tvLen = length(tangentialVelocity);
			float miStatic_d = staticFrictionMi * penetrationDepth;
			float miDynamic_d = dynamicFrictionMi * penetrationDepth;
			deltaP = (w1 / w) * tangentialVelocity;
			if (tvLen >= miStatic_d && tvLen > 0.0f) deltaP *= min(1.0f, miDynamic_d / tvLen);
			totalDeltaP += deltaP;
			++n;
		}
	}
}

void ProcessNormalSpeck(
	in uint speckIndex, in SpeckData thisSpeck,
	out float3 totalDeltaP, out uint n)
{
	float doubleSpeckRadius = gSpeckRadius * 2.0f;
	float dynamicFrictionMi = thisSpeck.frictionCoefficient;
	float staticFrictionMi = 0.5f*(dynamicFrictionMi + 1.0f);
	uint thisSpeckUpperCode = thisSpeck.code & SPECK_CODE_UPPER_WORD_MASK;
	uint thisSpeckLowerCode = thisSpeck.code & SPECK_CODE_LOWER_WORD_MASK;
	totalDeltaP = float3(0.0f, 0.0f, 0.0f);
	n = 0;

	// Other specks
	uint i;
	for (i = 0; i < gSpecksConstraints[speckIndex].numSpeckContacts; ++i)
	{
		// interpenetration
		uint otherSpeckIndex = gSpecksConstraints[speckIndex].speckContacts[i].speckIndex;
		SpeckData otherSpeck = gSpecks[otherSpeckIndex];
		uint otherSpeckUpperCode = otherSpeck.code & SPECK_CODE_UPPER_WORD_MASK;
		float w1 = thisSpeck.invMass;
		float w2 = otherSpeck.invMass;
		float w = w1 + w2;
		float3 p1 = thisSpeck.pos_predicted;
		float3 p2 = otherSpeck.pos_predicted;
		float3 p21 = p1 - p2;
		float lenP21 = length(p21);
		float penetrationDepth = (lenP21 - doubleSpeckRadius);
		// This is inequality constraint, so clamp every positive value of s to zero.
		if (penetrationDepth < 0.0f)
		{
			// penetration
			float s = penetrationDepth / w;
			float3 grad_p1_C = (p21) / (lenP21);
			// Special case for grad_p1_C if other speck is part of the rigid body
			if (otherSpeckUpperCode == SPECK_CODE_RIGID_BODY)
			{
				float x = otherSpeck.param[0];
				float y = otherSpeck.param[1];
				float z = otherSpeck.param[2];
				float4 SDF_grad_localSpace = float4(x, y, z, 0.0f);
				float lenSq = dot(SDF_grad_localSpace, SDF_grad_localSpace);

				// Every rigid body this speck is part of will suffice
				uint rbIndex = gSpecksConstraints[otherSpeckIndex].speckRigidBodyConstraint[0].rigidBodyIndex;
				float4x4 rbWorld = gRigidBodies[rbIndex].world;
				float3 SDF_grad = mul(SDF_grad_localSpace, rbWorld).xyz;
				SDF_grad = normalize(SDF_grad);

				if (lenSq >= 2.0f) // this is a boundary speck
				{
					float sphereProj = dot(grad_p1_C, SDF_grad);
					if (sphereProj < 0.0f)
						grad_p1_C -= 2.0f * sphereProj * SDF_grad;
				}
				else // this is not a boundary speck
				{
					grad_p1_C = SDF_grad;
				}
			}

			float3 deltaP = -w1 * s * grad_p1_C;
			totalDeltaP += deltaP;
			++n;

			// friction
			float3 x1Vel = p1 - thisSpeck.pos;
			float3 x2Vel = p2 - otherSpeck.pos;
			float3 tangentialVelocity = OrthogonalProjection(x1Vel - x2Vel, grad_p1_C);
			float tvLen = length(tangentialVelocity);
			float miStatic_d = staticFrictionMi * penetrationDepth;
			float miDynamic_d = dynamicFrictionMi * penetrationDepth;
			deltaP = (w1 / w) * tangentialVelocity;
			if (tvLen >= miStatic_d && tvLen > 0.0f) deltaP *= min(1.0f, miDynamic_d / tvLen);
			totalDeltaP += deltaP;
			++n;
		}
	}

	// Static colliders
	float3 static_totalDeltaP = float3(0.0f, 0.0f, 0.0f);
	uint static_n = 0;
	ProcessStaticColliders(speckIndex, thisSpeck, dynamicFrictionMi, staticFrictionMi, static_totalDeltaP, static_n);
	totalDeltaP += static_totalDeltaP;
	n += static_n;
}

void ProcessFluidSpeck(
	in uint speckIndex, in SpeckData thisSpeck,
	out float3 totalDeltaP, out uint n)
{
	float doubleSpeckRadius = gSpeckRadius * 2.0f;
	float h = doubleSpeckRadius * COLLISION_DETECTION_MULTIPLIER; // for density kernels
	//float mass = 1.0f;
	float ro0 = (thisSpeck.mass) / (pow(gSpeckRadius, 3.0f)*PI*4.0f / 3.0f); // rest densitiy
	float invRo0 = 1.0f / ro0;
	float dynamicFrictionMi = thisSpeck.frictionCoefficient;
	float staticFrictionMi = 0.5f*(dynamicFrictionMi + 1.0f);
	uint thisSpeckUpperCode = thisSpeck.code & SPECK_CODE_UPPER_WORD_MASK;
	uint thisSpeckLowerCode = thisSpeck.code & SPECK_CODE_LOWER_WORD_MASK;
	totalDeltaP = float3(0.0f, 0.0f, 0.0f);
	n = 0;
	// Density velocity update should be calculated and applied only once and not for each particle like
	// friction and penetration update.
	float3 densityDeltaVel = float3(0.0f, 0.0f, 0.0f);

	// Other specks
	uint i;
	for (i = 0; i < gSpecksConstraints[speckIndex].numSpeckContacts; ++i)
	{
		uint otherSpeckIndex = gSpecksConstraints[speckIndex].speckContacts[i].speckIndex;
		SpeckData otherSpeck = gSpecks[otherSpeckIndex];
		uint otherSpeckUpperCode = otherSpeck.code & SPECK_CODE_UPPER_WORD_MASK;
		uint otherSpeckLowerCode = otherSpeck.code & SPECK_CODE_LOWER_WORD_MASK;
		float w1 = thisSpeck.invMass;
		float w2 = otherSpeck.invMass;
		float w = w1 + w2;
		float3 p1 = thisSpeck.pos_predicted;
		float3 p2 = otherSpeck.pos_predicted;
		float3 p21 = p1 - p2;
		float lenP21 = length(p21);
		float penetrationDepth = (lenP21 - doubleSpeckRadius);
		float3 x1Vel = p1 - thisSpeck.pos;
		float3 x2Vel = p2 - otherSpeck.pos;
		float3 grad_p1_C = (p21) / (lenP21);

		// Special case for grad_p1_C if other speck is part of the rigid body
		if (otherSpeckUpperCode == SPECK_CODE_RIGID_BODY)
		{
			float x = otherSpeck.param[0];
			float y = otherSpeck.param[1];
			float z = otherSpeck.param[2];
			float4 SDF_grad_localSpace = float4(x, y, z, 0.0f);
			float lenSq = dot(SDF_grad_localSpace, SDF_grad_localSpace);

			// Every rigid body this speck is part of will suffice
			uint rbIndex = gSpecksConstraints[otherSpeckIndex].speckRigidBodyConstraint[0].rigidBodyIndex;
			float4x4 rbWorld = gRigidBodies[rbIndex].world;
			float3 SDF_grad = mul(SDF_grad_localSpace, rbWorld).xyz;
			SDF_grad = normalize(SDF_grad);

			if (lenSq >= 2.0f) // this is a boundary speck
			{
				float sphereProj = dot(grad_p1_C, SDF_grad);
				if (sphereProj < 0.0f)
					grad_p1_C -= 2.0f * sphereProj * SDF_grad;
			}
			else // this is not a boundary speck
			{
				grad_p1_C = SDF_grad;
			}
		}

		float3 velAdd = float3(0.0f, 0.0f, 0.0f);
		// Tensile Instability solution from 
		// Position Based Fluids Miles Macklin and Matthias Muller whitepaper
		float deltaQ = 0.2f * h;
		float k = 0.1f;
		float nPow = 4;
		float sCorr = -k * pow(W_poly6(lenP21, h) / W_poly6(lenP21, h), nPow);

		float lambdaSum =
			(gSpecksConstraints[speckIndex].densityConstraintLambda +
				gSpecksConstraints[otherSpeckIndex].densityConstraintLambda) + sCorr;

		if (lambdaSum < 0.0f)
		{
			// pressure
			float3 acc = invRo0 * lambdaSum * otherSpeck.mass * W_spiky_d(lenP21, h) * grad_p1_C;
			velAdd += acc*gDeltaTime;
		}

		// Both specks are part of the same rigid body
		if (otherSpeckUpperCode == SPECK_CODE_FLUID &&
			thisSpeckLowerCode == otherSpeckLowerCode)
		{
			// cohesion
			float gamma = thisSpeck.param[0];
			float3 accCohesion = -gamma *  (w1 / w) * C_akinci(lenP21, h) * grad_p1_C;
			velAdd += gDeltaTime*accCohesion;
		}

		// viscosity
		float c = thisSpeck.param[1];
		float3 x1VelNew = x1Vel + velAdd;
		velAdd += gDeltaTime*c*(x2Vel - x1VelNew) * (w1 / w) * W_poly6(lenP21, h);

		densityDeltaVel += velAdd;
	}

	totalDeltaP += densityDeltaVel;
	++n;

	// Static colliders
	float3 static_totalDeltaP = float3(0.0f, 0.0f, 0.0f);
	uint static_n = 0;
	ProcessStaticColliders(speckIndex, thisSpeck, dynamicFrictionMi, staticFrictionMi, static_totalDeltaP, static_n);
	totalDeltaP += static_totalDeltaP;
	n += static_n;
}

void ProcessRigidBodySpeck(
	in uint speckIndex, in SpeckData thisSpeck,
	out float3 totalDeltaP, out uint n)
{
	float doubleSpeckRadius = gSpeckRadius * 2.0f;
	float dynamicFrictionMi = thisSpeck.frictionCoefficient;
	float staticFrictionMi = 0.5f*(dynamicFrictionMi + 1.0f);
	uint thisSpeckUpperCode = thisSpeck.code & SPECK_CODE_UPPER_WORD_MASK;
	uint thisSpeckLowerCode = thisSpeck.code & SPECK_CODE_LOWER_WORD_MASK;
	totalDeltaP = float3(0.0f, 0.0f, 0.0f);
	n = 0;

	// Other specks
	uint i;
	for (i = 0; i < gSpecksConstraints[speckIndex].numSpeckContacts; ++i)
	{
		// interpenetration
		uint otherSpeckIndex = gSpecksConstraints[speckIndex].speckContacts[i].speckIndex;
		SpeckData otherSpeck = gSpecks[otherSpeckIndex];
		uint otherSpeckUpperCode = otherSpeck.code & SPECK_CODE_UPPER_WORD_MASK;
		uint otherSpeckLowerCode = otherSpeck.code & SPECK_CODE_LOWER_WORD_MASK;
		float w1 = thisSpeck.invMass;
		float w2 = otherSpeck.invMass;
		float w = w1 + w2;
		float3 p1 = thisSpeck.pos_predicted;
		float3 p2 = otherSpeck.pos_predicted;
		float3 p21 = p1 - p2;
		float lenP21 = length(p21);
		float penetrationDepth = (lenP21 - doubleSpeckRadius);
		// This is inequality constraint, so clamp every positive value of s to zero.
		if (penetrationDepth < 0.0f)
		{
			// Both specks are part of the same rigid body
			if (otherSpeckUpperCode == SPECK_CODE_RIGID_BODY &&
				thisSpeckLowerCode == otherSpeckLowerCode)
			{
				// penetration
				float s = penetrationDepth / w;
				float3 grad_p1_C = (p21) / (lenP21);
				float3 deltaP = -w1 * s * grad_p1_C;
				totalDeltaP += deltaP;
				++n;
			}
			else // Not part of the same rigid body.
			{
				// penetration
				float s = penetrationDepth / w;
				float3 grad_p1_C = (p21) / (lenP21);
				// Special case for grad_p1_C if other speck is part of the rigid body
				if (otherSpeckUpperCode == SPECK_CODE_RIGID_BODY)
				{
					float x = otherSpeck.param[0];
					float y = otherSpeck.param[1];
					float z = otherSpeck.param[2];
					float4 SDF_grad_localSpace = float4(x, y, z, 0.0f);
					float lenSq = dot(SDF_grad_localSpace, SDF_grad_localSpace);

					// Every rigid body this speck is part of will suffice
					uint rbIndex = gSpecksConstraints[otherSpeckIndex].speckRigidBodyConstraint[0].rigidBodyIndex;
					float4x4 rbWorld = gRigidBodies[rbIndex].world;
					float3 SDF_grad = mul(SDF_grad_localSpace, rbWorld).xyz;
					SDF_grad = normalize(SDF_grad);

					if (lenSq >= 2.0f) // this is a boundary speck
					{
						float sphereProj = dot(grad_p1_C, SDF_grad);
						if (sphereProj < 0.0f)
							grad_p1_C -= 2.0f * sphereProj * SDF_grad;
					}
					else // this is not a boundary speck
					{
						grad_p1_C = SDF_grad;
					}
				}

				float3 deltaP = -w1 * s * grad_p1_C;
				totalDeltaP += deltaP;
				++n;

				// friction
				float3 x1Vel = p1 - thisSpeck.pos;
				float3 x2Vel = p2 - otherSpeck.pos;
				float3 tangentialVelocity = OrthogonalProjection(x1Vel - x2Vel, grad_p1_C);
				float tvLen = length(tangentialVelocity);
				float miStatic_d = staticFrictionMi * penetrationDepth;
				float miDynamic_d = dynamicFrictionMi * penetrationDepth;
				deltaP = (w1 / w) * tangentialVelocity;
				if (tvLen >= miStatic_d && tvLen > 0.0f) deltaP *= min(1.0f, miDynamic_d / tvLen);
				totalDeltaP += deltaP;
				++n;
			}
		}
	}

	// Static colliders
	float3 static_totalDeltaP = float3(0.0f, 0.0f, 0.0f);
	uint static_n = 0;
	ProcessStaticColliders(speckIndex, thisSpeck, dynamicFrictionMi, staticFrictionMi, static_totalDeltaP, static_n);
	totalDeltaP += static_totalDeltaP;
	n += static_n;
}

// All constraints resolution used for solver iterations.
// Rigid body constraints are handled differently in the last resolution phase.
[numthreads(SPECKS_CS_N_THREADS, 1, 1)]
void main(int3 threadGroupID : SV_GroupID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	uint speckIndex = dispatchThreadID.x;
	if (speckIndex >= gParticleNum)
		return; // early exit

	uint phaseType = gPhaseIteration % 2;
	SpeckData thisSpeck = gSpecks[speckIndex];

	if (gPhaseIteration % 2 == 0) // compute delta pos
	{
		uint thisSpeckUpperCode = thisSpeck.code & SPECK_CODE_UPPER_WORD_MASK;
		float3 totalDeltaP = float3(0.0f, 0.0f, 0.0f);
		uint n = 0;

		switch (thisSpeckUpperCode)
		{
			case SPECK_CODE_NORMAL:
				ProcessNormalSpeck(speckIndex, thisSpeck, totalDeltaP, n);
				break;
			case SPECK_CODE_FLUID:
				ProcessFluidSpeck(speckIndex, thisSpeck, totalDeltaP, n);
				break;
			case SPECK_CODE_RIGID_BODY:
				ProcessRigidBodySpeck(speckIndex, thisSpeck, totalDeltaP, n);
				break;
		}

		gSpecksConstraints[speckIndex].appliedDeltaPos = totalDeltaP;
		gSpecksConstraints[speckIndex].n = n;
	}
	else // apply delta pos
	{
		float3 totalDeltaP = gSpecksConstraints[speckIndex].appliedDeltaPos;
		uint n = gSpecksConstraints[speckIndex].n;
		if (n > 0)
		{
			float3 appliedDeltaP = totalDeltaP / n;
			thisSpeck.pos_predicted += appliedDeltaP;
		}
		gSpecks[speckIndex] = thisSpeck;
	}
}
