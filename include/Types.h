#pragma once

#include <stdint.h>

#define OUTER_NAMESPACE     ndickson
#define LIBRARY_NAMESPACE   Common
#define OUTER_NAMESPACE_START   namespace OUTER_NAMESPACE {
#define OUTER_NAMESPACE_END     }
#define LIBRARY_NAMESPACE_START namespace LIBRARY_NAMESPACE {
#define LIBRARY_NAMESPACE_END   }

OUTER_NAMESPACE_START
LIBRARY_NAMESPACE_START

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

// Forward declarations
template<typename SUBCLASS,typename T,size_t N>
struct BaseVec;
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

LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END

#ifdef _WIN32
#define INLINE __forceinline
#else
#define INLINE __attribute__((always_inline))
#endif
