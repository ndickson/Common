#pragma once

// This file contains a generic, fixed-dimension box class, Box, and
// two specializations, Box2 and Box3.

#include "Types.h"
#include "Span.h"
#include "Vec.h"
#include <type_traits>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

template<typename SUBCLASS,typename T,size_t N>
struct BaseBox {
protected:
	constexpr INLINE SUBCLASS& subclass() {
		return static_cast<SUBCLASS&>(*this);
	}
	constexpr INLINE const SUBCLASS& subclass() const {
		return static_cast<const SUBCLASS&>(*this);
	}
public:

	using BaseType = BaseBox<SUBCLASS,T,N>;
	using ValueType = T;
	using SpanType = Span<T>;
	static constexpr size_t TupleSize = N;

private:
	[[nodiscard]] constexpr INLINE Span<T>& operator[](size_t i) {
		return subclass()[i];
	}
	[[nodiscard]] constexpr INLINE const Span<T>& operator[](size_t i) const {
		return subclass()[i];
	}
	template<typename THAT_SUBCLASS,typename S,size_t M>
	friend struct BaseBox;
public:

	constexpr INLINE void unionWith(const BaseType& that) {
		for (size_t i = 0; i < N; ++i) {
			subclass()[i].unionWith(that[i]);
		}
	}
	constexpr INLINE void intersectWith(const BaseType& that) {
		for (size_t i = 0; i < N; ++i) {
			subclass()[i].intersectWith(that[i]);
		}
	}

	constexpr INLINE void insert(const Vec<T,N>& position) {
		for (size_t i = 0; i < N; ++i) {
			subclass()[i].insert(position[i]);
		}
	}

