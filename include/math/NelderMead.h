#pragma once

// This file contains an implementation of the Nelder-Mead method for
// locally minimizing a non-linear, possibly even discontinuous function
// whose input is a continuous-valued state in n dimensions.

#include "Types.h"
#include <limits>

OUTER_NAMESPACE_BEGIN
namespace math {

// Locally minimize the given function using the Nelder-Mead method.
// The index of the best (lowest value) state in states is returned.
// The states and values arrays should already be initialized to
// numStates states and their corresponding values of function.
// numStates must be (at least) one more than the number of dimensions
// of a state.
//
// VALUE_TYPE can be a vector type, to support different levels of constraints,
// with the most significant components first.  For example, component 0
// could represent the number of violated hard constraints, and component 1
// could represent the extent to which soft constraints are violated.
//
// Nelder-Mead doesn't scale well as the number of dimensions increases,
// since the total space of n+1 states in n dimensions is Omega(n^2), and so
// moving the whole set of states once takes Omega(n^2) time.
// This is fine for n=3, but not so great for n=1000.
template<typename STATE_TYPE,typename VALUE_TYPE,typename FUNCTION_TYPE>
constexpr inline size_t minimizeNelderMead(
	STATE_TYPE* states,
	VALUE_TYPE* values,
	size_t numStates,
	size_t maxSteps,
	const VALUE_TYPE& goodEnoughValue,
	const FUNCTION_TYPE& function) {

	if (numStates < 2) {
		// Can't optimize with a single state.
		return 0;
	}

	// Might as well do a perfect reflection to start.
	constexpr double reflectCoefficient = 1.0;
	// The coefficients below are intentionally not easily
	// approximated by powers or products of each other,
	// by using square roots of prime numbers 2, 3, and 5.
	// Expand by golden ratio (sqrt(5)+1)/2, instead of 2,
	// to slightly reduce risk of power of two artifacts in the function.
	constexpr double expandCoefficient = 1.61803398874989484820;
	// Contract by sqrt(2)-1, instead of 0.5, to slightly reduce risk of
	// power of two artifacts in the function.
	constexpr double contractCoefficient = 0.41421356237309504880;
	// Shrink by sqrt(3)/4, instead of 0.5, to slightly reduce risk of
	// power of two artifacts in the function.
	constexpr double shrinkCoefficient = 0.43301270189221932338;

	const size_t centroidCheckPeriod = numStates/2;
	size_t stepsSinceComputedCentroid = centroidCheckPeriod+1;
	const size_t improvementCheckPeriod = 4*numStates;
	size_t stepsSinceImprovementCheck = 0;
	// NOTE: This is an arbitrary value to start,
	// just so that the function can be constexpr.
	VALUE_TYPE improvementCheckValue(values[0]);
	// NOTE: This is an arbitrary value to start,
	// just so that the function can be constexpr.
	STATE_TYPE centroid(states[0]);
	const double centroidNormalization = 1.0/(numStates-1);
	size_t stepi = 0;
	while (true) {
		// This approach only cares about the best state, the worst state,
		// and the 2nd-worst state, so instead of sorting, just find those.
		// Start with ordering the first two states.
		size_t bestStateIndex = 0;
		size_t worstStateIndex = 1;
		if (values[1] < values[0]) {
			bestStateIndex = 1;
			worstStateIndex = 0;
		}
		size_t nextWorstStateIndex = bestStateIndex;

		// Then, order the rest of the states.
		for (size_t statei = 2; statei < numStates; ++statei) {
			const VALUE_TYPE& value = values[statei];
			if (value < values[bestStateIndex]) {
				bestStateIndex = statei;
			}
			else if (values[worstStateIndex] < value) {
				nextWorstStateIndex = worstStateIndex;
				worstStateIndex = statei;
			}
			else if (values[nextWorstStateIndex] < value) {
				nextWorstStateIndex = statei;
			}
		}

		while (true) {
			// Repeat until isDone met or maxSteps met
			if (stepi >= maxSteps || !(goodEnoughValue < values[bestStateIndex])) {
				return bestStateIndex;
			}
			++stepi;

			if (stepsSinceImprovementCheck == 0) {
				// Set initial best state.
				improvementCheckValue = values[bestStateIndex];
			}
			else if (stepsSinceImprovementCheck >= improvementCheckPeriod) {
				// If the search hasn't improved the best state in
				// enough steps to have potentially updated every state
				// several times, give up.
				if (!(values[bestStateIndex] < improvementCheckValue)) {
					return bestStateIndex;
				}
				stepsSinceImprovementCheck = 0;
			}
			++stepsSinceImprovementCheck;

			if (stepsSinceComputedCentroid >= centroidCheckPeriod) {
				// Fully re-compute centroid (average) of all states except the worst.
				size_t firstNonWorstIndex = (worstStateIndex == 0);
				centroid = centroidNormalization*states[firstNonWorstIndex];
				for (size_t statei = firstNonWorstIndex+1; statei < numStates; ++statei) {
					if (statei == worstStateIndex) {
						continue;
					}
					centroid += centroidNormalization*states[statei];
				}
				stepsSinceComputedCentroid = 0;
			}
			++stepsSinceComputedCentroid;

			const STATE_TYPE direction(centroid - states[worstStateIndex]);

			// Compute worst state reflected through the centroid of the rest
			const STATE_TYPE reflectedState(centroid + reflectCoefficient*direction);
			const VALUE_TYPE reflectedValue = function(reflectedState);

			if (reflectedValue < values[nextWorstStateIndex]) {
				// Reflected state is better than the 2nd-worst.
				if (reflectedValue < values[bestStateIndex]) {
					// If reflected state is the new best, try going even farther
					// by computing expanded state.
					const STATE_TYPE expandedState(centroid + expandCoefficient*direction);
					const VALUE_TYPE expandedValue = function(expandedState);
					const bool isReflectedBetter = (reflectedValue < expandedValue);
					const STATE_TYPE& betterState = isReflectedBetter ? reflectedState : expandedState;
					const VALUE_TYPE betterValue = isReflectedBetter ? reflectedValue : expandedValue;

					// Replace the worst with the better of the reflected state and the expanded state.
					states[worstStateIndex] = betterState;
					values[worstStateIndex] = betterValue;
					bestStateIndex = worstStateIndex;
				}
				else {
					// If reflected state is better than the 2nd-worst,
					// but not better than the best, replace the worst.
					states[worstStateIndex] = reflectedState;
					values[worstStateIndex] = reflectedValue;
				}

				// Second-worst state is now the worst state.
				// centroid must be updated for the change.
				centroid += centroidNormalization*(states[worstStateIndex] - states[nextWorstStateIndex]);
				worstStateIndex = nextWorstStateIndex;

				// Find new 2nd-worst state.
				// NOTE: It doesn't need to have a value strictly greater than the worst;
				// it just needs to be a worst with index not equal to the worst.
				nextWorstStateIndex = (worstStateIndex == 0);
				for (size_t statei = nextWorstStateIndex+1; statei < numStates; ++statei) {
					if (statei != worstStateIndex && values[nextWorstStateIndex] < values[statei]) {
						nextWorstStateIndex = statei;
					}
				}
			}
			else {
				// Else compute contracted state.
				const STATE_TYPE contractedState(centroid - contractCoefficient*direction);
				const VALUE_TYPE contractedValue = function(contractedState);
				if (contractedValue < values[worstStateIndex]) {
					// If contracted state is better than the worst state, replace the worst.
					states[worstStateIndex] = contractedState;
					values[worstStateIndex] = contractedValue;

					// Update centroid if worst state is no longer the worst state.
					const VALUE_TYPE& nextWorstValue = values[nextWorstStateIndex];
					if (contractedValue < nextWorstValue || (contractedValue == nextWorstValue && nextWorstStateIndex < worstStateIndex)) {
						centroid += centroidNormalization * (states[worstStateIndex] - states[nextWorstStateIndex]);
					}
				}
				else {
					// Else move all states toward the best, and recompute all values.
					const STATE_TYPE& bestState = states[bestStateIndex];
					for (size_t statei = 0; statei < numStates; ++statei) {
						if (statei == bestStateIndex) {
							continue;
						}
						states[statei] = bestState + shrinkCoefficient*(states[statei] - bestState);
						values[statei] = function(states[statei]);
					}

					// centroid must be recomputed.
					stepsSinceComputedCentroid = centroidCheckPeriod;
				}
				// Break out of the inner loop to re-find best, worst, and 2nd-worst.
				break;
			}
		}
	}
}

} // namespace math
OUTER_NAMESPACE_END
