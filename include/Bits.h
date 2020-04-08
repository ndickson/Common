#pragma once

// This file defines some simple bit manipulation functions.

#include "Types.h"

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

[[nodiscard]] constexpr INLINE uint64 rotateLeft(const uint64 x, const int bits) {
	// Hopefully the compiler realizes that there's a single x86-64 instruction for this.
	return (x << bits) | (x >> (64-bits));
}
[[nodiscard]] constexpr INLINE uint64 rotateRight(const uint64 x, const int bits) {
	// Hopefully the compiler realizes that there's a single x86-64 instruction for this.
	return (x >> bits) | (x << (64-bits));
}
[[nodiscard]] constexpr INLINE uint32 rotateLeft(const uint32 x, const int bits) {
	// Hopefully the compiler realizes that there's a single x86-64 instruction for this.
	return (x << bits) | (x >> (32-bits));
}
[[nodiscard]] constexpr INLINE uint32 rotateRight(const uint32 x, const int bits) {
	// Hopefully the compiler realizes that there's a single x86-64 instruction for this.
	return (x >> bits) | (x << (32-bits));
}
[[nodiscard]] constexpr INLINE uint16 rotateLeft(const uint16 x, const int bits) {
	// Hopefully the compiler realizes that there's a single x86-64 instruction for this.
	return (x << bits) | (x >> (16-bits));
}
[[nodiscard]] constexpr INLINE uint16 rotateRight(const uint16 x, const int bits) {
	// Hopefully the compiler realizes that there's a single x86-64 instruction for this.
	return (x >> bits) | (x << (16-bits));
}
[[nodiscard]] constexpr INLINE uint8 rotateLeft(const uint8 x, const int bits) {
	// Hopefully the compiler realizes that there's a single x86-64 instruction for this.
	return (x << bits) | (x >> (8-bits));
}
[[nodiscard]] constexpr INLINE uint8 rotateRight(const uint8 x, const int bits) {
	// Hopefully the compiler realizes that there's a single x86-64 instruction for this.
	return (x >> bits) | (x << (8-bits));
}

[[nodiscard]] constexpr inline uint32 bitScanF64(uint64 v) {
	// NOTE: This doesn't use _BitScanForward64 (Visual Studio) or __builtin_ctzll (GCC/Clang)
	//       because _BitScanForward64 can't be called from constexpr code.
	if (v == 0) {
		return uint32(64);
	}
	bool has_32 = (uint32(v) != 0);
	uint32 count = uint32(!has_32)<<5;
	v = has_32 ? v : (v>>32);
	bool has_16 = ((v&0xFFFF) != 0);
	count += uint32(!has_16)<<4;
	v = has_16 ? v : (v>>16);
	bool has_8 = ((v&0xFF) != 0);
	count += uint32(!has_8)<<3;
	v = has_8 ? v : (v>>8);
	bool has_4 = ((v&0xF) != 0);
	count += uint32(!has_4)<<2;
	v = has_4 ? v : (v>>4);
	bool has_2 = ((v&0x3) != 0);
	count += uint32(!has_2)<<1;
	v = has_2 ? v : (v>>2);
	count += !(v&1);
	return count;
}

[[nodiscard]] constexpr inline uint32 bitScanF32(uint32 v) {
	// NOTE: This doesn't use _BitScanForward (Visual Studio) or __builtin_ctz (GCC/Clang)
	//       because _BitScanForward can't be called from constexpr code.
	if (v == 0) {
		return uint32(32);
	}
	bool has_16 = ((v&0xFFFF) != 0);
	uint32 count = uint32(!has_16)<<4;
	v = has_16 ? v : (v>>16);
	bool has_8 = ((v&0xFF) != 0);
	count += uint32(!has_8)<<3;
	v = has_8 ? v : (v>>8);
	bool has_4 = ((v&0xF) != 0);
	count += uint32(!has_4)<<2;
	v = has_4 ? v : (v>>4);
	bool has_2 = ((v&0x3) != 0);
	count += uint32(!has_2)<<1;
	v = has_2 ? v : (v>>2);
	count += !(v&1);
	return count;
}

[[nodiscard]] constexpr inline uint32 bitScanF32(uint16 v) {
	// NOTE: This doesn't use _BitScanForward (Visual Studio) or __builtin_ctz (GCC/Clang)
	//       because _BitScanForward can't be called from constexpr code.
	if (v == 0) {
		return uint32(32);
	}
	bool has_8 = ((v&0xFF) != 0);
	uint32 count = uint32(!has_8)<<3;
	v = has_8 ? v : (v>>8);
	bool has_4 = ((v&0xF) != 0);
	count += uint32(!has_4)<<2;
	v = has_4 ? v : (v>>4);
	bool has_2 = ((v&0x3) != 0);
	count += uint32(!has_2)<<1;
	v = has_2 ? v : (v>>2);
	count += !(v&1);
	return count;
}

