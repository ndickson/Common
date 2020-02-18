#pragma once

#include "SharedArray.h"
#include "SharedString.h"

#include <assert.h>
#include <cstdlib>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

template<typename T, typename EXTRA_T>
INLINE void SharedArray<T,EXTRA_T>::SharedArrayHeader::incRef() {
	// Atomic increment
	++refCount;
}

template<typename T, typename EXTRA_T>
inline void SharedArray<T,EXTRA_T>::SharedArrayHeader::decRef() {
	assert(refCount.load(std::memory_order_relaxed) != 0);
	size_t newCount = --refCount;
	if (newCount != 0) {
		return;
	}

	// If EXTRA_T needs destruction, destruct it.
	if constexpr (!std::is_pod<EXTRA_T>::value) {
		static_cast<EXTRA_T*>(this)->~EXTRA_T();
	}

	// Destruct array if T needs destruction.
	// Note that this is okay for SharedString, even though size isn't
	// "correct", because char doesn't need destruction.
	if constexpr (!std::is_pod<T>::value) {
		T* array = reinterpret_cast<T*>(this+1);
		for (size_t i = 0; i < size; ++i) {
			// Destruct T in place.
			(array+i)->~T();
		}
	}

	// Free the memory.
	free(this);
}

template<typename T, typename EXTRA_T>
inline SharedArray<T,EXTRA_T>::SharedArray(const SharedArray<T,EXTRA_T>& that) : data_(that.data_) {
	data_->incRef();
}

template<typename T, typename EXTRA_T>
inline SharedArray<T,EXTRA_T>::~SharedArray() {
	if (data_ != nullptr) {
		data_->decRef();
	}
}

template<typename T, typename EXTRA_T>
constexpr INLINE T* SharedArray<T,EXTRA_T>::unshare() {
	if (data_ == nullptr) {
		return nullptr;
	}
	size_t count = data_->refCount.load(std::memory_order_relaxed);
	if (count > 1) {
		const size_t size = data_->size;
		SharedArrayHeader* origData = data_;

		// Allocate a new array.
		SharedArrayHeader* newData = (SharedArrayHeader*)malloc(sizeof(SharedArrayHeader) + sizeof(T)*size);
		data_ = newData;

		// Copy header.
		newData->refCount.store(1, std::memory_order_relaxed);
		newData->size = size;
		if constexpr (sizeof(SharedArrayHeader) != sizeof(SharedArrayHeaderBase)) {
			// Copy construct EXTRA_T in place.
			new (static_cast<EXTRA_T*>(newData)) EXTRA_T(*static_cast<const EXTRA_T*>(origData));
		}

		// Copy array
		const T* oldArray = reinterpret_cast<const T*>(origData+1);
		T* newArray = reinterpret_cast<T*>(newData+1);
		copyConstructSpan(newArray, newArray+size, oldArray);

		// Remove referece, which may bring it down to zero if something else
		// removed a reference after we checked it above.
		size_t newCount = origData->refCount.add(-1);
		if (newCount == 0) {
			free(origData);
		}
	}
	return reinterpret_cast<T*>(data_+1);
}

template<typename T, typename EXTRA_T>
INLINE SharedArray<T,EXTRA_T>& SharedArray<T,EXTRA_T>::operator=(SharedArray<T,EXTRA_T>&& that) {
	if (data_ != nullptr) {
		data_->decRef();
	}
	data_ = that.data_;
	that.data_ = nullptr;
}

template<typename T, typename EXTRA_T>
INLINE SharedArray<T,EXTRA_T>& SharedArray<T,EXTRA_T>::operator=(const SharedArray<T,EXTRA_T>& that) {
	if (data_ != nullptr) {
		data_->decRef();
	}
	data_ = that.data_;
	data_->incRef();
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
