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

	explicit INLINE float16(float f) noexcept {
		static_assert(sizeof(float) == sizeof(uint32));
		uint32 fbits = *reinterpret_cast<const uint32*>(&f);

		// Sign bit
		bits = uint16((fbits & 0x80000000)>>16);

		int32 exponent = int32((fbits>>23) & 0xFF) - 0x7F;
		uint32 mantissa = fbits & 0x007FFFFF;
		if (exponent >= 16) {
			// Infinity or NaN
			bool isNaN = (exponent == 0x80 && mantissa != 0);
			bits |= uint16(0x7C00) + uint16(isNaN);
			return;
		}
		if (exponent >= -14) {
			// Normal number (unless rounds up to infinity)
			bits |= uint16((exponent+15)<<10) | uint16(mantissa >> (23-10));

			// Round half to even.
			bool odd = (bits & 1);
			bool roundingBit = ((mantissa >> (23-10-1)) & 1);
			bool nonzeroBelow = (mantissa & ((1<<(23-10-1)) - 1)) != 0;
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
		mantissa |= uint32(1<<23);
		bits |= (mantissa >> (-exponent-1));

		// Round half to even.
		bool odd = (bits & 1);
		bool roundingBit = ((mantissa >> (-exponent-2)) & 1);
		bool nonzeroBelow = (mantissa & ((1<<(-exponent-2)) - 1)) != 0;
		bits += roundingBit && (nonzeroBelow || odd);
	}

	explicit INLINE operator float() const noexcept {
		uint32 fbits = (uint32(bits & 0x8000) << 16);

		int32 exponent = int32((bits>>10) & 0x1F) - 0xF;
		uint32 mantissa = uint32(bits & 0x03FF);

		if (exponent == -15) {
			if (mantissa != 0) {
				// Non-zero denormal number
				uint32 topBit = bitScanR32(mantissa);

				mantissa <<= (23-topBit);
				// mantissa has an extra 1 bit already in the low exponent bit,
				// so add one less to the exponent.
				// topBit of 9 corresponds with exponent of -15.
				fbits |= mantissa + ((topBit-9-15-1+0x7F)<<23);
			}
		}
		else if (exponent < 16) {
			// Normal number
			fbits |= uint32((exponent + 0x7F)<<23) | (mantissa<<(23-10));
		}
		else {
			// Infinity or NaN
			fbits |= uint32(0x7F800000) | mantissa;
		}

		static_assert(sizeof(float) == sizeof(uint32));
		return *reinterpret_cast<const float*>(&fbits);
	}
};

} // namespace math
OUTER_NAMESPACE_END
