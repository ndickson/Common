#pragma once

// This file contains a generic, fixed-dimension vector class, Vec, and
// two specializations, Vec2 and Vec3.

#include "Types.h"
#include <initializer_list>
#include <math.h>
#include <type_traits>

OUTER_NAMESPACE_START
COMMON_LIBRARY_NAMESPACE_START

// These are default implementations that do nothing for float, double, or integer types.
[[nodiscard]] constexpr INLINE const float& conjugate(const float& v) {
	return v;
}
[[nodiscard]] constexpr INLINE const double& conjugate(const double& v) {
	return v;
}
[[nodiscard]] constexpr INLINE const int8& conjugate(const int8& v) {
	return v;
}
[[nodiscard]] constexpr INLINE const int16& conjugate(const int16& v) {
	return v;
}
[[nodiscard]] constexpr INLINE const int32& conjugate(const int32& v) {
	return v;
}
[[nodiscard]] constexpr INLINE const int64& conjugate(const int64& v) {
	return v;
}
[[nodiscard]] constexpr INLINE const uint8& conjugate(const uint8& v) {
	return v;
}
[[nodiscard]] constexpr INLINE const uint16& conjugate(const uint16& v) {
	return v;
}
[[nodiscard]] constexpr INLINE const uint32& conjugate(const uint32& v) {
	return v;
}
[[nodiscard]] constexpr INLINE const uint64& conjugate(const uint64& v) {
	return v;
}

[[nodiscard]] constexpr INLINE float magnitude2(const float& v) {
	return v*v;
}
[[nodiscard]] constexpr INLINE double magnitude2(const double& v) {
	return v*v;
}
[[nodiscard]] constexpr INLINE int16 magnitude2(const int8& v) {
	return int16(v)*v;
}
[[nodiscard]] constexpr INLINE int32 magnitude2(const int16& v) {
	return int32(v)*v;
}
[[nodiscard]] constexpr INLINE int64 magnitude2(const int32& v) {
	return int64(v)*v;
}
[[nodiscard]] constexpr INLINE uint64 magnitude2(const int64& v) {
	// No 128-bit integer type to avoid overflow, but we can extend the range
	// slightly with uint64.
	uint64 vp = (v < 0) ? uint64(-v) : uint64(v);
	return vp*vp;
}
[[nodiscard]] constexpr INLINE uint16 magnitude2(const uint8& v) {
	return uint16(v)*v;
}
[[nodiscard]] constexpr INLINE uint32 magnitude2(const uint16& v) {
	return uint32(v)*v;
}
[[nodiscard]] constexpr INLINE uint64 magnitude2(const uint32& v) {
	return uint64(v)*v;
}
[[nodiscard]] constexpr INLINE uint64 magnitude2(const uint64& v) {
	// No 128-bit integer type to avoid overflow.
	return v*v;
}

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

	template<typename S>
	struct BaseTypeConvert {
		using Type = typename SUBCLASS::template TypeConvert<S>::Type;
	};
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

	// The type handling on these non-assignment operators ensures that
	// precision handling is the same as if scalars were being added.
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE typename BaseTypeConvert<decltype(T()+S())>::Type operator+(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		using TS = decltype(T()+S());
		using OutVec = typename BaseTypeConvert<TS>::Type;
		// Hopefully the compiler is smart enough to avoid the redundant
		// zero-initialization; it's unfortunately necessary for this function
		// to be constexpr, in the case where TS can't be initialized from T.
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = subclass()[i] + that[i];
		}
		return out;
	}
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE typename BaseTypeConvert<decltype(T()-S())>::Type operator-(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		using TS = decltype(T()-S());
		using OutVec = typename BaseTypeConvert<TS>::Type;
		// Hopefully the compiler is smart enough to avoid the redundant
		// zero-initialization; it's unfortunately necessary for this function
		// to be constexpr, in the case where TS can't be initialized from T.
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = subclass()[i] - that[i];
		}
		return out;
	}
	constexpr INLINE void negate() {
		for (size_t i = 0; i < N; ++i) {
			subclass()[i] = -subclass()[i];
		}
	}
	// Unary plus operator (returns this unchanged)
	[[nodiscard]] constexpr INLINE SUBCLASS operator+() const {
		return subclass();
	}
	// Unary minus operator
	[[nodiscard]] constexpr INLINE SUBCLASS operator-() const {
		SUBCLASS out(subclass());
		out.negate();
		return out;
	}
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE typename BaseTypeConvert<decltype(T()*S())>::Type operator*(const BaseVec<THAT_SUBCLASS,S,N>& that) const {
		using TS = decltype(T()*S());
		using OutVec = typename BaseTypeConvert<TS>::Type;
		// Hopefully the compiler is smart enough to avoid the redundant
		// zero-initialization; it's unfortunately necessary for this function
		// to be constexpr, in the case where TS can't be initialized from T.
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = subclass()[i] * that[i];
		}
		return out;
	}
	template<typename S>
	[[nodiscard]] constexpr INLINE typename BaseTypeConvert<decltype(T()*S())>::Type operator*(S that) const {
		using TS = decltype(T()*S());
		using OutVec = typename BaseTypeConvert<TS>::Type;
		// Hopefully the compiler is smart enough to avoid the redundant
		// zero-initialization; it's unfortunately necessary for this function
		// to be constexpr, in the case where TS can't be initialized from T.
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = subclass()[i] * that;
		}
		return out;
	}

	constexpr INLINE void operator+=(const BaseType& that) {
		for (size_t i = 0; i < N; ++i) {
			subclass()[i] += that[i];
		}
	}
	constexpr INLINE void operator-=(const BaseType& that) {
		for (size_t i = 0; i < N; ++i) {
			subclass()[i] -= that[i];
		}
	}
	template<typename S>
	constexpr INLINE void operator*=(S that) {
		for (size_t i = 0; i < N; ++i) {
			subclass()[i] *= that;
		}
	}
	template<typename S>
	constexpr INLINE void operator/=(S that) {
		for (size_t i = 0; i < N; ++i) {
			subclass()[i] /= that;
		}
	}
};

