#pragma once

// This file contains a generic, fixed-dimension vector class, Vec, and
// two specializations, Vec2 and Vec3.

#include "Types.h"
#include <initializer_list>
#include <math.h>
#include <type_traits>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN


// Base class for Vec specializations, so that the specializations don't need
// to duplicate all of the common functions.
// This uses the "curiously recurring template pattern" to have the
// implementations in the base class, with the data members in the subclasses,
// so that some initialization constructors can possibly be constexpr.
// There seems to be no way for an initializer_list constructor to be constexpr.
template<typename SUBCLASS,typename T,size_t N>
struct BaseVec {
protected:
	[[nodiscard]] constexpr INLINE SUBCLASS& subclass() {
		return static_cast<SUBCLASS&>(*this);
	}
	[[nodiscard]] constexpr INLINE const SUBCLASS& subclass() const {
		return static_cast<const SUBCLASS&>(*this);
	}
public:

	using BaseType = BaseVec<SUBCLASS,T,N>;
	using ValueType = T;
	static constexpr size_t TupleSize = N;

protected:
	[[nodiscard]] constexpr INLINE T& operator[](size_t i) {
		return subclass()[i];
	}
	[[nodiscard]] constexpr INLINE const T& operator[](size_t i) const {
		return subclass()[i];
	}

	template<typename THAT_SUBCLASS,typename S,size_t M>
	friend struct BaseVec;

public:

	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE bool operator==(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		bool equal = true;
		for (size_t i = 0; i < N; ++i) {
			// This could use branches to early-exit instead, but for small vectors,
			// it's probably more efficient to do all of the comparisons,
			// instead of branching.
			equal &= (subclass()[i] == that[i]);
		}
		return equal;
	}
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE bool operator!=(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		return !(*this == that);
	}

	// These ordered comparison operators are just for sorting purposes.
	// They treat the first component as the most significant.
	// They should always return false if the first unequal component is NaN.
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE bool operator<(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		for (size_t i = 0; i < N; ++i) {
			if (subclass()[i] != that[i]) {
				return (subclass()[i] < that[i]);
			}
		}
		return false;
	}
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE bool operator<=(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		for (size_t i = 0; i < N; ++i) {
			if (subclass()[i] != that[i]) {
				return (subclass()[i] < that[i]);
			}
		}
		return true;
	}
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE bool operator>(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		for (size_t i = 0; i < N; ++i) {
			if (subclass()[i] != that[i]) {
				return (subclass()[i] > that[i]);
			}
		}
		return false;
	}
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE bool operator>=(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		for (size_t i = 0; i < N; ++i) {
			if (subclass()[i] != that[i]) {
				return (subclass()[i] > that[i]);
			}
		}
		return true;
	}

	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE decltype(conjugate(T())*S()) dot(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		using TS = decltype(conjugate(T())*S());
		TS sum(0);
		for (size_t i = 0; i < N; ++i) {
			TS product = conjugate(subclass()[i])*that[i];
			sum += product;
		}
		return sum;
	}

	[[nodiscard]] constexpr INLINE decltype(magnitude2(T())) length2() const {
		using T2 = decltype(magnitude2(T()));
		T2 sum(0);
		for (size_t i = 0; i < N; ++i) {
			const T2 square = magnitude2(subclass()[i]);
			sum += square;
		}
		return sum;
	}

	constexpr INLINE void negate() {
		for (size_t i = 0; i < N; ++i) {
			subclass()[i] = -subclass()[i];
		}
	}
};

// Intermediate-level class for floating-point (or related) vector types,
// or at least for vector types that have a Euclidean norm.
template<typename SUBCLASS,typename T,size_t N>
struct NormVec : public BaseVec<SUBCLASS,T,N> {
	using BaseType = BaseVec<SUBCLASS,T,N>;

	[[nodiscard]] INLINE auto length() const {
		return sqrt(BaseType::length2());
	}

	// These are implemented below the operators, since they use the *= operator.
	INLINE T makeUnit();
	INLINE T makeLength(T length);
};