[[nodiscard]] constexpr inline uint32 bitScanF32(uint8 v) {
	// NOTE: This doesn't use _BitScanForward (Visual Studio) or __builtin_ctz (GCC/Clang)
	//       because _BitScanForward can't be called from constexpr code.
	if (v == 0) {
		return uint32(32);
	}
	bool has_4 = ((v&0xF) != 0);
	uint32 count = uint32(!has_4)<<2;
	v = has_4 ? v : (v>>4);
	bool has_2 = ((v&0x3) != 0);
	count += uint32(!has_2)<<1;
	v = has_2 ? v : (v>>2);
	count += !(v&1);
	return count;
}

[[nodiscard]] constexpr inline uint32 bitScanR64(uint64 v) {
	// NOTE: This doesn't use _BitScanReverse64 (Visual Studio) or __builtin_clzll (GCC/Clang)
	//       because _BitScanReverse64 can't be called from constexpr code.
	if (v == 0) {
		return uint32(-1);
	}
	bool has_32 = ((v>>32) != 0);
	uint32 count = uint32(has_32)<<5;
	v = has_32 ? (v>>32) : (v);
	bool has_16 = ((v>>16) != 0);
	count += uint32(has_16)<<4;
	v = has_16 ? (v>>16) : (v);
	bool has_8 = ((v>>8) != 0);
	count += uint32(has_8)<<3;
	v = has_8 ? (v>>8) : v;
	bool has_4 = ((v>>4) != 0);
	count += uint32(has_4)<<2;
	v = has_4 ? (v>>4) : v;
	bool has_2 = ((v>>2) != 0);
	count += uint32(has_2)<<1;
	v = has_2 ? (v>>2) : v;
	count += uint32(v>>1);
	return count;
}

[[nodiscard]] constexpr inline uint32 bitScanR32(uint32 v) {
	// NOTE: This doesn't use _BitScanReverse (Visual Studio) or __builtin_clz (GCC/Clang)
	//       because _BitScanReverse can't be called from constexpr code.
	if (v == 0) {
		return uint32(-1);
	}
	bool has_16 = ((v>>16) != 0);
	uint32 count = uint32(has_16)<<4;
	v = has_16 ? (v>>16) : (v);
	bool has_8 = ((v>>8) != 0);
	count += uint32(has_8)<<3;
	v = has_8 ? (v>>8) : v;
	bool has_4 = ((v>>4) != 0);
	count += uint32(has_4)<<2;
	v = has_4 ? (v>>4) : v;
	bool has_2 = ((v>>2) != 0);
	count += uint32(has_2)<<1;
	v = has_2 ? (v>>2) : v;
	count += uint32(v>>1);
	return count;
}

[[nodiscard]] constexpr inline uint32 bitScanR32(uint16 v) {
	// NOTE: This doesn't use _BitScanReverse (Visual Studio) or __builtin_clz (GCC/Clang)
	//       because _BitScanReverse can't be called from constexpr code.
	if (v == 0) {
		return uint32(-1);
	}
	bool has_8 = ((v>>8) != 0);
	uint32 count = uint32(has_8)<<3;
	v = has_8 ? (v>>8) : v;
	bool has_4 = ((v>>4) != 0);
	count += uint32(has_4)<<2;
	v = has_4 ? (v>>4) : v;
	bool has_2 = ((v>>2) != 0);
	count += uint32(has_2)<<1;
	v = has_2 ? (v>>2) : v;
	count += uint32(v>>1);
	return count;
}

[[nodiscard]] constexpr inline uint32 bitScanR32(uint8 v) {
	// NOTE: This doesn't use _BitScanReverse (Visual Studio) or __builtin_clz (GCC/Clang)
	//       because _BitScanReverse can't be called from constexpr code.
	if (v == 0) {
		return uint32(-1);
	}
	bool has_4 = ((v>>4) != 0);
	uint32 count = uint32(has_4)<<2;
	v = has_4 ? (v>>4) : v;
	bool has_2 = ((v>>2) != 0);
	count += uint32(has_2)<<1;
	v = has_2 ? (v>>2) : v;
	count += uint32(v>>1);
	return count;
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
