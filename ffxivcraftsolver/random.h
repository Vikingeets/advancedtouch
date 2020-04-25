#pragma once

#include <cstdint>
#include <random>
#include <mutex>

// implemented from http://xoshiro.di.unimi.it/splitmix64.c
class splitmix64
{
public:
	using result_type = uint64_t;
private:
	result_type state;
public:
	static constexpr result_type min() { return 0; }
	static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

	splitmix64(result_type seed) :
		state(seed) {}
	splitmix64() :
		splitmix64(0) {}

	result_type operator()()
	{
		result_type z = (state += 0x9e3779b97f4a7c15);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
		z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
		return z ^ (z >> 31);
	}
};

// http://xoshiro.di.unimi.it/xoshiro256starstar.c
class xorshift
{
public:
	using result_type = uint64_t;
private:
	result_type state[4];

	inline result_type rotl(const result_type x, int k)
	{
		return (x << k) | (x >> (64 - k));
	}

public:
	static constexpr result_type min() { return 0; }
	static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

	void seed(result_type s)
	{
		splitmix64 sm64(s);
		state[0] = sm64();
		state[1] = sm64();
		state[2] = sm64();
		state[3] = sm64();
	}

	xorshift(result_type s)
	{
		seed(s);
	}
	xorshift() : xorshift(0) {}

	result_type operator()()
	{
		const result_type result = rotl(state[1] * 5, 7) * 9;
		const result_type t = state[1] << 17;

		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];

		state[2] ^= t;

		state[3] = rotl(state[3], 45);

		return result;
	}
};

class randomGenerator
{
private:
	static std::mutex devicelock;
	xorshift engine;

	template<typename T> 
	T uniformInt(T upper, T lower)
	{
		uint64_t high = upper - lower + 1;
		// From https://arxiv.org/abs/1805.10941
#if defined(__SIZEOF_INT128__)
		__uint128_t m = static_cast<__uint128_t>(engine()) * static_cast<__uint128_t>(high);
		uint64_t l = static_cast<uint64_t>(m);
		if (l < high)
		{
			uint64_t t = -high % high;
			while (l < t)
			{
				m = static_cast<__uint128_t>(engine()) * static_cast<__uint128_t>(high);
				l = static_cast<uint64_t>(m);
			}
		}
		return lower + (m >> 64);
#elif defined(_M_X64)
		uint64_t mHigh, mLow;
		mLow = _umul128(engine(), high, &mHigh);
		if (mLow < high)
		{
			uint64_t t = -high % high;
			while (mLow < t)
				mLow = _umul128(engine(), high, &mHigh);
		}
		return lower + mHigh;
#else
		return lower + std::uniform_int_distribution<T>(0, high - 1)(engine);
#endif
	}

	public:
	randomGenerator(const randomGenerator&) = delete;
	randomGenerator& operator=(const randomGenerator&) = delete;

	randomGenerator()
	{
		std::lock_guard<std::mutex> lock(devicelock);
		engine.seed(std::random_device()());
	}

	template<typename T>
	inline T generateInt(T low, T high)
	{
		assert(high >= low);
		T output = uniformInt<T>(low, high);

#if defined(_MSC_VER)
		__assume(output >= low);
		__assume(output <= high);
#endif

		return output;
	}

	template<typename T>
	inline T generateInt(T high)
	{
		return generateInt(static_cast<T>(0), high);
	}
};