// Generic, fixed-dimension vector class
template<typename T,size_t N>
struct Vec : public std::conditional<std::is_integral<T>::value,BaseVec<Vec<T,N>,T,N>,NormVec<Vec<T,N>,T,N>>::type {
	T v[N];

	using ThisType = Vec<T,N>;

	INLINE Vec() = default;
	constexpr INLINE Vec(const ThisType& that) = default;
	constexpr INLINE Vec(ThisType&& that) = default;

	template<typename S>
	explicit INLINE Vec(const Vec<S,N>& that) {
		for (size_t i = 0; i < N; ++i) {
			v[i] = T(that[i]);
		}
	}

	// Scalar value constructor
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	template<typename S>
	constexpr explicit INLINE Vec(S that) : v{T(0)} {
		const T value = T(that);
		for (size_t i = 0; i < N; ++i) {
			v[i] = value;
		}
	}

	template<typename S>
	Vec(std::initializer_list<S> list) {
		// TODO: Ensure that list is the correct size at compile time if possible.
		size_t n = list.size();
		if (n > N) {
			n = N;
		}
		for (size_t i = 0; i < n; ++i) {
			v[i] = T(list.begin()[i]);
		}
		for (size_t i = n; i < N; ++i) {
			v[i] = T(0);
		}
	}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;
	template<typename S>
	INLINE ThisType& operator=(std::initializer_list<S> list) {
		*this = Vec(list);
		return *this;
	}

	[[nodiscard]] constexpr INLINE T& operator[](size_t i) {
		// This static_assert needs to be in a function, because it refers to
		// ThisType, and the compiler doesn't let you reference the type that's
		// currently being compiled from class scope.
		static_assert(std::is_pod<ThisType>::value || !std::is_pod<T>::value, "Vec should be a POD type if T is.");

		return v[i];
	}
	[[nodiscard]] constexpr INLINE const T& operator[](size_t i) const {
		return v[i];
	}

	[[nodiscard]] constexpr INLINE T* data() {
		return v;
	}
	[[nodiscard]] constexpr INLINE const T* data() const {
		return v;
	}

	constexpr INLINE void swap(ThisType& that) {
		ThisType other(*this);
		*this = that;
		that = other;
	}
};


// Specialize Vec for N=2, so that initialization constructors can be constexpr,
// and to add cross, perpendicular, and any other additional functions.
template<typename T>
struct Vec<T,2> : public std::conditional<std::is_integral<T>::value,BaseVec<Vec<T,2>,T,2>,NormVec<Vec<T,2>,T,2>>::type {
	T v[2];

private:
	static constexpr size_t N = 2;
public:
	// NOTE: Visual C++ 2017 has issues with the defaulted functions if
	// ThisType is declared as Vec<T,N>, instead of Vec<T,2>.
	using ThisType = Vec<T,2>;

	INLINE Vec() = default;
	constexpr INLINE Vec(const ThisType& that) = default;
	constexpr INLINE Vec(ThisType&& that) = default;

	template<typename S>
	explicit constexpr INLINE Vec(const Vec<S,N>& that) : v{T(that[0]), T(that[1])} {}

	template<typename S>
	explicit constexpr INLINE Vec(S that) : v{T(that), T(that)} {}

	template<typename S>
	constexpr INLINE Vec(S v0, S v1) : v{T(v0), T(v1)} {}

	template<typename S>
	Vec(std::initializer_list<S> list) {
		// TODO: Ensure that list is the correct size at compile time if possible.
		size_t n = list.size();
		if (n > N) {
			n = N;
		}
		for (size_t i = 0; i < n; ++i) {
			v[i] = T(list.begin()[i]);
		}
		for (size_t i = n; i < N; ++i) {
			v[i] = T(0);
		}
	}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;
	template<typename S>
	INLINE ThisType& operator=(std::initializer_list<S> list) {
		*this = Vec(list);
		return *this;
	}

	[[nodiscard]] constexpr INLINE T& operator[](size_t i) {
		// This static_assert needs to be in a function, because it refers to
		// ThisType, and the compiler doesn't let you reference the type that's
		// currently being compiled from class scope.
		static_assert(std::is_pod<ThisType>::value || !std::is_pod<T>::value, "Vec should be a POD type if T is.");

		return v[i];
	}
	[[nodiscard]] constexpr INLINE const T& operator[](size_t i) const {
		return v[i];
	}

