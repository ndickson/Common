#pragma once

#include "Types.h"
#include <stdint.h>

OUTER_NAMESPACE_START
COMMON_LIBRARY_NAMESPACE_START

constexpr INLINE uint64 rotateLeft(const uint64 x, const int bits) {
	// Hopefully the compiler realizes that there's a single x86-64 instruction for this.
	return (x << bits) | (x >> (64-bits));
}

// This is based on the 2015 splitmix64 random number generator,
// from xoshiro.di.unimi.it, which has been released into the public domain,
// for use in seeding other random number generators.
// All 2^64 values are valid seeds for splitmix64, unlike most other
// generators, which need to avoid zero.
constexpr INLINE uint64 splitMix64(uint64& state) {
	state += 0x9e3779b97f4a7c15;
	uint64 z = state;
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

// This is based on the 2018 xoroshiro128+ 1.0 random number generator
// from xoshiro.di.unimi.it, which has been released into the public domain.
struct Random128 {
	uint64 state[2];

	INLINE Random128() {}
	constexpr INLINE Random128(const Random128& that) = default;
	constexpr INLINE Random128(Random128&& that) = default;

	// Initialize from a single seed.
	constexpr INLINE Random128(uint64 seed) : state{0,0} {
		reseed(seed);
	}

	// This reseeds in a way that should be consistent with the constructor.
	constexpr void reseed(uint64 seed) {
		// NOTE: splitMix64 doesn't generate the same number twice in a row,
		// so can't generate the dangerous zero state.
		state[0] = splitMix64(seed);
		state[1] = splitMix64(seed);
		// One extra step forward, just in case.
		next();
	}

	constexpr INLINE Random128& operator=(const Random128& that) = default;
	constexpr INLINE Random128& operator=(Random128&& that) = default;

	constexpr INLINE uint64 next() {
		const uint64 s0 = state[0];
		uint64 s1 = state[1];
		const uint64 result = s0 + s1;

		s1 ^= s0;
		state[0] = rotateLeft(s0, 24) ^ s1 ^ (s1 << 16);
		state[1] = rotateLeft(s1, 37);

		return result;
	}

	// This is the jump function for the generator. It is equivalent
	// to 2^64 calls to next(); it can be used to generate 2^64
	// non-overlapping subsequences for parallel computations.
	constexpr void jump() {
		constexpr int JUMP_LENGTH = 2;
		constexpr uint64 JUMP[JUMP_LENGTH] = {
			0xdf900294d8f554a5,
			0x170865df4b3201fc
		};

		uint64 s0 = 0;
		uint64 s1 = 0;
		for (int i = 0; i < JUMP_LENGTH; ++i) {
			for (int bit = 0; bit < 64; ++bit) {
				if (JUMP[i] & (UINT64_C(1) << bit)) {
					s0 ^= state[0];
					s1 ^= state[1];
				}
				next();
			}
		}
		state[0] = s0;
		state[1] = s1;
	}

	// This is the long-jump function for the generator. It is equivalent to
	// 2^96 calls to next(); it can be used to generate 2^32 starting points,
	// from each of which jump() will generate 2^32 non-overlapping
	// subsequences for parallel distributed computations.
	constexpr void longJump() {
		constexpr int JUMP_LENGTH = 2;
		constexpr uint64 JUMP[JUMP_LENGTH] = {
			0xd2a98b26625eee7b,
			0xdddf9b1090aa7ac1
		};

		uint64 s0 = 0;
		uint64 s1 = 0;
		for (int i = 0; i < JUMP_LENGTH; ++i) {
			for (int bit = 0; bit < 64; ++bit) {
				if (JUMP[i] & (UINT64_C(1) << bit)) {
					s0 ^= state[0];
					s1 ^= state[1];
				}
				next();
			}
		}
		state[0] = s0;
		state[1] = s1;
	}
};

// This is based on the 2018 xoshiro256** 1.0 random number generator
// from xoshiro.di.unimi.it, which has been released into the public domain.
struct Random256 {
	uint64 state[4];

	INLINE Random256() {}
	constexpr INLINE Random256(const Random256& that) = default;
	constexpr INLINE Random256(Random256&& that) = default;

	// Initialize from a single seed.
	constexpr INLINE Random256(uint64 seed) : state{0,0,0,0} {
		reseed(seed);
	}

	// This reseeds in a way that should be consistent with the constructor.
	constexpr void reseed(uint64 seed) {
		// NOTE: splitMix64 doesn't generate the same number twice in a row,
		// so can't generate the dangerous zero state.
		state[0] = splitMix64(seed);
		state[1] = splitMix64(seed);
		state[2] = splitMix64(seed);
		state[3] = splitMix64(seed);
		// One extra step forward, just in case.
		next();
	}

	constexpr INLINE Random256& operator=(const Random256& that) = default;
	constexpr INLINE Random256& operator=(Random256&& that) = default;

	constexpr INLINE uint64 next() {
		const uint64 result_starstar = rotateLeft(state[1] * 5, 7) * 9;
		const uint64 t = state[1] << 17;

		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];

		state[2] ^= t;

		state[3] = rotateLeft(state[3], 45);

		return result_starstar;
	}

	// This is the jump function for the generator. It is equivalent
	// to 2^128 calls to next(); it can be used to generate 2^128
	// non-overlapping subsequences for parallel computations.
	constexpr void jump() {
		constexpr int JUMP_LENGTH = 4;
		constexpr uint64 JUMP[JUMP_LENGTH] = {
			0x180ec6d33cfd0aba,
			0xd5a61266f0c9392c,
			0xa9582618e03fc9aa,
			0x39abdc4529b1661c
		};

		uint64 s0 = 0;
		uint64 s1 = 0;
		uint64 s2 = 0;
		uint64 s3 = 0;
		for (int i = 0; i < JUMP_LENGTH; ++i) {
			for (int bit = 0; bit < 64; ++bit) {
				if (JUMP[i] & (UINT64_C(1) << bit)) {
					s0 ^= state[0];
					s1 ^= state[1];
					s2 ^= state[2];
					s3 ^= state[3];
				}
				next();
			}
		}
		state[0] = s0;
		state[1] = s1;
		state[2] = s2;
		state[3] = s3;
	}

	// This is the long-jump function for the generator. It is equivalent to
	// 2^192 calls to next(); it can be used to generate 2^64 starting points,
	// from each of which jump() will generate 2^64 non-overlapping
	// subsequences for parallel distributed computations.
	constexpr void longJump() {
		constexpr int JUMP_LENGTH = 4;
		constexpr uint64 JUMP[JUMP_LENGTH] = {
			0x76e15d3efefdcbbf,
			0xc5004e441c522fb3,
			0x77710069854ee241,
			0x39109bb02acbe635
		};

		uint64 s0 = 0;
		uint64 s1 = 0;
		uint64 s2 = 0;
		uint64 s3 = 0;
		for (int i = 0; i < JUMP_LENGTH; ++i) {
			for (int bit = 0; bit < 64; ++bit) {
				if (JUMP[i] & (UINT64_C(1) << bit)) {
					s0 ^= state[0];
					s1 ^= state[1];
					s2 ^= state[2];
					s3 ^= state[3];
				}
				next();
			}
		}
		state[0] = s0;
		state[1] = s1;
		state[2] = s2;
		state[3] = s3;
	}
};

template<typename RNG>
constexpr INLINE void random(RNG& rng, double& result) {
	uint64 i = rng.next();
	// FIXME: This can result in the value 0 and the value 1.
	result = double(i) * (1.0 / (double(1ULL<<32)*double(1ULL<<32)));
}
template<typename RNG>
constexpr INLINE void random(RNG& rng, float& result) {
	uint64 i = rng.next();
	// FIXME: This can result in the value 0 and the value 1.
	result = float(i) * (1.0f / (float(1ULL<<32)*float(1ULL<<32)));
}
template<typename RNG>
constexpr INLINE void random(RNG& rng, uint64& result) {
	result = rng.next();
}
template<typename RNG>
constexpr INLINE void random(RNG& rng, uint32& result) {
	uint64 i = rng.next();
	result = uint32(i);
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
