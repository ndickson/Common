#pragma once

// This file defines a class representing a contiguous span (a.k.a. interval or
// range).  For integer types, the range is [min,max) (i.e. inclusive-exclusive),
// and for floating-point types, it is [min,max] (i.e. inclusive-inclusive).
// This ensures that max-min is always the size, while still allowing
// floating-point spans that contain a single value.
// An intentionally empty span is represented by being maximally inverted
// for the given type, without any infinite values, (to ensure that the centre
// isn't NaN).

#include "Types.h"
#include "Vec.h"
#include <limits>
#include <type_traits>

OUTER_NAMESPACE_START
LIBRARY_NAMESPACE_START

template<typename T>
struct Span : public Vec<T,2> {
	using ThisType = Span<T>;
	using ParentType = Vec<T,2>;

	INLINE Span() = default;
	constexpr INLINE Span(const ThisType& that) = default;
	constexpr INLINE Span(ThisType&& that) = default;

	constexpr INLINE Span(const T& min, const T& max) : ParentType(min, max) {}

	template<typename S>
	explicit constexpr INLINE Span(const Span<S>& that) : ParentType(T(that.min()), T(that.max())) {}

	struct MakeEmpty {
		constexpr INLINE MakeEmpty() {}
	};
	// NOTE: This uses max and lowest instead of infinity and negative infinity
	// for floating-point types, so that the average of min and max is 0, not NaN.
	constexpr INLINE Span(MakeEmpty) : ParentType(std::numeric_limits<T>::max(),std::numeric_limits<T>::lowest()) {}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;

	constexpr INLINE void makeEmpty() {
		// NOTE: This uses max and lowest instead of infinity and negative infinity
		// for floating-point types, so that the average of min and max is 0, not NaN.
		min() = std::numeric_limits<T>::max();
		max() = std::numeric_limits<T>::lowest();
	}

	// An integer span is empty if max <= min, since it's inclusive-exclusive.
	// A floating-point span is empty if max < min, since it's inclusive-inclusive.
	constexpr INLINE bool isEmpty() const {
		if (std::is_integral<T>::value)
			return max() <= min();
		return max() < min();
	}

	constexpr INLINE T& min() {
		return ParentType::v[0];
	}
	constexpr INLINE const T& min() const {
		return ParentType::v[0];
	}
	// NOTE: For integer types, this is one more than the maximum value.
	constexpr INLINE T& max() {
		return ParentType::v[1];
	}
	// NOTE: For integer types, this is one more than the maximum value.
	constexpr INLINE const T& max() const {
		return ParentType::v[1];
	}

	constexpr INLINE T centre() const {
		if constexpr (std::is_integral<T>::value) {
			// Use >>1 instead of /2 for consistent floor with negative integers.
			// (/2 gives ceiling for odd negative integers.)
			// It's also very slightly faster.
			return (min()+max())>>1;
		}
		else {
			return T(0.5)*(min()+max());
		}
	}
	constexpr INLINE T size() const {
		return max()-min();
	}

	constexpr INLINE void swap(ThisType& that) {
		ThisType other(*this);
		*this = that;
		that = other;
	}

	constexpr INLINE void unionWith(const ThisType& that) {
		if (that.min() < min()) {
			min() = that.min();
		}
		if (that.max() > max()) {
			max() = that.max();
		}
	}
	constexpr INLINE void intersectWith(const ThisType& that) {
		if (that.min() > min()) {
			min() = that.min();
		}
		if (that.max() < max()) {
			max() = that.max();
		}
		// TODO: Do we need to make this span maximally inverted if it's now inverted?
	}

	constexpr INLINE void insert(const T& value) {
		if (value < min()) {
			min() = value;
		}
		// Although this is usually mutually exclusive from the
		// previous condition, if the span is the empty span (inverted),
		// both could be necessary.
		if (value > max()) {
			max() = value;
		}
	}

private:
	// Delete functions from BaseVec and Vec<T,2> that don't apply to a Span
	T length2() const = delete;
	T length() const = delete;
	T makeUnit() = delete;
	T makeLength(T length) = delete;
	template<typename THAT_SUBCLASS,typename S,size_t N>
	decltype(conjugate(T())*S()) dot(const BaseVec<THAT_SUBCLASS,S,N>& that) const = delete;
	decltype(T()*T()) cross(const ParentType& that) const = delete;
	ParentType perpendicular() const = delete;
};

LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