	[[nodiscard]] constexpr INLINE T* data() {
		return v;
	}
	[[nodiscard]] constexpr INLINE const T* data() const {
		return v;
	}

	constexpr INLINE void swap(ThisType& that) {
		ThisType other(*this);
		*this = that;
		that = other;
	}

	[[nodiscard]] constexpr INLINE decltype(T()*T()) cross(const ThisType& that) const {
		return v[0]*that[1] - v[1]*that[0];
	}

	// Returns the vector that's rotated a positive quarter turn.
	[[nodiscard]] constexpr INLINE ThisType perpendicular() const {
		return ThisType(-v[1], v[0]);
	}
};


// Specialize Vec for N=3, so that initialization constructors can be constexpr,
// and to add cross and any other additional functions.
template<typename T>
struct Vec<T,3> : public std::conditional<std::is_integral<T>::value,BaseVec<Vec<T,3>,T,3>,NormVec<Vec<T,3>,T,3>>::type {
	T v[3];

private:
	static constexpr size_t N = 3;
public:
	// NOTE: Visual C++ 2017 has issues with the defaulted functions if
	// ThisType is declared as Vec<T,N>, instead of Vec<T,3>.
	using ThisType = Vec<T,3>;

	INLINE Vec() = default;
	constexpr INLINE Vec(const ThisType& that) = default;
	constexpr INLINE Vec(ThisType&& that) = default;

	template<typename S>
	explicit constexpr INLINE Vec(const Vec<S,N>& that) : v{T(that[0]), T(that[1]), T(that[2])} {}

	template<typename S>
	explicit constexpr INLINE Vec(S that) : v{T(that), T(that), T(that)} {}

	template<typename S>
	constexpr INLINE Vec(S v0, S v1, S v2) : v{T(v0), T(v1), T(v2)} {}

	template<typename S>
	Vec(std::initializer_list<S> list) {
		// TODO: Ensure that list is the correct size at compile time if possible.
		size_t n = list.size();
		if (n > N) {
			n = N;
		}
		for (size_t i = 0; i < n; ++i) {
			v[i] = T(list.begin()[i]);
		}
		for (size_t i = n; i < N; ++i) {
			v[i] = T(0);
		}
	}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;
	template<typename S>
	INLINE ThisType& operator=(std::initializer_list<S> list) {
		*this = Vec(list);
		return *this;
	}

	[[nodiscard]] constexpr INLINE T& operator[](size_t i) {
		// This static_assert needs to be in a function, because it refers to
		// ThisType, and the compiler doesn't let you reference the type that's
		// currently being compiled from class scope.
		static_assert(std::is_pod<ThisType>::value || !std::is_pod<T>::value, "Vec should be a POD type if T is.");

		return v[i];
	}
	[[nodiscard]] constexpr INLINE const T& operator[](size_t i) const {
		return v[i];
	}

	[[nodiscard]] constexpr INLINE T* data() {
		return v;
	}
	[[nodiscard]] constexpr INLINE const T* data() const {
		return v;
	}

	constexpr INLINE void swap(ThisType& that) {
		ThisType other(*this);
		*this = that;
		that = other;
	}

	[[nodiscard]] constexpr INLINE Vec<decltype(T()*T()),N> cross(const ThisType& that) const {
		return Vec<decltype(T()*T()),N>(
			v[1]*that[2] - v[2]*that[1],
			v[2]*that[0] - v[0]*that[2],
			v[0]*that[1] - v[1]*that[0]
		);
	}
};


// Specialize Vec for N=4, so that initialization constructors can be constexpr.
template<typename T>
struct Vec<T,4> : public std::conditional<std::is_integral<T>::value,BaseVec<Vec<T,4>,T,4>,NormVec<Vec<T,4>,T,4>>::type {
	T v[4];

private:
	static constexpr size_t N = 4;
public:
	// NOTE: Visual C++ 2017 has issues with the defaulted functions if
	// ThisType is declared as Vec<T,N>, instead of Vec<T,4>.
	using ThisType = Vec<T,4>;