	[[nodiscard]] constexpr INLINE Vec<T,N> min() const {
		// NOTE: Initialization to zero is just so that the function can be constexpr.
		Vec<T,N> out(T(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = subclass()[i].min();
		}
		return out;
	}
	[[nodiscard]] constexpr INLINE Vec<T,N> max() const {
		// NOTE: Initialization to zero is just so that the function can be constexpr.
		Vec<T,N> out(T(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = subclass()[i].max();
		}
		return out;
	}
	[[nodiscard]] constexpr INLINE Vec<T,N> centre() const {
		// NOTE: Initialization to zero is just so that the function can be constexpr.
		Vec<T,N> out(T(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = subclass()[i].centre();
		}
		return out;
	}
	[[nodiscard]] constexpr INLINE Vec<decltype(T()-T()),N> size() const {
		using S = decltype(T()-T());
		// NOTE: Initialization to zero is just so that the function can be constexpr.
		Vec<S,N> out(S(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = subclass()[i].size();
		}
		return out;
	}

	constexpr INLINE void operator+=(const Vec<T,N>& translation) {
		for (size_t i = 0; i < N; ++i) {
			Span<T>& axis = subclass()[i];
			const T &offset = translation[i];
			axis.min() += offset;
			axis.max() += offset;
		}
	}
	constexpr INLINE void operator-=(const Vec<T,N>& translation) {
		for (size_t i = 0; i < N; ++i) {
			Span<T>& axis = subclass()[i];
			const T &offset = translation[i];
			axis.min() -= offset;
			axis.max() -= offset;
		}
	}
};

template<typename T,size_t N>
struct Box : public BaseBox<Box<T,N>,T,N> {
	Span<T> v[N];

	using ThisType = Box<T,N>;

	INLINE Box() = default;
	constexpr INLINE Box(const ThisType& that) = default;
	constexpr INLINE Box(ThisType&& that) = default;

	template<typename S>
	explicit INLINE Box(const Box<S,N>& that) {
		for (size_t i = 0; i < N; ++i) {
			v[i][0] = T(that[i][0]);
			v[i][1] = T(that[i][1]);
		}
	}

	struct MakeEmpty {
		constexpr INLINE MakeEmpty() {}
	};
	INLINE Box(MakeEmpty) {
		for (size_t i = 0; i < N; ++i) {
			v[i].makeEmpty();
		}
	}

	INLINE Box(const Vec<T,N>& min, const Vec<T,N>& max) {
		for (size_t i = 0; i < N; ++i) {
			v[i][0] = T(min[i]);
			v[i][1] = T(max[i]);
		}
	}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;

	[[nodiscard]] constexpr INLINE Span<T>& operator[](size_t i) {
		// This static_assert needs to be in a function, because it refers to
		// ThisType, and the compiler doesn't let you reference the type that's
		// currently being compiled from class scope.
		static_assert(std::is_pod<ThisType>::value || !std::is_pod<T>::value, "Box should be a POD type if T is.");

		return v[i];
	}
	[[nodiscard]] constexpr INLINE const Span<T>& operator[](size_t i) const {
		return v[i];
	}

	constexpr INLINE void swap(ThisType& that) {
		ThisType other(*this);
		*this = that;
		that = other;
	}
};

// Specialize Box for N=2, so that initialization constructors can be constexpr,
// and to add any additional functions.
template<typename T>
struct Box<T,2> : public BaseBox<Box<T,2>,T,2> {
	Span<T> v[2];

private:
	static constexpr size_t N = 2;
public:
	// NOTE: Visual C++ 2017 has issues with the defaulted functions if
	// ThisType is declared as Box<T,N>, instead of Box<T,2>.
	using ThisType = Box<T,2>;

	INLINE Box() = default;
	constexpr INLINE Box(const ThisType& that) = default;
	constexpr INLINE Box(ThisType&& that) = default;

	template<typename S>
	explicit constexpr INLINE Box(const Box<S,N>& that) :
		v{
			{T(that[0][0]), T(that[0][1])},
			{T(that[1][0]), T(that[1][1])}
		}
	{}

	struct MakeEmpty {
		constexpr INLINE MakeEmpty() {}
	};
	constexpr INLINE Box(MakeEmpty) :
		v{
			Span<T>(Span<T>::MakeEmpty()),
			Span<T>(Span<T>::MakeEmpty())
		}
	{}

	constexpr INLINE Box(const Vec<T,2>& min, const Vec<T,2>& max) :
		v{
			{min[0], max[0]},
			{min[1], max[1]}
		}
	{}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;

	[[nodiscard]] constexpr INLINE Span<T>& operator[](size_t i) {
		// This static_assert needs to be in a function, because it refers to
		// ThisType, and the compiler doesn't let you reference the type that's
		// currently being compiled from class scope.
		static_assert(std::is_pod<ThisType>::value || !std::is_pod<T>::value, "Box should be a POD type if T is.");

		return v[i];
	}
	[[nodiscard]] constexpr INLINE const Span<T>& operator[](size_t i) const {
		return v[i];
	}

	constexpr INLINE void swap(ThisType& that) {
		ThisType other(*this);
		*this = that;
		that = other;
	}
};

// Specialize Box for N=3, so that initialization constructors can be constexpr,
// and to add any additional functions.
template<typename T>
struct Box<T,3> : public BaseBox<Box<T,3>,T,3> {
	Span<T> v[3];

private:
	static constexpr size_t N = 3;
public:
	// NOTE: Visual C++ 2017 has issues with the defaulted functions if
	// ThisType is declared as Box<T,N>, instead of Box<T,3>.
	using ThisType = Box<T,3>;

	INLINE Box() = default;
	constexpr INLINE Box(const ThisType& that) = default;
	constexpr INLINE Box(ThisType&& that) = default;

	template<typename S>
	explicit constexpr INLINE Box(const Box<S,N>& that) :
		v{
			{T(that[0][0]), T(that[0][1])},
			{T(that[1][0]), T(that[1][1])},
			{T(that[2][0]), T(that[2][1])}
		}
	{}

	struct MakeEmpty {
		constexpr INLINE MakeEmpty() {}
	};
	constexpr INLINE Box(MakeEmpty) :
		v{
			Span<T>(Span<T>::MakeEmpty()),
			Span<T>(Span<T>::MakeEmpty()),
			Span<T>(Span<T>::MakeEmpty())
		}
	{}

	constexpr INLINE Box(const Vec<T,3>& min, const Vec<T,3>& max) :
		v{
			{min[0], max[0]},
			{min[1], max[1]},
			{min[2], max[2]}
		}
	{}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;

	[[nodiscard]] constexpr INLINE Span<T>& operator[](size_t i) {
		// This static_assert needs to be in a function, because it refers to
		// ThisType, and the compiler doesn't let you reference the type that's
		// currently being compiled from class scope.
		static_assert(std::is_pod<ThisType>::value || !std::is_pod<T>::value, "Box should be a POD type if T is.");

		return v[i];
	}
	[[nodiscard]] constexpr INLINE const Span<T>& operator[](size_t i) const {
		return v[i];
	}

	constexpr INLINE void swap(ThisType& that) {
		ThisType other(*this);
		*this = that;
		that = other;
	}
};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
