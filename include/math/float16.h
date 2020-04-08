#pragma once

// This file contains a struct representing a standard 16-bit floating-point
// number, with only conversion operations.

#include "../Types.h"
#include "../Bits.h"

#include <limits>

OUTER_NAMESPACE_BEGIN
namespace math {

using namespace COMMON_LIBRARY_NAMESPACE;

struct float16 {
	// 15       8 7  4 3  0
	//  seee eemm mmmm mmmm
	uint16 bits;

	INLINE float16() noexcept = default;
	constexpr INLINE float16(const float16&) noexcept = default;

	constexpr INLINE float16& operator=(const float16&) noexcept = default;

	using IntType = uint16;
	constexpr static uint32 MANTISSA_BITS = 10;
	constexpr static uint16 MANTISSA_MASK = uint16((uint16(1)<<MANTISSA_BITS)-1);
	constexpr static int32 EXP_EXCESS = 15;
	constexpr static uint32 EXP_BITS = 5;
	constexpr static uint16 EXP_MASK_SHIFT = uint16((uint16(1)<<EXP_BITS)-1);
	constexpr static uint16 EXP_MASK = EXP_MASK_SHIFT<<MANTISSA_BITS;
	constexpr static uint16 SIGN_MASK = uint16(0x8000);

	using FloatIntType = uint32;
	constexpr static uint32 FLOAT_MANTISSA_BITS = 23;
	constexpr static FloatIntType FLOAT_MANTISSA_MASK = ((FloatIntType(1)<<FLOAT_MANTISSA_BITS)-1);
	constexpr static int32 FLOAT_EXP_EXCESS = 0x7F;
	constexpr static uint32 FLOAT_EXP_BITS = 8;
	constexpr static FloatIntType FLOAT_EXP_MASK_SHIFT = FloatIntType((FloatIntType(1)<<FLOAT_EXP_BITS)-1);
	constexpr static FloatIntType FLOAT_EXP_MASK = FLOAT_EXP_MASK_SHIFT<<FLOAT_MANTISSA_BITS;

	using DoubleIntType = uint64;
	constexpr static uint32 DOUBLE_MANTISSA_BITS = 52;
	constexpr static DoubleIntType DOUBLE_MANTISSA_MASK = ((DoubleIntType(1)<<DOUBLE_MANTISSA_BITS)-1);
	constexpr static int64 DOUBLE_EXP_EXCESS = 0x3FF;
	constexpr static uint32 DOUBLE_EXP_BITS = 11;
	constexpr static DoubleIntType DOUBLE_EXP_MASK_SHIFT = DoubleIntType((DoubleIntType(1)<<DOUBLE_EXP_BITS)-1);
	constexpr static DoubleIntType DOUBLE_EXP_MASK = DOUBLE_EXP_MASK_SHIFT<<DOUBLE_MANTISSA_BITS;

	struct InitUsingBits {};

	constexpr INLINE float16(uint16 bits_, InitUsingBits) noexcept : bits(bits_) {}

	explicit float16(float f) noexcept {
		static_assert(sizeof(float) == sizeof(FloatIntType));
		FloatIntType fbits = *reinterpret_cast<const FloatIntType*>(&f);

		// Sign bit
		bits = IntType((fbits>>(32-16)) & SIGN_MASK);

		int32 exponent = int32((fbits>>FLOAT_MANTISSA_BITS) & FLOAT_EXP_MASK_SHIFT) - FLOAT_EXP_EXCESS;
		FloatIntType mantissa = fbits & FLOAT_MANTISSA_MASK;
		if (exponent >= 16) {
			// Infinity or NaN
			bool isNaN = (exponent == 0x80 && mantissa != 0);
			if (!isNaN) {
				bits |= EXP_MASK;
			}
			else {
				// This shifting preserves the high mantissa bit for distinguishing
				// between quiet NaN and signaling NaN.
				mantissa >>= (FLOAT_MANTISSA_BITS-MANTISSA_BITS);
				if (mantissa == 0) {
					// The shifting shifted out the non-zero bits, so make one non-zero.
					mantissa = 1;
				}
				bits |= EXP_MASK + IntType(mantissa);
			}
			return;
		}
		if (exponent >= -14) {
			// Normal number (unless rounds up to infinity)
			bits |= IntType((exponent+EXP_EXCESS)<<MANTISSA_BITS) | IntType(mantissa >> (FLOAT_MANTISSA_BITS-MANTISSA_BITS));

			// Round half to even.
			bool odd = (bits & 1);
			bool roundingBit = ((mantissa >> (FLOAT_MANTISSA_BITS-MANTISSA_BITS-1)) & 1);
			bool nonzeroBelow = (mantissa & ((FloatIntType(1)<<(FLOAT_MANTISSA_BITS-MANTISSA_BITS-1)) - 1)) != 0;
			bits += roundingBit && (nonzeroBelow || odd);
			return;
		}

		// Denormal number (unless rounds up to normal)

		// If bit 9 would be set, the effective exponent would be -15.
		// If bit 0 would be set, the effective exponent would be -24.
		// If the exponent is -25 and the mantissa is non-zero, round up to lowest possible non-zero value.
		// Below that, round to zero.
		if (exponent < -25 || (exponent == -25 && mantissa == 0)) {
			// Zero: bits already represents a signed zero value.
			return;
		}

		// Non-zero denormal number
		mantissa |= FloatIntType(1)<<FLOAT_MANTISSA_BITS;
		bits |= (mantissa >> (-exponent-1));

		// Round half to even.
		bool odd = (bits & 1);
		bool roundingBit = ((mantissa >> (-exponent-2)) & 1);
		bool nonzeroBelow = (mantissa & ((FloatIntType(1)<<(-exponent-2)) - 1)) != 0;
		bits += roundingBit && (nonzeroBelow || odd);
	}