	INLINE Vec() = default;
	constexpr INLINE Vec(const ThisType& that) = default;
	constexpr INLINE Vec(ThisType&& that) = default;

	template<typename S>
	explicit constexpr INLINE Vec(const Vec<S,N>& that) : v{T(that[0]), T(that[1]), T(that[2]), T(that[3])} {}

	template<typename S>
	explicit constexpr INLINE Vec(S that) : v{T(that), T(that), T(that), T(that)} {}

	template<typename S>
	constexpr INLINE Vec(S v0, S v1, S v2, S v3) : v{T(v0), T(v1), T(v2), T(v3)} {}

	template<typename S>
	Vec(std::initializer_list<S> list) {
		// TODO: Ensure that list is the correct size at compile time if possible.
		size_t n = list.size();
		if (n > N) {
			n = N;
		}
		for (size_t i = 0; i < n; ++i) {
			v[i] = T(list.begin()[i]);
		}
		for (size_t i = n; i < N; ++i) {
			v[i] = T(0);
		}
	}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;
	template<typename S>
	INLINE ThisType& operator=(std::initializer_list<S> list) {
		*this = Vec(list);
		return *this;
	}

	[[nodiscard]] constexpr INLINE T& operator[](size_t i) {
		// This static_assert needs to be in a function, because it refers to
		// ThisType, and the compiler doesn't let you reference the type that's
		// currently being compiled from class scope.
		static_assert(std::is_pod<ThisType>::value || !std::is_pod<T>::value, "Vec should be a POD type if T is.");

		return v[i];
	}
	[[nodiscard]] constexpr INLINE const T& operator[](size_t i) const {
		return v[i];
	}

	[[nodiscard]] constexpr INLINE T* data() {
		return v;
	}
	[[nodiscard]] constexpr INLINE const T* data() const {
		return v;
	}

	constexpr INLINE void swap(ThisType& that) {
		ThisType other(*this);
		*this = that;
		that = other;
	}
};


// +Vec (unary plus operator)
template<typename T,size_t N>
[[nodiscard]] constexpr INLINE const Vec<T,N>& operator+(const Vec<T,N>& vector) {
	return vector;
}
// -Vec (unary negation operator)
template<typename T,size_t N>
[[nodiscard]] constexpr INLINE Vec<decltype(-T()),N> operator-(const Vec<T,N>& vector) {
	using TS = decltype(-T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = -vector[i];
	}
	return out;
}

// Vec + Vec
template<typename T,size_t N,typename S>
[[nodiscard]] constexpr INLINE Vec<decltype(T()+S()),N> operator+(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()+S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] + vector1[i];
	}
	return out;
}
// Vec - Vec
template<typename T,size_t N,typename S>
[[nodiscard]] constexpr INLINE Vec<decltype(T()-S()),N> operator-(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()-S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] - vector1[i];
	}
	return out;
}
// Vec * Vec
template<typename T,size_t N,typename S>
[[nodiscard]] constexpr INLINE Vec<decltype(T()*S()),N> operator*(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()*S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] * vector1[i];
	}
	return out;
}
// Vec / Vec
template<typename T,size_t N,typename S>
[[nodiscard]] constexpr INLINE Vec<decltype(T()/S()),N> operator/(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()/S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] / vector1[i];
	}
	return out;
}

// Vec += Vec
template<typename T,size_t N,typename S>
constexpr INLINE Vec<T,N>& operator+=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] += vector1[i];
	}
	return vector0;
}
// Vec -= Vec
template<typename T,size_t N,typename S>
constexpr INLINE Vec<T,N>& operator-=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] -= vector1[i];
	}
	return vector0;
}
// Vec *= Vec
template<typename T,size_t N,typename S>
constexpr INLINE Vec<T,N>& operator*=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] *= vector1[i];
	}
	return vector0;
}
// Vec /= Vec
template<typename T,size_t N,typename S>
constexpr INLINE Vec<T,N>& operator/=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] /= vector1[i];
	}
	return vector0;
}

