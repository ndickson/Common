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
struct Span : public BaseVec<Span<T>,T,2> {
	T v[2];

	using ThisType = Span<T>;

	INLINE Span() = default;
	constexpr INLINE Span(const ThisType& that) = default;
	constexpr INLINE Span(ThisType&& that) = default;

	constexpr INLINE Span(const T& min, const T& max) : v{min, max} {}

	template<typename S>
	explicit constexpr INLINE Span(const Span<S>& that) : v{T(that.min()), T(that.max())} {}

	struct MakeEmpty {
		constexpr INLINE MakeEmpty() {}
	};
	// NOTE: This uses max and lowest instead of infinity and negative infinity
	// for floating-point types, so that the average of min and max is 0, not NaN.
	constexpr INLINE Span(MakeEmpty) : v{std::numeric_limits<T>::max(),std::numeric_limits<T>::lowest()} {}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;

	constexpr INLINE T& operator[](size_t i) {
		// This static_assert needs to be in a function, because it refers to
		// ThisType, and the compiler doesn't let you reference the type that's
		// currently being compiled from class scope.
		static_assert(std::is_pod<ThisType>::value || !std::is_pod<T>::value, "Span should be a POD type if T is.");

		return v[i];
	}
	constexpr INLINE const T& operator[](size_t i) const {
		return v[i];
	}

	constexpr INLINE T* data() {
		return v;
	}
	constexpr INLINE const T* data() const {
		return v;
	}
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
		return v[0];
	}
	constexpr INLINE const T& min() const {
		return v[0];
	}
	// NOTE: For integer types, this is one more than the maximum value.
	constexpr INLINE T& max() {
		return v[1];
	}
	// NOTE: For integer types, this is one more than the maximum value.
	constexpr INLINE const T& max() const {
		return v[1];
	}

	constexpr INLINE T centre() const {
		if constexpr (std::is_pointer<T>::value || (std::is_integral<T>::value && std::is_unsigned<T>::value)) {
			// size() always fits in the integer type if unsigned.
			// min()+max() might overflow, so use size() instead.
			// min()+max() is undefined for a pointer type, so use size().
			return min() + (size()>>1);
		}
		else if constexpr (std::is_integral<T>::value) {
			// The std::conditional is because Visual Studio 2017 doesn't respect
			// the if constexpr, and errors in make_unsigned if T is float.
			using unsignedT = typename std::make_unsigned<typename std::conditional<std::is_integral<T>::value,T,uint64>::type>::type;
			// Signed integer type: size() might overflow, but to a valid unsigned integer.
			return min() + T(((unsignedT)size())>>1);
		}
		else {
			return T(0.5)*(min()+max());
		}
	}
	constexpr INLINE auto size() const {
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
};

LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