	[[nodiscard]] explicit operator float() const noexcept {
		FloatIntType fbits = (FloatIntType(bits & SIGN_MASK) << (32-16));

		int32 exponent = int32((bits>>MANTISSA_BITS) & EXP_MASK_SHIFT) - EXP_EXCESS;
		IntType mantissa = IntType(bits & MANTISSA_MASK);

		if (exponent == -15) {
			if (mantissa != 0) {
				// Non-zero denormal number
				uint32 topBit = bitScanR16(mantissa);

				// mantissa has an extra 1 bit already in the low exponent bit,
				// so add one less to the exponent.
				// topBit of 9 corresponds with exponent of -15.
				fbits |= ((topBit-9-15-1+FLOAT_EXP_EXCESS)<<FLOAT_MANTISSA_BITS)
					+ (FloatIntType(mantissa)<<(FLOAT_MANTISSA_BITS-topBit));
			}
		}
		else if (exponent < 16) {
			// Normal number
			fbits |= (FloatIntType(exponent + FLOAT_EXP_EXCESS)<<FLOAT_MANTISSA_BITS)
				| (FloatIntType(mantissa)<<(FLOAT_MANTISSA_BITS-MANTISSA_BITS));
		}
		else {
			// Infinity or NaN
			fbits |= FLOAT_EXP_MASK | (FloatIntType(mantissa)<<(FLOAT_MANTISSA_BITS-MANTISSA_BITS));;
		}

		static_assert(sizeof(float) == sizeof(FloatIntType));
		return *reinterpret_cast<const float*>(&fbits);
	}

	explicit float16(double f) noexcept {
		static_assert(sizeof(double) == sizeof(DoubleIntType));
		DoubleIntType fbits = *reinterpret_cast<const DoubleIntType*>(&f);

		// Sign bit
		bits = IntType((fbits>>(64-16)) & SIGN_MASK);

		int64 exponent = int64((fbits>>DOUBLE_MANTISSA_BITS) & DOUBLE_EXP_MASK_SHIFT) - DOUBLE_EXP_EXCESS;
		DoubleIntType mantissa = fbits & DOUBLE_MANTISSA_MASK;
		if (exponent >= 16) {
			// Infinity or NaN
			bool isNaN = (exponent == 0x400 && mantissa != 0);
			if (!isNaN) {
				bits |= EXP_MASK;
			}
			else {
				// This shifting preserves the high mantissa bit for distinguishing
				// between quiet NaN and signaling NaN.
				mantissa >>= (DOUBLE_MANTISSA_BITS-MANTISSA_BITS);
				if (mantissa == 0) {
					// The shifting shifted out the non-zero bits, so make one non-zero.
					mantissa = 1;
				}
				bits |= EXP_MASK + IntType(mantissa);
			}
			return;
		}
		if (exponent >= -14) {
			// Normal number (unless rounds up to infinity)
			bits |= IntType((exponent+EXP_EXCESS)<<MANTISSA_BITS) | IntType(mantissa >> (DOUBLE_MANTISSA_BITS-MANTISSA_BITS));

			// Round half to even.
			bool odd = (bits & 1);
			bool roundingBit = ((mantissa >> (DOUBLE_MANTISSA_BITS-MANTISSA_BITS-1)) & 1);
			bool nonzeroBelow = (mantissa & ((DoubleIntType(1)<<(DOUBLE_MANTISSA_BITS-MANTISSA_BITS-1)) - 1)) != 0;
			bits += roundingBit && (nonzeroBelow || odd);
			return;
		}

		// Denormal number (unless rounds up to normal)

		// If bit 9 would be set, the effective exponent would be -15.
		// If bit 0 would be set, the effective exponent would be -24.
		// If the exponent is -25 and the mantissa is non-zero, round up to lowest possible non-zero value.
		// Below that, round to zero.
		if (exponent < -25 || (exponent == -25 && mantissa == 0)) {
			// Zero: bits already represents a signed zero value.
			return;
		}

		// Non-zero denormal number
		mantissa |= DoubleIntType(1)<<DOUBLE_MANTISSA_BITS;
		bits |= (mantissa >> (-exponent-1));

		// Round half to even.
		bool odd = (bits & 1);
		bool roundingBit = ((mantissa >> (-exponent-2)) & 1);
		bool nonzeroBelow = (mantissa & ((DoubleIntType(1)<<(-exponent-2)) - 1)) != 0;
		bits += roundingBit && (nonzeroBelow || odd);
	}