// Vec + scalar
template<typename T,size_t N,typename S>
[[nodiscard]] constexpr INLINE Vec<decltype(T()+S()),N> operator+(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()+S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] + scalar;
	}
	return out;
}
// Vec - scalar
template<typename T,size_t N,typename S>
[[nodiscard]] constexpr INLINE Vec<decltype(T()-S()),N> operator-(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()-S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] - scalar;
	}
	return out;
}
// Vec * scalar
template<typename T,size_t N,typename S>
[[nodiscard]] constexpr INLINE Vec<decltype(T()*S()),N> operator*(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()*S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] * scalar;
	}
	return out;
}
// Vec / scalar
template<typename T,size_t N,typename S>
[[nodiscard]] constexpr INLINE Vec<decltype(T()/S()),N> operator/(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()/S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] / scalar;
	}
	return out;
}

// Vec += scalar
template<typename T,size_t N,typename S>
constexpr INLINE Vec<T,N>& operator+=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] += scalar;
	}
	return vector;
}
// Vec -= scalar
template<typename T,size_t N,typename S>
constexpr INLINE Vec<T,N>& operator-=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] -= scalar;
	}
	return vector;
}
// Vec *= scalar
template<typename T,size_t N,typename S>
constexpr INLINE Vec<T,N>& operator*=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] *= scalar;
	}
	return vector;
}
// Vec /= scalar
template<typename T,size_t N,typename S>
constexpr INLINE Vec<T,N>& operator/=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] /= scalar;
	}
	return vector;
}

// scalar + Vec
template<typename S,typename T,size_t N>
[[nodiscard]] constexpr INLINE Vec<decltype(S()+T()),N> operator+(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()+T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar + vector[i];
	}
	return out;
}
// scalar - Vec
template<typename S,typename T,size_t N>
[[nodiscard]] constexpr INLINE Vec<decltype(S()-T()),N> operator-(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()-T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar - vector[i];
	}
	return out;
}
// scalar * Vec
template<typename S,typename T,size_t N>
[[nodiscard]] constexpr INLINE Vec<decltype(S()*T()),N> operator*(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()*T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar * vector[i];
	}
	return out;
}
// scalar / Vec
template<typename S,typename T,size_t N>
[[nodiscard]] constexpr INLINE Vec<decltype(S()/T()),N> operator/(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()/T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar / vector[i];
	}
	return out;
}

// Integer-only operations

// ~Vec (bitwise NOT operator)
template<typename T,size_t N, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<T,N> operator~(const Vec<T,N>& vector) {
	using TS = T;
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = ~vector[i];
	}
	return out;
}

// Vec & Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()&S()),N> operator&(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()&S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] & vector1[i];
	}
	return out;
}
// Vec ^ Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()^S()),N> operator^(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()^S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] ^ vector1[i];
	}
	return out;
}
// Vec | Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()|S()),N> operator|(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()|S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] | vector1[i];
	}
	return out;
}
// Vec << Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()<<S()),N> operator<<(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()<<S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] << vector1[i];
	}
	return out;
}
// Vec >> Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()>>S()),N> operator>>(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()>>S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] >> vector1[i];
	}
	return out;
}
// Vec % Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()%S()),N> operator%(const Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	using TS = decltype(T()%S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector0[i] % vector1[i];
	}
	return out;
}
// Vec &= Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator&=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] &= vector1[i];
	}
	return vector0;
}
// Vec ^= Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator^=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] ^= vector1[i];
	}
	return vector0;
}
// Vec |= Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator|=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] |= vector1[i];
	}
	return vector0;
}
// Vec <<= Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator<<=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] <<= vector1[i];
	}
	return vector0;
}
// Vec >>= Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator>>=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] >>= vector1[i];
	}
	return vector0;
}
// Vec %= Vec
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator%=(Vec<T,N>& vector0, const Vec<S,N>& vector1) {
	for (size_t i = 0; i < N; ++i) {
		vector0[i] %= vector1[i];
	}
	return vector0;
}

