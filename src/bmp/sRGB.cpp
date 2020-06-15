// This file contains functions for converting between
// linear colour space and sRGB colour space, frequently used
// for BMP (bitmap) files and screen display.

#include "Types.h"
#include "bmp/sRGB.h"

#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

OUTER_NAMESPACE_BEGIN
namespace bmp {

using namespace COMMON_LIBRARY_NAMESPACE;

constexpr static size_t numPolynomials = 8;
constexpr static size_t numCoefficients = 4;

// The coefficients are in reverse order: x^3, then x^2, then x, then 1
// Polynomial 8 is just for inputs 1.0 and above.
// These are scaled by 255.0.
alignas(16) static const float linearToSRGBPolynomials[numPolynomials+1][numCoefficients] = {
	{1.41447062326479340e+03f, -7.02177271809363333e+02f, 4.26894052667308017e+02f, -1.16191379380294180e+01f},
	{2.87012451360190369e+02f, -2.96380120535544904e+02f, 3.77189412650852034e+02f, -9.54413336568641490e+00f},
	{9.18956815854448337e+01f, -1.59346447261694237e+02f, 3.44814099441341568e+02f, -6.96734904870682659e+00f},
	{4.39503788492497307e+01f, -1.06907852023738158e+02f, 3.25611955194472898e+02f, -4.61253590865128693e+00f},
	{2.54076972841162529e+01f, -7.95266383801442061e+01f, 3.12100145396487221e+02f, -2.38415047204857578e+00f},
	{1.64200605005294378e+01f, -6.28422557026247830e+01f, 3.01759070921935916e+02f, -2.44088398248549082e-01f},
	{1.14220084024107482e+01f, -5.16741351277254211e+01f, 2.93431176004029396e+02f,  1.82830907258328290e+00f},
	{8.37169666646112454e+00f, -4.37075762431614763e+01f, 2.86489851173805448e+02f,  3.84603744743100640e+00f},
	{0.0f, 0.0f, 0.0f, 255.0f}
};

alignas(16) static const int32 staticZeros[4] = {0,0,0,0};
alignas(16) static const int32 staticOnes[4] = {0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000};

// Always choose alpha to be "small", so that it's scaled linearly.
alignas(16) static const int32 staticThreshold[4] = {0x3B4D2BE7, 0x3B4D2BE7, 0x3B4D2BE7, 0x40000000};

constexpr static float smallScale = float(255.0*323.0/25.0);
constexpr static float alphaScale = 255.0f;
alignas(16) static const float staticSmallScales[4] = {smallScale, smallScale, smallScale, alphaScale};

// For multiplying floats by 8 for numbers that aren't too small or too large.
alignas(16) static const int32 staticExponentAdd3[4] = {0x01800000, 0x01800000, 0x01800000, 0};

alignas(16) static const float staticHalfs[4] = {0.5f, 0.5f, 0.5f, 0.5f};

void linearToSRGB(const Vec4f* linear, uint32* sRGB, size_t n) {
	const __m128i zeros = _mm_load_si128(reinterpret_cast<const __m128i*>(staticZeros));
	const __m128i ones = _mm_load_si128(reinterpret_cast<const __m128i*>(staticOnes));
	const __m128i threshold = _mm_load_si128(reinterpret_cast<const __m128i*>(staticThreshold));
	const __m128 smallScales = _mm_castsi128_ps(_mm_load_si128(reinterpret_cast<const __m128i*>(staticSmallScales)));
	const __m128i exponentAdd3 = _mm_load_si128(reinterpret_cast<const __m128i*>(staticExponentAdd3));
	const __m128 halfs = _mm_castsi128_ps(_mm_load_si128(reinterpret_cast<const __m128i*>(staticHalfs)));

	for (size_t i = 0; i < n; ++i) {
		__m128i linearInts = _mm_loadu_si128(reinterpret_cast<const __m128i*>(linear+i));

		// Max with zero (0), to remove negative values
		// Min with one (0x3F800000), to remove values that are too high,
		// and to ensure that indices below don't exceed 8.
		// Doing these in ints also ensures that NaNs are eliminated.
		linearInts = _mm_max_epi32(linearInts, zeros);
		linearInts = _mm_min_epi32(linearInts, ones);

		// Make mask of anything below 0.00313066844250054713, where conversion is linear.
		// Multiply any matching by the correct linear scale (255.0*323.0/25.0).
		const __m128i smallMask = _mm_cmplt_epi32(linearInts, threshold);
		__m128 smallFloats = _mm_castsi128_ps(_mm_and_si128(smallMask, linearInts));
		smallFloats = _mm_mul_ps(smallFloats, smallScales);

		// For remaining, (though masked later), take square root to quickly get closer, then
		// multiply floats by 8, and round down to get indices.
		const __m128 sqrtFloats = _mm_sqrt_ps(_mm_castsi128_ps(linearInts));
		const __m128i largeInts = _mm_castps_si128(sqrtFloats);
		const __m128i indices = _mm_cvttps_epi32(_mm_castsi128_ps(_mm_add_epi32(largeInts, exponentAdd3)));
		const int32 indexA = reinterpret_cast<const int32*>(&indices)[0];
		const int32 indexB = reinterpret_cast<const int32*>(&indices)[1];
		const int32 indexC = reinterpret_cast<const int32*>(&indices)[2];

		// Look up the coefficients for the indices.
		const __m128 polynomialA = _mm_castsi128_ps(_mm_load_si128(reinterpret_cast<const __m128i*>(linearToSRGBPolynomials[indexA])));
		const __m128 polynomialB = _mm_castsi128_ps(_mm_load_si128(reinterpret_cast<const __m128i*>(linearToSRGBPolynomials[indexB])));
		const __m128 polynomialC = _mm_castsi128_ps(_mm_load_si128(reinterpret_cast<const __m128i*>(linearToSRGBPolynomials[indexC])));

		// Swizzle to (A0, B0, C0, *), (A1, B1, C1, *), ...
		const __m128 temp01 = _mm_shuffle_ps(polynomialA, polynomialB, 0x44); // A0, A1, B0, B1
		const __m128 temp23 = _mm_shuffle_ps(polynomialA, polynomialB, 0xEE); // A2, A3, B2, B3
		const __m128 coeff0 = _mm_shuffle_ps(temp01, polynomialC, 0x08); // A0, B0, C0, (C0)
		const __m128 coeff1 = _mm_shuffle_ps(temp01, polynomialC, 0x1D); // A1, B1, C1, (C0)
		const __m128 coeff2 = _mm_shuffle_ps(temp23, polynomialC, 0x28); // A2, B2, C2, (C0)
		const __m128 coeff3 = _mm_shuffle_ps(temp23, polynomialC, 0x3D); // A3, B3, C3, (C0)

		// Apply polynomials
		const __m128 powFloats =
			_mm_add_ps(coeff3, _mm_mul_ps(sqrtFloats,
				_mm_add_ps(coeff2, _mm_mul_ps(sqrtFloats,
					_mm_add_ps(coeff1, _mm_mul_ps(sqrtFloats, coeff0))
				))
			));

		// Combine and round to integers
		const __m128 allFloats = _mm_or_ps(smallFloats, _mm_andnot_ps(_mm_castsi128_ps(smallMask), powFloats));
		const __m128i roundedInts = _mm_cvtps_epi32(allFloats);

		// Store, remembering to swap components 0 and 2 for BGRA ordering.
		// NOTE: Order is blue, green, red, alpha in sRGB, but red, green, blue, alpha in linear.
		const uint32 out0 = reinterpret_cast<const uint32*>(&roundedInts)[0];
		const uint32 out1 = reinterpret_cast<const uint32*>(&roundedInts)[1];
		const uint32 out2 = reinterpret_cast<const uint32*>(&roundedInts)[2];
		const uint32 out3 = reinterpret_cast<const uint32*>(&roundedInts)[3];
		sRGB[i] = out2 | (out1<<8) | (out0<<16) | (out3<<24);
	}
}

} // namespace bmp
OUTER_NAMESPACE_END
