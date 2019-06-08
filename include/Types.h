#pragma once

// This file declares common types and macros for use in other files.

#include <stdint.h>
#include <type_traits>

#define OUTER_NAMESPACE             ndickson
#define COMMON_LIBRARY_NAMESPACE    Common
#define OUTER_NAMESPACE_START   namespace OUTER_NAMESPACE {
#define OUTER_NAMESPACE_END     }
#define COMMON_LIBRARY_NAMESPACE_START namespace COMMON_LIBRARY_NAMESPACE {
#define COMMON_LIBRARY_NAMESPACE_END   }

OUTER_NAMESPACE_START
COMMON_LIBRARY_NAMESPACE_START

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

using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;
using Vec2i = Vec2<int32>;
using Vec2I = Vec2<int64>;
using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;
using Vec3i = Vec3<int32>;
using Vec3I = Vec3<int64>;


template<typename T,size_t NROWS,size_t NCOLS=NROWS,bool ROW_MAJOR=true>
struct Mat;


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

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END

#ifdef _WIN32
#define INLINE __forceinline
#else
#define INLINE __attribute__((always_inline))
#endif