// Vec & scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()&S()),N> operator&(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()&S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] & scalar;
	}
	return out;
}
// Vec ^ scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()^S()),N> operator^(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()^S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] ^ scalar;
	}
	return out;
}
// Vec | scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()|S()),N> operator|(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()|S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] | scalar;
	}
	return out;
}
// Vec << scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()<<S()),N> operator<<(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()<<S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] << scalar;
	}
	return out;
}
// Vec >> scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()>>S()),N> operator>>(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()>>S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] >> scalar;
	}
	return out;
}
// Vec % scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(T()%S()),N> operator%(const Vec<T,N>& vector, S scalar) {
	using TS = decltype(T()%S());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = vector[i] % scalar;
	}
	return out;
}
// Vec &= scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator&=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] &= scalar;
	}
	return vector;
}
// Vec ^= scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator^=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] ^= scalar;
	}
	return vector;
}
// Vec |= scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator|=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] |= scalar;
	}
	return vector;
}
// Vec <<= scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator<<=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] <<= scalar;
	}
	return vector;
}
// Vec >>= scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator>>=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] >>= scalar;
	}
	return vector;
}
// Vec %= scalar
template<typename T,size_t N,typename S, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
constexpr INLINE Vec<T,N>& operator%=(Vec<T,N>& vector, S scalar) {
	for (size_t i = 0; i < N; ++i) {
		vector[i] %= scalar;
	}
	return vector;
}

// scalar & Vec
template<typename S,typename T,size_t N, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(S()&T()),N> operator&(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()&T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar & vector[i];
	}
	return out;
}
// scalar ^ Vec
template<typename S,typename T,size_t N, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(S()^T()),N> operator^(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()^T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar ^ vector[i];
	}
	return out;
}
// scalar | Vec
template<typename S,typename T,size_t N, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(S()|T()),N> operator|(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()|T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar | vector[i];
	}
	return out;
}
// scalar << Vec
template<typename S,typename T,size_t N, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(S()<<T()),N> operator<<(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()<<T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar << vector[i];
	}
	return out;
}
// scalar >> Vec
template<typename S,typename T,size_t N, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(S()>>T()),N> operator>>(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()>>T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar >> vector[i];
	}
	return out;
}
// scalar % Vec
template<typename S,typename T,size_t N, typename std::enable_if<std::is_integral<T>::value && std::is_integral<S>::value>::type* = nullptr>
[[nodiscard]] constexpr INLINE Vec<decltype(S()%T()),N> operator%(S scalar, const Vec<T,N>& vector) {
	using TS = decltype(S()%T());
	using OutVec = Vec<TS,N>;
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	OutVec out(TS(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar % vector[i];
	}
	return out;
}

template<typename SUBCLASS,typename T,size_t N>
INLINE T NormVec<SUBCLASS,T,N>::makeUnit() {
	T l2 = BaseType::length2();
	T factor;
	if (l2 != T(0) && l2 != T(1)) {
		l2 = sqrt(l2);
		factor = (T(1)/l2);
	}
	else {
		// If length squared is zero due to underflow, the vector might still be
		// of small-but-non-zero length, so still multiply by zero, to ensure
		// that it's exactly zero, for robustness.
		factor = l2;
	}
	*(SUBCLASS*)this *= factor;
	return l2;
}
template<typename SUBCLASS,typename T,size_t N>
INLINE T NormVec<SUBCLASS,T,N>::makeLength(T length) {
	T l2 = BaseType::length2();
	T factor;
	if (l2 != T(0) && l2 != T(1)) {
		l2 = sqrt(l2);
		factor = (length/l2);
	}
	else {
		// If length squared is zero due to underflow, the vector might still be
		// of small-but-non-zero length, so still multiply by zero, to ensure
		// that it's exactly zero, for robustness.
		factor = l2*length;
	}
	*(SUBCLASS*)this *= factor;
	return l2;
}


template<typename T,size_t N>
[[nodiscard]] constexpr INLINE Vec<decltype(conjugate(T())),N> conjugate(const Vec<T,N>& v) {
	using CT = decltype(conjugate(T()));
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Vec<CT,N> out(CT(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = conjugate(v[i]);
	}
	return out;
}

template<typename T,size_t N>
[[nodiscard]] constexpr INLINE decltype(magnitude2(T())) magnitude2(const Vec<T,N>& v) {
	return v.length2();
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
