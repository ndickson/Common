#pragma once

#include "../Types.h"

OUTER_NAMESPACE_BEGIN
namespace math {

using namespace COMMON_LIBRARY_NAMESPACE;

// Add a single item to the combination of sumHigh and sumLow,
// achieving better accuracy in the sum than just a single value
// for the sum.
//
// To do better than both original Kahan summation and the simple improvement,
// this algorithm is more careful about handling the low parts of the sums.
// Tests summing uniform random numbers suggest that this algorithm is
// at least as much better than either common variant of Kahan summation, as
// they are better than simple summing with a single sum variable,
// achieving almost the maximum possible accuracy for the given precision.
//
// This may be overkill for most cases, but it's better to
// be on the safe side, since the whole point of the added precision is
// improved accuracy.  Tests suggest that if T is double (53-bit mantissa),
// this gets accuracy similar to that of a 107-bit mantissa.
template<typename T>
constexpr void kahanSumSingle(T& sumHigh, T& sumLow, const T& currentItem) {
	// This should usually be in descending order of absolute value already.
	T values[3] = {
		sumHigh,
		currentItem,
		sumLow
	};
	T absValues[3] = {
		abs(values[0]),
		abs(values[1]),
		abs(values[2])
	};

	// Make values[0] the largest in absolute value.
	if (absValues[1] >= absValues[0]) {
		T temp = values[0];
		values[0] = values[1];
		values[1] = temp;
		T absTemp = absValues[0];
		absValues[0] = absValues[1];
		absValues[1] = absTemp;
	}
	if (absValues[2] >= absValues[0]) {
		T temp = values[0];
		values[0] = values[2];
		values[2] = temp;
		T absTemp = absValues[0];
		absValues[0] = absValues[2];
		absValues[2] = absTemp;
	}

	// Add smallest to 2nd-smallest
	T larger  = (absValues[1] >= absValues[2]) ? values[1] : values[2];
	T smaller = (absValues[1] >= absValues[2]) ? values[2] : values[1];
	T firstSumHigh = larger + smaller;
	T firstSumLow = smaller - (firstSumHigh - larger);
	T absFirstSumHigh = abs(firstSumHigh);
	T absFirstSumLow  = abs(firstSumLow);
	if (absFirstSumLow > absFirstSumHigh) {
		T temp = firstSumHigh;
		firstSumHigh = firstSumLow;
		firstSumLow = temp;
		absFirstSumHigh = absFirstSumLow;
	}

	// Add the high part of that sum to largest
	larger  = (absFirstSumHigh >= absValues[0]) ? firstSumHigh : values[0];
	smaller = (absFirstSumHigh >= absValues[0]) ? values[0] : firstSumHigh;
	T mainSumHigh = larger + smaller;
	T mainSumLow = smaller - (mainSumHigh - larger);
	T absMainSumHigh = abs(mainSumHigh);
	T absMainSumLow  = abs(mainSumLow);
	if (absMainSumLow > absMainSumHigh) {
		T temp = mainSumHigh;
		mainSumHigh = mainSumLow;
		mainSumLow = temp;
		absMainSumHigh = absMainSumLow;
	}

	// Add low portions together
	mainSumLow += firstSumLow;
	absMainSumLow  = abs(mainSumLow);
	if (absMainSumLow > absMainSumHigh) {
		T temp = mainSumHigh;
		mainSumHigh = mainSumLow;
		mainSumLow = temp;
	}
	sumHigh = mainSumHigh;
	sumLow = mainSumLow;
}

} // namespace math
OUTER_NAMESPACE_END
