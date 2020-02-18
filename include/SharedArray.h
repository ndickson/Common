#pragma once

// This file declares the class SharedArray, an array class where the contents
// are reference-counted copy-on-write.
// Implementations of functions requiring calls to malloc, free, or realloc
// are in SharedArrayDef.h, so to call them, you must include SharedArrayDef.h, too.

#include "ArrayUtil.h"
#include "Types.h"
#include <atomic>
#include <type_traits>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

class EmptyClassType {};

template<typename T, typename EXTRA_T = EmptyClassType>
class SharedArray {
protected:
	struct SharedArrayHeaderBase {
		std::atomic<size_t> refCount;
		size_t size;
	};

	struct alignas(alignof(T) > alignof(size_t) ? alignof(T) : alignof(size_t))
		SharedArrayHeader : public SharedArrayHeaderBase, public EXTRA_T {
		using SharedArrayHeaderBase::refCount;
		using SharedArrayHeaderBase::size;
		INLINE void incRef();
		inline void decRef();
	};

	SharedArrayHeader* data_;

public:
	constexpr INLINE SharedArray() : data_(nullptr) {}

	// Move constructor
	// This will take ownership of the memory owned by that.
	constexpr INLINE SharedArray(SharedArray<T,EXTRA_T>&& that) : data_(that.data_) {
		that.data_ = nullptr;
	}

	// Copy constructor
	// This is marked explicit to reduce accidental array copying,
	// even though this array type is reference counted.
	explicit inline SharedArray(const SharedArray<T,EXTRA_T>& that);

	// The destructor destructs data_, whose destructor is in SharedArrayDef.h,
	// so this destructor is also in SharedArrayDef.h
	inline ~SharedArray();

	// Move assignment operator
	inline SharedArray<T,EXTRA_T>& operator=(SharedArray<T,EXTRA_T>&& that);
	// Copy assignment operator
	inline SharedArray<T,EXTRA_T>& operator=(const SharedArray<T,EXTRA_T>& that);

	// If the reference count is 2 or greater, this copies the array
	// to make the reference count 1, so that it's safe to write to
	// the array.
	[[nodiscard]] constexpr INLINE T* unshare();

	[[nodiscard]] constexpr INLINE const T* data() const {
		return data_ ? reinterpret_cast<const T*>(data_+1) : nullptr;
	}
	[[nodiscard]] constexpr INLINE const EXTRA_T* extraData() const {
		return data_ ? static_cast<const EXTRA_T*>(data_) : nullptr;
	}
	[[nodiscard]] constexpr INLINE size_t size() const {
		return data_ ? data_->size : 0;
	}
};

// SharedArray does not contain self-pointers, so can be realloc'd.
template<typename T, typename EXTRA_T>
struct is_trivially_relocatable<SharedArray<T,EXTRA_T>> : public std::true_type {};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