	[[nodiscard]] explicit operator double() const noexcept {
		DoubleIntType fbits = (DoubleIntType(bits & SIGN_MASK) << (64-16));

		int32 exponent = int32((bits>>MANTISSA_BITS) & EXP_MASK_SHIFT) - EXP_EXCESS;
		IntType mantissa = IntType(bits & MANTISSA_MASK);

		if (exponent == -15) {
			if (mantissa != 0) {
				// Non-zero denormal number
				uint32 topBit = bitScanR16(mantissa);

				// mantissa has an extra 1 bit already in the low exponent bit,
				// so add one less to the exponent.
				// topBit of 9 corresponds with exponent of -15.
				fbits |= ((topBit-9-15-1+DOUBLE_EXP_EXCESS)<<DOUBLE_MANTISSA_BITS)
					+ (DoubleIntType(mantissa)<<(DOUBLE_MANTISSA_BITS-topBit));
			}
		}
		else if (exponent < 16) {
			// Normal number
			fbits |= (DoubleIntType(exponent + DOUBLE_EXP_EXCESS)<<DOUBLE_MANTISSA_BITS)
				| (DoubleIntType(mantissa)<<(DOUBLE_MANTISSA_BITS-MANTISSA_BITS));
		}
		else {
			// Infinity or NaN
			fbits |= DOUBLE_EXP_MASK | (DoubleIntType(mantissa)<<(DOUBLE_MANTISSA_BITS-MANTISSA_BITS));
		}

		static_assert(sizeof(double) == sizeof(DoubleIntType));
		return *reinterpret_cast<const double*>(&fbits);
	}

	[[nodiscard]] constexpr float16 operator-() const noexcept {
		return float16(bits & 0x8000, InitUsingBits());
	}
	[[nodiscard]] constexpr float16 operator+() const noexcept {
		return *this;
	}
};

} // namespace math
OUTER_NAMESPACE_END

namespace std {
	template<>
	class numeric_limits<OUTER_NAMESPACE::math::float16> {
		using float16 = OUTER_NAMESPACE::math::float16;

		constexpr static bool is_specialized = true;

		constexpr static std::float_denorm_style has_denorm = std::denorm_present;
		constexpr static bool has_denorm_loss = false;
		constexpr static bool has_infinity = true;
		constexpr static bool has_quiet_NaN = true;
		constexpr static bool has_signaling_NaN = true;
		constexpr static bool is_bounded = true;
		constexpr static bool is_exact = false;
		constexpr static bool is_iec559 = true;
		constexpr static bool is_integer = false;
		constexpr static bool is_modulo = false;
		constexpr static bool is_signed = true;
		constexpr static std::float_round_style round_style = std::round_to_nearest;
		constexpr static bool tinyness_before = false;
		constexpr static bool traps = false;

		constexpr static int digits = 11;
		constexpr static int digits10 = 3;
		constexpr static int max_digits10 = 5;
		constexpr static int radix = 2;

		// The standard requires that this be *1 higher* than the
		// minimum exponent that produces a normal number, i.e. -14.
		constexpr static int min_exponent = -13;

		constexpr static int min_exponent10 = -4;

		// The standard requires that this be *1 higher* than the
		// maximum exponent that produces a normal number, i.e. 15.
		constexpr static int max_exponent = 16;

		constexpr static int max_exponent10 = 4;

		constexpr static float16 min() noexcept {
			// Exponent -14 and mantissa all zeros
			return float16(0x0400, float16::InitUsingBits());
		}
		constexpr static float16 max() noexcept {
			// Exponent 15 and mantissa all ones
			return float16(0x7BFF, float16::InitUsingBits());
		}
		constexpr static float16 lowest() noexcept {
			return -max();
		}
		constexpr static float16 epsilon() noexcept {
			// Exponent -10 and mantissa all zeros
			return float16(0x1400, float16::InitUsingBits());
		}
		constexpr static float16 round_error() noexcept {
			// Exponent -1 and mantissa all zeros
			return float16(0x4000, float16::InitUsingBits());
		}
		constexpr static float16 infinity() noexcept {
			// Exponent 16 and mantissa all zeros
			return float16(0x7C00, float16::InitUsingBits());
		}
		constexpr static float16 quiet_NaN() noexcept {
			// Exponent 16 and mantissa all zeros except highest bit
			return float16(0x7E00, float16::InitUsingBits());
		}
		constexpr static float16 signaling_NaN() noexcept {
			// Exponent 16 and mantissa all zeros except 2nd-highest bit
			return float16(0x7D00, float16::InitUsingBits());
		}
		constexpr static float16 denorm_min() noexcept {
			// Exponent -15 and mantissa all zeros except lowest bit
			return float16(0x0001, float16::InitUsingBits());
		}
	};
}
