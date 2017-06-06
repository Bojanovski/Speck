
#ifndef RANDOM_GENERATOR_H
#define RANDOM_GENERATOR_H

#include "SpeckEngineDefinitions.h"
#include <random>

#undef max

namespace Speck
{
	class RandomGenerator
	{
	public:
		RandomGenerator(int seed = 0)
			: mState(seed)
		{

		}

		bool BernoulliTest(float p = 0.5f)
		{
			return ((mRndReal(mState) / mRndReal.max()) < p);
		}

		int GetInt(int min = 0, int max = INT_MAX)
		{
			int diff = max - min + 1;
			return (mRndInteger(mState) % diff) + min;
		}

		float GetReal(float min = 0.0f, float max = 1.0f)
		{
			float diff = max - min;
			return ((mRndReal(mState) / mRndReal.max()) * diff) + min;
		}

	private:
		std::default_random_engine mState; // random engine generator state
		std::uniform_int_distribution<int> mRndInteger;
		std::uniform_real_distribution<float> mRndReal;
	};
}

#endif