// Intermediate-level class for floating-point (or related) vector types,
// or at least for vector types that have a Euclidean norm.
template<typename SUBCLASS,typename T,size_t N>
struct NormVec : public BaseVec<SUBCLASS,T,N> {
	using BaseType = BaseVec<SUBCLASS,T,N>;

	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE decltype(conjugate(T())*S()) dot(const NormVec<THAT_SUBCLASS,S,N>& that) const {
		using TS = decltype(conjugate(T())*S());
		TS sum(0);
		for (size_t i = 0; i < N; ++i) {
			TS product = conjugate(BaseType::subclass()[i])*that[i];
			sum += product;
		}
		return sum;
	}

	[[nodiscard]] constexpr INLINE decltype(magnitude2(T())) length2() const {
		using T2 = decltype(magnitude2(T()));
		T2 sum(0);
		for (size_t i = 0; i < N; ++i) {
			T2 square = magnitude2(BaseType::subclass()[i]);
			sum += square;
		}
		return sum;
	}
	[[nodiscard]] INLINE auto length() const {
		return sqrt(length2());
	}
	INLINE T makeUnit() {
		T l2 = length2();
		if (l2 == T(0) || l2 == T(1)) {
			return l2;
		}
		l2 = sqrt(l2);
		*this *= (T(1)/l2);
		return l2;
	}
	INLINE T makeLength(T length) {
		T l2 = length2();
		if (l2 == T(0)) {
			return l2;
		}
		if (l2 == T(1)) {
			*this *= length;
			return l2;
		}
		l2 = sqrt(l2);
		*this *= (length/l2);
		return l2;
	}
};

// Intermediate-level class for integer vector types, with various
// integer-specific operators, and no length(), makeUnit(), or makeLength().
template<typename SUBCLASS,typename T,size_t N>
struct IntVec : public BaseVec<SUBCLASS,T,N> {
	using BaseType = BaseVec<SUBCLASS,T,N>;

