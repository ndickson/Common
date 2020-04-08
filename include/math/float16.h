#pragma once

// This file contains a struct representing a standard 16-bit floating-point
// number, with only conversion operations.

#include "../Types.h"
#include "../Bits.h"

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
			bits |= EXP_MASK + IntType(isNaN);
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
			fbits |= FLOAT_EXP_MASK | mantissa;
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
			bits |= EXP_MASK + IntType(isNaN);
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
			fbits |= DOUBLE_EXP_MASK | mantissa;
		}

		static_assert(sizeof(double) == sizeof(DoubleIntType));
		return *reinterpret_cast<const double*>(&fbits);
	}
};

} // namespace math
OUTER_NAMESPACE_END
