#pragma once

// This file contains functions for converting between
// linear colour space and sRGB colour space, frequently used
// for BMP (bitmap) files and screen display.

#include "../Types.h"
#include "../Vec.h"

#include <cmath>

OUTER_NAMESPACE_BEGIN
namespace bmp {

using namespace COMMON_LIBRARY_NAMESPACE;

[[nodiscard]] static INLINE float linearToSRGB(float linear) {
	using T = float;
	// This is the higher of the two crossovers of these two functions.
	if (linear <= T(0.00313066844250054713)) {
		return T(323.0/25.0)*linear;
	}
	else {
		return (T(211.0)*std::pow(linear, T(5.0/12.0)) - T(11.0))/T(200.0);
	}
}

[[nodiscard]] static INLINE float sRGBToLinear(float sRGB) {
	using T = float;
	if (sRGB <= T(0.0404482362771070689196)) {
		return T(25.0/323.0)*sRGB;
	}
	else {
		return std::pow((T(200.0)*sRGB + T(11.0))/T(211.0), T(12.0/5.0));
	}
}

[[nodiscard]] constexpr INLINE uint8 floatToInt(float f) {
	if (f <= 0) {
		return 0;
	}
	if (f >= 1) {
		return uint8(255);
	}
	return uint8(255.0f*f + 0.5f);
}

[[nodiscard]] constexpr INLINE float intToFloat(uint8 i) {
	return i/255.0f;
}

[[nodiscard]] static inline uint32 linearToSRGB(const Vec3f& linear) {
	// Alpha of 1.0
	// NOTE: Order is blue, green, red, alpha in sRGB, but red, green, blue in linear.
	return uint32(floatToInt(linearToSRGB(linear[2])))
		| (uint32(floatToInt(linearToSRGB(linear[1])))<<8)
		| (uint32(floatToInt(linearToSRGB(linear[0])))<<16)
		| 0xFF000000;
}
[[nodiscard]] static inline uint32 linearToSRGB(const Vec4f& linear) {
	// Alpha stays linear
	// NOTE: Order is blue, green, red, alpha in sRGB, but red, green, blue, alpha in linear.
	return uint32(floatToInt(linearToSRGB(linear[2])))
		| (uint32(floatToInt(linearToSRGB(linear[1])))<<8)
		| (uint32(floatToInt(linearToSRGB(linear[0])))<<16)
		| (uint32(floatToInt(linear[3]))<<24);
}
[[nodiscard]] static inline uint32 linearToSRGB(const Vec3f& linear, const float alpha) {
	// Alpha stays linear
	// NOTE: Order is blue, green, red, alpha in sRGB, but red, green, blue in linear.
	return uint32(floatToInt(linearToSRGB(linear[2])))
		| (uint32(floatToInt(linearToSRGB(linear[1])))<<8)
		| (uint32(floatToInt(linearToSRGB(linear[0])))<<16)
		| (uint32(floatToInt(alpha))<<24);
}

[[nodiscard]] static inline Vec4f sRGBToLinear(uint32 sRGB) {
	// Alpha stays linear
	// NOTE: Order is blue, green, red, alpha in sRGB, but red, green, blue, alpha in linear.
	return Vec4f(
		sRGBToLinear(intToFloat(uint8(sRGB>>16))),
		sRGBToLinear(intToFloat(uint8(sRGB>>8))),
		sRGBToLinear(intToFloat(uint8(sRGB))),
		intToFloat(uint8(sRGB>>24))
	);
}

// This uses an approximation that should be faster and good enough for most uses.
COMMON_LIBRARY_EXPORTED void linearToSRGB(const Vec4f* linear, uint32* sRGB, size_t n);

} // namespace bmp
OUTER_NAMESPACE_END