	// Dot product
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE decltype(T()*S()) dot(const IntVec<THAT_SUBCLASS,S,N>& that) const {
		using TS = decltype(T()*S());
		TS sum(0);
		for (size_t i = 0; i < N; ++i) {
			TS product = BaseType::subclass()[i]*that[i];
			sum += product;
		}
		return sum;
	}

	// Euclidean length squared
	[[nodiscard]] constexpr INLINE decltype(magnitude2(T())) length2() const {
		using T2 = decltype(magnitude2(T()));
		T2 sum(0);
		for (size_t i = 0; i < N; ++i) {
			T2 square = magnitude2(BaseType::subclass()[i]);
			sum += square;
		}
		return sum;
	}

	// Bitwise NOT operator
	[[nodiscard]] constexpr INLINE SUBCLASS operator~() const {
		SUBCLASS out(T(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = ~(BaseType::subclass()[i]);
		}
		return out;
	}

	// Vec & Vec
	[[nodiscard]] constexpr INLINE SUBCLASS operator&(const SUBCLASS& that) const {
		SUBCLASS out(T(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] & that[i];
		}
		return out;
	}
	// Vec & scalar
	template<typename S>
	[[nodiscard]] constexpr INLINE typename BaseType::template BaseTypeConvert<decltype(T()&S())>::Type operator&(S that) const {
		using TS = decltype(T()&S());
		using OutVec = typename SUBCLASS::template TypeConvert<TS>::Type;
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] & that;
		}
		return out;
	}
	// Vec & Vec
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE typename BaseType::template BaseTypeConvert<decltype(T()&S())>::Type operator&(const IntVec<THAT_SUBCLASS,S,N>& that) const {
		using TS = decltype(T()&S());
		using OutVec = typename SUBCLASS::template TypeConvert<TS>::Type;
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] & that[i];
		}
		return out;
	}
	// Vec &= Vec
	constexpr INLINE void operator&=(const SUBCLASS& that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] &= that[i];
		}
	}
	// Vec &= scalar
	template<typename S>
	constexpr INLINE void operator&=(S that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] &= that;
		}
	}
	// Vec &= Vec
	template<typename THAT_SUBCLASS,typename S>
	constexpr INLINE void operator&=(const IntVec<THAT_SUBCLASS,S,N>& that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] &= that[i];
		}
	}
	// Vec ^ Vec
	[[nodiscard]] constexpr INLINE SUBCLASS operator^(const SUBCLASS& that) const {
		SUBCLASS out(T(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] ^ that[i];
		}
		return out;
	}
	// Vec ^ scalar
	template<typename S>
	[[nodiscard]] constexpr INLINE typename BaseType::template BaseTypeConvert<decltype(T()^S())>::Type operator^(S that) const {
		using TS = decltype(T()^S());
		using OutVec = typename SUBCLASS::template TypeConvert<TS>::Type;
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] ^ that;
		}
		return out;
	}
	// Vec ^ Vec
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE typename BaseType::template BaseTypeConvert<decltype(T()^S())>::Type operator^(const IntVec<THAT_SUBCLASS,S,N>& that) const {
		using TS = decltype(T()^S());
		using OutVec = typename SUBCLASS::template TypeConvert<TS>::Type;
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] ^ that[i];
		}
		return out;
	}
	// Vec ^= Vec
	constexpr INLINE void operator^=(const SUBCLASS& that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] ^= that[i];
		}
	}
	// Vec ^= scalar
	template<typename S>
	constexpr INLINE void operator^=(S that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] ^= that;
		}
	}
	// Vec ^= Vec
	template<typename THAT_SUBCLASS,typename S>
	constexpr INLINE void operator^=(const IntVec<THAT_SUBCLASS,S,N>& that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] ^= that[i];
		}
	}
	// Vec | Vec
	[[nodiscard]] constexpr INLINE SUBCLASS operator|(const SUBCLASS& that) const {
		SUBCLASS out(T(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] | that[i];
		}
		return out;
	}
	// Vec | scalar
	template<typename S>
	[[nodiscard]] constexpr INLINE typename BaseType::template BaseTypeConvert<decltype(T()|S())>::Type operator|(S that) const {
		using TS = decltype(T()|S());
		using OutVec = typename SUBCLASS::template TypeConvert<TS>::Type;
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] | that;
		}
		return out;
	}
	// Vec | Vec
	template<typename THAT_SUBCLASS,typename S>
	[[nodiscard]] constexpr INLINE typename BaseType::template BaseTypeConvert<decltype(T()|S())>::Type operator|(const IntVec<THAT_SUBCLASS,S,N>& that) const {
		using TS = decltype(T()|S());
		using OutVec = typename SUBCLASS::template TypeConvert<TS>::Type;
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] | that[i];
		}
		return out;
	}
	// Vec |= Vec
	constexpr INLINE void operator|=(const SUBCLASS& that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] |= that[i];
		}
	}
	// Vec |= scalar
	template<typename S>
	constexpr INLINE void operator|=(S that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] |= that;
		}
	}
	// Vec |= Vec
	template<typename THAT_SUBCLASS,typename S>
	constexpr INLINE void operator|=(const IntVec<THAT_SUBCLASS,S,N>& that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] |= that[i];
		}
	}
	// Vec << scalar
	template<typename S>
	[[nodiscard]] constexpr INLINE typename BaseType::template BaseTypeConvert<decltype(T()<<S())>::Type operator<<(S that) const {
		using TS = decltype(T()<<S());
		using OutVec = typename SUBCLASS::template TypeConvert<TS>::Type;
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] << that;
		}
		return out;
	}
	// Vec <<= scalar
	template<typename S>
	constexpr INLINE void operator<<=(S that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] <<= that;
		}
	}
	// Vec >> scalar
	template<typename S>
	[[nodiscard]] constexpr INLINE typename BaseType::template BaseTypeConvert<decltype(T()>>S())>::Type operator>>(S that) const {
		using TS = decltype(T()>>S());
		using OutVec = typename SUBCLASS::template TypeConvert<TS>::Type;
		OutVec out(TS(0));
		for (size_t i = 0; i < N; ++i) {
			out[i] = BaseType::subclass()[i] >> that;
		}
		return out;
	}
	// Vec >>= scalar
	template<typename S>
	constexpr INLINE void operator>>=(S that) {
		for (size_t i = 0; i < N; ++i) {
			BaseType::subclass()[i] >>= that;
		}
	}
};

