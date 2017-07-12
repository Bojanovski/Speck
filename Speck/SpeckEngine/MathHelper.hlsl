
#ifndef MATH_HELPER_TXT
#define MATH_HELPER_TXT

// Safe normalization function.
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509630(v=vs.85).aspx
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

float2x2 Identity2x2()
{
	float2x2 ret;
	ret[0] = float2(1.0f, 0.0f);
	ret[1] = float2(0.0f, 1.0f);
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

float4x4 Identity4x4()
{
	float4x4 ret;
	ret[0] = float4(1.0f, 0.0f, 0.0f, 0.0f);
	ret[1] = float4(0.0f, 1.0f, 0.0f, 0.0f);
	ret[2] = float4(0.0f, 0.0f, 1.0f, 0.0f);
	ret[3] = float4(0.0f, 0.0f, 0.0f, 1.0f);
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

#endif
