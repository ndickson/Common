#pragma once

// This file declares common types and macros for use in other files.

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

#define OUTER_NAMESPACE             ndickson
#define COMMON_LIBRARY_NAMESPACE    Common
#define OUTER_NAMESPACE_BEGIN   namespace OUTER_NAMESPACE {
#define OUTER_NAMESPACE_END     }
#define COMMON_LIBRARY_NAMESPACE_BEGIN namespace COMMON_LIBRARY_NAMESPACE {
#define COMMON_LIBRARY_NAMESPACE_END   }

#ifdef _WIN32
#define INLINE __forceinline
#else
#define INLINE __attribute__((always_inline))
#endif

#if BUILDING_COMMON_LIBRARY && HAVE_VISIBILITY
#define COMMON_LIBRARY_EXPORTED __attribute__((__visibility__("default")))
#elif BUILDING_COMMON_LIBRARY && defined(_MSC_VER)
#define COMMON_LIBRARY_EXPORTED __declspec(dllexport)
#elif defined(_MSC_VER)
#define COMMON_LIBRARY_EXPORTED __declspec(dllimport)
#else
#define COMMON_LIBRARY_EXPORTED
#endif

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

// Forward declarations

template<typename T,size_t N>
struct Vec;

template<typename T>
using Vec2 = Vec<T,2>;
template<typename T>
using Vec3 = Vec<T,3>;
template<typename T>
using Vec4 = Vec<T,4>;

using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;
using Vec2i = Vec2<int32>;
using Vec2I = Vec2<int64>;
using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;
using Vec3i = Vec3<int32>;
using Vec3I = Vec3<int64>;
using Vec4f = Vec4<float>;
using Vec4d = Vec4<double>;
using Vec4i = Vec4<int32>;
using Vec4I = Vec4<int64>;


template<typename T,size_t NROWS,size_t NCOLS=NROWS,bool ROW_MAJOR=true>
struct Mat;

template<typename T>
using Mat2 = Mat<T,2,2>;
template<typename T>
using Mat3 = Mat<T,3,3>;

using Mat2f = Mat2<float>;
using Mat2d = Mat2<double>;
using Mat3f = Mat3<float>;
using Mat3d = Mat3<double>;


template<typename T>
struct Span;

using Spanf = Span<float>;
using Spand = Span<double>;
using Spani = Span<int32>;
using SpanI = Span<int64>;


template<typename T,size_t N>
struct Box;

template<typename T>
using Box2 = Box<T,2>;
template<typename T>
using Box3 = Box<T,3>;

using Box2f = Box2<float>;
using Box2d = Box2<double>;
using Box2i = Box2<int32>;
using Box2I = Box2<int64>;
using Box3f = Box3<float>;
using Box3d = Box3<double>;
using Box3i = Box3<int32>;
using Box3I = Box3<int64>;

template<typename T>
class ArrayPtr;

template<typename T>
class Array;
template<typename T, size_t BUF_N>
class BufArray;

template<typename T>
class Queue;
template<typename T, size_t BUF_N>
class BufQueue;

// Specialize this for types that can be realloc'd, but cannot be memcpy'd.
// It's primarily types that contain pointers to within their own direct memory
// that can't be realloc'd, (e.g. BufArray), but not many types have this
// property, and realloc can save having to separately move-assign data.
template<typename T>
struct is_trivially_relocatable : public std::is_trivially_copyable<T> {};

// std::abs isn't constexpr yet, unfortunately.
template<typename T>
[[nodiscard]] constexpr INLINE T abs(T v) {
	return (v < 0) ? -v : v;
}

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

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