// Generic, fixed-dimension vector class
template<typename T,size_t N>
struct Vec : public std::conditional<std::is_integral<T>::value,IntVec<Vec<T,2>,T,2>,NormVec<Vec<T,N>,T,N>>::type {
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

	template<typename S>
	explicit INLINE Vec(S that) {
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

	template<typename S>
	struct TypeConvert {
		using Type = Vec<S,N>;
	};
};


// Specialize Vec for N=2, so that initialization constructors can be constexpr,
// and to add cross, perpendicular, and any other additional functions.
template<typename T>
struct Vec<T,2> : public std::conditional<std::is_integral<T>::value,IntVec<Vec<T,2>,T,2>,NormVec<Vec<T,2>,T,2>>::type {
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

	template<typename S>
	struct TypeConvert {
		using Type = Vec<S,N>;
	};
};


// Specialize Vec for N=3, so that initialization constructors can be constexpr,
// and to add cross and any other additional functions.
template<typename T>
struct Vec<T,3> : public std::conditional<std::is_integral<T>::value,IntVec<Vec<T,2>,T,2>,NormVec<Vec<T,3>,T,3>>::type {
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

	template<typename S>
	struct TypeConvert {
		using Type = Vec<S,N>;
	};
};


template<typename S,typename T,size_t N>
[[nodiscard]] constexpr INLINE Vec<decltype(S()*T()),N> operator*(S scalar, const Vec<T,N>& vector) {
	using ST = decltype(S()*T());
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Vec<ST,N> out(ST(0));
	for (size_t i = 0; i < N; ++i) {
		out[i] = scalar * vector[i];
	}
	return out;
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
