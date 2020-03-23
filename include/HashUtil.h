#pragma once

#include "Types.h"

#include <assert.h>
#include <utility>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

namespace hash {

template<typename VALUE_T>
using Pair = std::pair<VALUE_T, size_t>;

constexpr size_t EMPTY_INDEX = ~size_t(0);

template<bool CHECK_EQUAL, typename StaticEquals, typename VALUE_T, typename KEY_T>
static inline bool findInTable(const Pair<VALUE_T>*const begin, const size_t capacity, uint64 hashCode, const KEY_T& key, size_t& index, size_t& targetIndex) {
	if (begin == nullptr) {
		assert(capacity == 0);
		index = EMPTY_INDEX;
		return false;
	}
	assert(capacity != 0);

	targetIndex = size_t(hashCode % capacity);
	size_t currentIndex = targetIndex;

	const Pair<VALUE_T>* current = begin + currentIndex;
	size_t currentTargetIndex = current->second;

	// The most common cases are immediately hitting an empty slot
	// or immediately finding a match.
	if (currentTargetIndex == EMPTY_INDEX) {
		// Empty slot, so not found
		index = currentIndex;
		return false;
	}
	if constexpr (CHECK_EQUAL) {
		if ((currentTargetIndex == targetIndex) && StaticEquals::equals(current->first, key)) {
			// Found an equal item, so return its index.
			index = currentIndex;
			return true;
		}
	}

	// If the starting index is in a wrap-around region, skip it
	// by finding the first item with a lower target index.
	bool insideWrapAround = (currentTargetIndex > targetIndex);
	if (insideWrapAround) {
		do {
			++current;
			++currentIndex;

			// NOTE: currentIndex should never reach the capacity here,
			// since a wrap-around region should never reach the end.
			assert(currentIndex != capacity);
			// NOTE: currentIndex also should never reach targetIndex,
			// since it doesn't wrap back there
			assert(currentIndex != targetIndex);

			size_t newTargetIndex = current->second;
			// This check for empty index is necessary here,
			// since EMPTY_INDEX is greater than any valid target index.
			if (newTargetIndex == EMPTY_INDEX) {
				index = currentIndex;
				return false;
			}
			insideWrapAround = (newTargetIndex >= currentTargetIndex);
			currentTargetIndex = newTargetIndex;
		} while (insideWrapAround);
	}
	else {
		++current;
		++currentIndex;
		if (currentIndex == capacity) {
			currentIndex = 0;
			current = begin;
		}
		if (currentIndex == targetIndex) {
			// Wrapped all the way around, (table must be capacity 1),
			// so item wasn't found.
			index = currentIndex;
			return false;
		}
		currentTargetIndex = current->second;
	}

	// NOTE: The empty slot index condition is actually redundant,
	// since the value of EMPTY_INDEX is gerater than any targetIndex.
	while (currentTargetIndex <= targetIndex && currentTargetIndex != EMPTY_INDEX) {
		if constexpr (CHECK_EQUAL) {
			if ((currentTargetIndex == targetIndex) && StaticEquals::equals(current->first, key)) {
				// Found an equal item, so return its index.
				index = currentIndex;
				return true;
			}
		}

		++current;
		++currentIndex;
		if (currentIndex == capacity) {
			currentIndex = 0;
			current = begin;
		}
		if (currentIndex == targetIndex) {
			// Wrapped all the way around, (table must be completely full),
			// so item wasn't found.
			index = currentIndex;
			return false;
		}
		currentTargetIndex = current->second;
	}


	// Not in a wrap around and reached something that belongs
	// after the query value or empty slot.
	index = currentIndex;
	return false;
}

template<typename VALUE_T>
static inline void insertIntoTable(Pair<VALUE_T>*const begin, const size_t capacity, VALUE_T&& value, const size_t index, const size_t targetIndex) {
	// Insert (value,targetIndex) at index and shift anything down to make room, if needed.
	assert(index < capacity);

	Pair<VALUE_T>* current = begin + index;
	size_t currentIndex = index;
	if (current->second == EMPTY_INDEX) {
		// Empty space right away, so just write it.
		current->first = std::move(value);
		current->second = targetIndex;
		return;
	}

	Pair<VALUE_T> previous = std::move(*current);
	current->first = std::move(value);
	current->second = targetIndex;

	// Shift pairs forward.
	while (true) {
		++current;
		++currentIndex;
		if (currentIndex == capacity) {
			current = begin;
			currentIndex = 0;
		}
		// If we've reached targetIndex, there's something wrong with the table.
		assert(currentIndex != targetIndex);
		if (current->second == EMPTY_INDEX) {
			// Found empty space, so write the final pair.
			*current = std::move(previous);
			break;
		}
		std::swap(previous, *current);
	}
}

template<typename VALUE_T>
static inline void eraseFromTable(Pair<VALUE_T>*const begin, const size_t capacity, Pair<VALUE_T>* current, size_t currentIndex) {
	assert(current->second != EMPTY_INDEX);

	while (true) {
		Pair<VALUE_T>* next = current + 1;
		size_t nextIndex = currentIndex + 1;
		if (nextIndex == capacity) {
			nextIndex = 0;
			next = begin;
		}

		size_t nextTargetIndex = next->second;
		if (nextTargetIndex == EMPTY_INDEX || nextTargetIndex == nextIndex) {
			// Empty item or item in correct place, so this is the
			// end of the span of displaced items.
			break;
		}

		// Move the next item back.
		*current = std::move(*next);

		current = next;
		currentIndex = nextIndex;
	}

	// Overwrite last item in span to clear it.
	current->first = VALUE_T();
	current->second = EMPTY_INDEX;
}

// These increase by a factor of around 2.1 each time.
// This is good up to over 18 trillion, so should be plenty for now.
constexpr static size_t primeCapacities[] = {
	        2,         5,        11,         23,
	       47,        97,       199,        419,
	      877,      1847,      3881,       8161,
	    17137,     35983,     75571,     158699,
	   333269,    699863,   1469717,    3086407,
	  6481457,  13611053,  28583207,   60024763,
	126052013, 264709219, 555889381, 1167367727,
	size_t(2451472223ULL), size_t(5148091673ULL),
	size_t(10810992509ULL), size_t(22703084269ULL),
	size_t(47676476983ULL), size_t(100120601663ULL),
	size_t(210253263499ULL), size_t(441531853349ULL),
	size_t(927216892031ULL), size_t(1947155473271ULL),
	size_t(4089026493883ULL), size_t(8586955637209ULL),
	size_t(18032606838161ULL)
};

[[nodiscard]] static inline size_t nextPrimeCapacity(size_t atLeast) {
	size_t index = 0;
	while (primeCapacities[index] < atLeast) {
		++index;
	}
	return primeCapacities[index];
}

} // namespace hash

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
