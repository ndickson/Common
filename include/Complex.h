#pragma once

// This file contains a generic complex number implementation,
// as well as corresponding conjugate and magnitude2 functions,
// enabling use as types in Vec and Mat.

#include "Types.h"

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

template<typename T>
struct Complex {
	T v[2];

	using ThisType = Complex<T>;

	INLINE Complex() = default;
	constexpr INLINE Complex(const ThisType& that) = default;
	constexpr INLINE Complex(ThisType&& that) = default;

	template<typename S>
	constexpr explicit INLINE Complex(const Complex<S>& that) : v{T(that[0]), T(that[1])} {}

	template<typename S>
	constexpr INLINE Complex(S real, S imag) : v{T(real), T(imag)} {}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;

	// Assign from real number
	template<typename S>
	constexpr INLINE ThisType& operator=(S that) {
		v[0] = that;
		v[1] = T(0);
	}

	[[nodiscard]] constexpr INLINE const T& operator[](size_t i) const {
		return v[i];
	}
	[[nodiscard]] constexpr INLINE T& operator[](size_t i) {
		return v[i];
	}

	template<typename S>
	[[nodiscard]] constexpr INLINE bool operator==(const Complex<S>& that) const {
		return (v[0] == that[0]) && (v[1] == that[1]);
	}
	template<typename S>
	[[nodiscard]] constexpr INLINE bool operator!=(const Complex<S>& that) const {
		return !(*this == that);
	}
	template<typename S>
	[[nodiscard]] constexpr INLINE bool operator==(S that) const {
		return (v[0] == that) && (v[1] == T(0));
	}
	template<typename S>
	[[nodiscard]] constexpr INLINE bool operator!=(S that) const {
		return !(*this == that);
	}

	[[nodiscard]] constexpr INLINE const T& real() const {
		return v[0];
	}
	[[nodiscard]] constexpr INLINE T& real() {
		return v[0];
	}
	[[nodiscard]] constexpr INLINE const T& imag() const {
		return v[1];
	}
	[[nodiscard]] constexpr INLINE T& imag() {
		return v[1];
	}
};

template<typename T>
[[nodiscard]] constexpr INLINE Complex<T> conjugate(const Complex<T>& c) {
	return Complex<T>(c[0], -c[1]);
}

template<typename T>
[[nodiscard]] constexpr INLINE auto magnitude2(const Complex<T>& c) {
	return magnitude2(c[0]) + magnitude2(c[1]);
}

// +Complex (unary plus operator)
template<typename T>
[[nodiscard]] constexpr INLINE const Complex<T>& operator+(const Complex<T>& c) {
	return c;
}
// -Complex (unary negation operator)
template<typename T>
[[nodiscard]] constexpr INLINE Complex<decltype(-T())> operator-(const Complex<T>& c) {
	return Complex<decltype(-T())>(-c[0], -c[1]);
}

// Complex + real
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(T()+S())> operator+(const Complex<T>& c, S real) {
	using TS = decltype(T()+S());
	return Complex<TS>(c[0] + real, TS(c[1]));
}
// Complex - real
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(T()-S())> operator-(const Complex<T>& c, S real) {
	using TS = decltype(T()-S());
	return Complex<TS>(c[0] - real, TS(c[1]));
}
// Complex * real
// NOTE: This must come before real / Complex, since that uses this!
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(T()*S())> operator*(const Complex<T>& c, S real) {
	using TS = decltype(T()*S());
	return Complex<TS>(c[0]*real, c[1]*real);
}
// Complex / real
// NOTE: This must come before Complex / Complex, since that uses this!
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(T()/S())> operator/(const Complex<T>& c, S real) {
	using TS = decltype(T()/S());
	return Complex<TS>(c[0]/real, c[1]/real);
}

// real + Complex
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(S()+T())> operator+(S real, const Complex<T>& c) {
	using TS = decltype(S()+T());
	return Complex<TS>(real + c[0], TS(c[1]));
}
// real - Complex
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(S()-T())> operator-(S real, const Complex<T>& c) {
	using TS = decltype(S()-T());
	return Complex<TS>(real - c[0], -TS(c[1]));
}
// real * Complex
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(S()*T())> operator*(S real, const Complex<T>& c) {
	using TS = decltype(S()*T());
	return Complex<TS>(real*c[0], real*c[1]);
}
// real / Complex
// NOTE: This must come after the declaration of Complex * real, since this uses it!
template<typename T,typename S>
[[nodiscard]] constexpr INLINE auto operator/(S real, const Complex<T>& c) {
	return conjugate(c)*(real / magnitude2(c));
}

// Complex + Complex
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(T()+S())> operator+(const Complex<T>& c0, const Complex<S>& c1) {
	using TS = decltype(T()+S());
	return Complex<TS>(c0[0] + c1[0], c0[1] + c1[1]);
}
// Complex - Complex
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(T()-S())> operator-(const Complex<T>& c0, const Complex<S>& c1) {
	using TS = decltype(T()-S());
	return Complex<TS>(c0[0] - c1[0], c0[1] - c1[1]);
}
// Complex * Complex
template<typename T,typename S>
[[nodiscard]] constexpr INLINE Complex<decltype(T()*S())> operator*(const Complex<T>& c0, const Complex<S>& c1) {
	using TS = decltype(T()*S());
	return Complex<TS>(c0[0]*c1[0] - c0[1]*c1[1], c0[1]*c1[0] + c0[0]*c1[1]);
}
// Complex / Complex
// NOTE: This must come after the declaration of Complex / real, since this uses it!
template<typename T,typename S>
[[nodiscard]] constexpr INLINE auto operator/(const Complex<T>& c0, const Complex<S>& c1) {
	auto denom2 = magnitude2(c1);
	return (c0*conjugate(c1))/denom2;
}

// Complex += Complex
template<typename T,typename S>
constexpr INLINE Complex<T>& operator+=(Complex<T>& c0, const Complex<S>& c1) {
	c0[0] += c1[0];
	c0[1] += c1[1];
	return c0;
}
// Complex -= Complex
template<typename T,typename S>
constexpr INLINE Complex<T>& operator-=(Complex<T>& c0, const Complex<S>& c1) {
	c0[0] -= c1[0];
	c0[1] -= c1[1];
	return c0;
}
// Complex *= Complex
template<typename T,typename S>
constexpr INLINE Complex<T>& operator*=(Complex<T>& c0, const Complex<S>& c1) {
	c0 = c0*c1;
	return c0;
}
// Complex /= Complex
template<typename T,typename S>
constexpr INLINE Complex<T>& operator/=(Complex<T>& c0, const Complex<S>& c1) {
	c0 = c0/c1;
	return c0;
}

// Complex += real
template<typename T,typename S>
constexpr INLINE Complex<T>& operator+=(Complex<T>& c, S real) {
	c[0] += real;
	return c;
}
// Complex -= real
template<typename T,typename S>
constexpr INLINE Complex<T>& operator-=(Complex<T>& c, S real) {
	c[0] -= real;
	return c;
}
// Complex *= real
template<typename T,typename S>
constexpr INLINE Complex<T>& operator*=(Complex<T>& c, S real) {
	c[0] *= real;
	c[1] *= real;
	return c;
}
// Complex /= real
template<typename T,typename S>
constexpr INLINE Complex<T>& operator/=(Complex<T>& c, S real) {
	c[0] /= real;
	c[1] /= real;
	return c;
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
