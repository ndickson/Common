#pragma once

// This file primarily contains definitions of functions for the classes
// Array and BufArray, declared in Array.h, that require malloc, free, or realloc.
// In order to call those functions, you must include this file.

#include "Array.h"
#include "Types.h"

#include <new> // For placement new operator
#include <cstdlib> // For free and realloc
#include <type_traits>
#include <iterator> // For std::distance

#ifdef _WIN32
#include <malloc.h> // For _aligned_realloc
#else
#include <string.h> // For memcpy
#endif

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

template<typename T>
T* typedRealloc(T* p, size_t newCapacity, size_t sizeToCopy) {
	size_t numBytes = newCapacity*sizeof(T);
#ifdef _WIN32
	// 64-bit Windows standard allocator is always 16-byte aligned.
	constexpr bool isDefaultOkay = (alignof(T) <= ((sizeof(void*) == 8) ? 16 : 8));
#else
	// Change this if using an allocator that doesn't guarantee 8-byte alignment.
	constexpr bool isDefaultOkay = (alignof(T) <= 8);
#endif
	if constexpr (isDefaultOkay) {
		return (T*)std::realloc(p, numBytes);
	}
	else {
#ifdef _WIN32
		return (T*)_aligned_realloc(p, numBytes, alignof(T));
#else
		T* newp = std::aligned_alloc(alignof(T), numBytes);
		if (sizeToCopy != 0) {
			assert(is_trivially_relocatable<T>::value);
			memcpy(newp, p, sizeToCopy*sizeof(T));
		}
		if (p != nullptr) {
			std::free(p);
		}
#endif
	}
}

template<typename T>
void typedFree(T* p) {
	assert(p != nullptr);
#ifdef _WIN32
	// 64-bit Windows standard allocator is always 16-byte aligned.
	constexpr bool isDefaultOkay = (alignof(T) <= ((sizeof(void*) == 8) ? 16 : 8));
#else
	constexpr bool isDefaultOkay = true;
#endif
	if constexpr (isDefaultOkay) {
		std::free(p);
	}
	else {
#ifdef _WIN32
		_aligned_free(p);
#endif
	}
}

template<typename T>
INLINE ArrayPtr<T>::~ArrayPtr() {
	if (p != nullptr) {
		typedFree(p);
	}
}

template<typename T>
INLINE void ArrayPtr<T>::reset() {
	if (p != nullptr) {
		typedFree(p);
		p = nullptr;
	}
}

template<typename T>
INLINE void ArrayPtr<T>::reset(T* thatp) {
	// Don't free if same pointer
	if (p == thatp) {
		return;
	}
	if (p != nullptr) {
		typedFree(p);
	}
	p = thatp;
}

template<typename T>
INLINE void ArrayPtr<T>::realloc(size_t newCapacity, size_t sizeToCopy) {
	if (newCapacity == 0) {
		reset();
	}
	else {
		p = typedRealloc(p, newCapacity, sizeToCopy);
	}
}

template<typename T>
Array<T>::Array(Array<T>&& that) : data_(nullptr) {
	if (that.isLocalBuffer()) {
		// Copy construct, since ownership of the buffer can't be taken.
		size_ = 0;
		capacity_ = 0;
		increaseCapacity(that.size_);
		copyConstructSpan(data(), data()+that.size_, that.data());
		size_ = that.size_;
	}
	else {
		// Take ownership of memory block from that.
		size_ = that.size_;
		capacity_ = that.capacity_;
		// NOTE: data_ is null, so we don't need to check isLocalBuffer()
		data_.reset(that.data_.release());
		that.size_ = 0;
		that.capacity_ = 0;
		if (isLocalBuffer()) {
			// Heap buffer happens to be immediately following this array.
			reallocLocalBuffer(capacity_, true);
		}
	}
}

template<typename T>
Array<T>::Array(const Array<T>& that) : data_(nullptr), size_(0), capacity_(0) {
	if (that.size_ == 0) {
		return;
	}
	// Only allocate up to that.size_
	increaseCapacity(that.size_);
	size_ = that.size_;
	T* begin = data();
	T* end = begin + that.size_;
	const T* source = that.data();
	copyConstructSpan(begin, end, list.begin());
}

template<typename T>
Array<T>::~Array() {
	if (!std::is_pod<T>::value && size_ > 0) {
		destructSpan(begin(), end());
	}
	if (isLocalBuffer()) {
		data_.release();
	}
	// data_ frees array automatically on destruction
}

template<typename T>
Array<T>& Array<T>::operator=(Array<T>&& that) {
	assert(this != &that);

	if (that.isLocalBuffer()) {
		// Copy assign, since ownership of the buffer can't be taken.
		*this = that;
	}
	else {
		if (isLocalBuffer()) {
			data_.release();
		}
		data_.reset(that.data_.release());
		size_ = that.size_;
		capacity_ = that.capacity_;
		that.size_ = 0;
		that.capacity_ = 0;
		if (isLocalBuffer()) {
			// Heap buffer happens to be immediately following this array.
			reallocLocalBuffer(capacity_, true);
		}
	}
	return *this;
}

template<typename T>
Array<T>& Array<T>::operator=(const Array<T>& that) {
	if (this == &that) {
		return *this;
	}

	// Only set capacity if growing to more than current capacity.
	if (that.size_ > capacity_) {
		// TODO: We don't care about the old data, so don't bother moving it.
		increaseCapacity(that.size_);
	}
	const bool isGrowing = (size_ < that.size_);
	const size_t smallerSize = isGrowing ? size_ : that.size_;
	// Copy assign overlap between old elements and new elements.
	copyAssignSpan(data(), data()+smallerSize, that.data());
	if (isGrowing) {
		// Copy construct new elements past old end.
		copyConstructSpan(data()+smallerSize, data()+that.size_, that.data());
	}
	else if (that.size_ < size_) {
		// Shrinking, so destruct elements past the new end.
		destructSpan(data()+smallerSize, data()+size_);
	}
	size_ = that.size_;
	return *this;
}

template<typename T>
Array<T>::Array(std::initializer_list<T> list) : data_(nullptr), size_(0), capacity_(0) {
	const size_t listSize = list.size();
	if (listSize == 0) {
		return;
	}
	// Only allocate up to listSize
	increaseCapacity(listSize);
	size_ = listSize;
	T* begin = data();
	T* end = begin + listSize;
	const T* source = that.data();
	copyConstructSpan(begin, end, list.begin());
}

template<typename T>
void Array<T>::setCapacity(const size_t newCapacity) {
	if (capacity() == newCapacity) {
		return;
	}
	T*const oldData = data();
	const size_t oldSize = size_;
	if (newCapacity < oldSize) {
		if (!std::is_pod<T>::value) {
			destructSpan(oldData + newCapacity, oldData + oldSize);
		}
		size_ = newCapacity;
	}

	if (isLocalBuffer()) {
		// Don't free the local buffer; just realloc.
		reallocLocalBuffer(newCapacity, false);
	}
	else {
		if (is_trivially_relocatable<T>::value || size_ == 0) {
			data_.realloc(newCapacity, size_);
		}
		else {
			// realloc doesn't work for types T that contain pointers to within themselves,
			// so we must allocate a new buffer and move-assign the data.
			T* oldData = data_.release();
			data_.realloc(newCapacity, 0);
			// release() should have made realloc() allocate a new buffer.
			assert(begin() != oldData);
			moveConstructSpan(begin(), end(), oldData);
			free(oldData);
		}

		capacity_ = newCapacity;

		// If the heap happens to allocate a block that
		// is immediately after this, allocate again,
		// so that the isLocalBuffer() check can always be used
		// to tell whether the block is a local buffer or not.
		if (isLocalBuffer()) {
			reallocLocalBuffer(capacity_, true);
		}
	}
}

template<typename T>
void Array<T>::increaseCapacity(const size_t newCapacity) {
	if (isLocalBuffer()) {
		// Don't free the local buffer; just realloc.
		reallocLocalBuffer(newCapacity, false);
	}
	else {
		if (is_trivially_relocatable<T>::value || size_ == 0) {
			data_.realloc(newCapacity, size_);
		}
		else {
			// realloc doesn't work for types T that contain pointers to within themselves,
			// so we must allocate a new buffer and move-assign the data.
			T* oldData = data_.release();
			data_.realloc(newCapacity, 0);
			// release() should have made realloc() allocate a new buffer.
			assert(begin() != oldData);
			moveConstructSpan(begin(), end(), oldData);
			free(oldData);
		}

		capacity_ = newCapacity;

		// If the heap happens to allocate a block that
		// is immediately after this, allocate again,
		// so that the isLocalBuffer() check can always be used
		// to tell whether the block is a local buffer or not.
		if (isLocalBuffer()) {
			reallocLocalBuffer(capacity_, true);
		}
	}
}

template<typename T>
INLINE void Array<T>::setSize(const size_t newSize) {
	const size_t oldSize = size();
	if (newSize == oldSize) {
		return;
	}
	const size_t oldCapacity = capacity();
	if (newSize > oldCapacity) {
		const size_t coarse = coarseCapacity(oldCapacity);
		increaseCapacity((newSize > coarse) ? newSize : coarse);
	}
	if (!std::is_pod<T>::value) {
		T*const oldEnd = data() + oldSize;
		T*const newEnd = data() + newSize;
		if (newSize > oldSize) {
			constructSpan(oldEnd, newEnd);
		}
		else {
			destructSpan(newEnd, oldEnd);
		}
	}
	size_ = newSize;
}

template<typename T>
INLINE void Array<T>::append(const T& that, const size_t numCopies) {
	if (numCopies == 0) {
		return;
	}
	const size_t oldSize = size();
	const size_t newSize = oldSize + numCopies;
	const size_t oldCapacity = capacity();
	if (newSize > oldCapacity) {
		const size_t coarse = coarseCapacity(oldCapacity);
		increaseCapacity((newSize > coarse) ? newSize : coarse);
	}
	T*const oldEnd = data() + oldSize;
	T*const newEnd = data() + newSize;
	if constexpr (!std::is_pod<T>::value) {
		for (T* p = oldEnd; p != newEnd; ++p) {
			new (p) T(that);
		}
	}
	else {
		for (T* p = oldEnd; p != newEnd; ++p) {
			*p = that;
		}
	}
	size_ = newSize;
}

template<typename T>
INLINE void Array<T>::append(T&& that) {
	const size_t oldSize = size();
	const size_t newSize = oldSize + 1;
	const size_t oldCapacity = capacity();
	if (newSize > oldCapacity) {
		const size_t coarse = coarseCapacity(oldCapacity);
		increaseCapacity((newSize > coarse) ? newSize : coarse);
	}
	T* p = data() + oldSize;
	if constexpr (!std::is_pod<T>::value) {
		new (p) T(std::move(that));
	}
	else {
		*p = that;
	}
	size_ = newSize;
}

template<typename T>
template<typename ITER_T>
INLINE void Array<T>::append(ITER_T begin, const ITER_T end) {
	const size_t numNew = std::distance(begin, end);
	if (numNew == 0) {
		return;
	}
	const size_t oldSize = size();
	const size_t newSize = oldSize + numNew;
	const size_t oldCapacity = capacity();
	if (newSize > oldCapacity) {
		const size_t coarse = coarseCapacity(oldCapacity);
		increaseCapacity((newSize > coarse) ? newSize : coarse);
	}
	T*const oldEnd = data() + oldSize;
	T*const newEnd = data() + newSize;
	if constexpr (!std::is_pod<T>::value) {
		for (T* p = oldEnd; p != newEnd; ++p, ++begin) {
			new (p) T(*begin);
		}
	}
	else {
		for (T* p = oldEnd; p != newEnd; ++p, ++begin) {
			*p = *begin;
		}
	}
	size_ = newSize;
}

template<typename T>
void Array<T>::reallocLocalBuffer(size_t newCapacity, bool freeOldBlock) {
	// There's currently a memory block in the address of a local buffer,
	// so allocating another on the heap will yield a location that is in
	// a different address.
	// Any elements to be destructed must already have been destructed.
	T* oldData = data_.release();
	data_.realloc(newCapacity, 0);
	capacity_ = newCapacity;
	T* newData = data();
	// Move the data to the new block
	moveAssignSpan(newData, newData+size_, oldData);
	if (freeOldBlock) {
		free(oldData);
	}
}

// Delegate to the more general move constructor that takes an Array,
// since there's no significant benefit to taking a BufArray.
template<typename T, size_t BUF_N>
INLINE BufArray<T,BUF_N>::BufArray(BufArray<T,BUF_N>&& that) : BufArray(static_cast<Array<T>&&>(that)) {}

template<typename T, size_t BUF_N>
INLINE BufArray<T,BUF_N>::BufArray(Array<T>&& that) : BufArray() {
	// Default constructor was called, so we can delegate to move assignment.
	*this = std::move(that);
}

// Delegate to the more general copy constructor that takes an Array,
// since there's no significant benefit to taking a BufArray.
template<typename T, size_t BUF_N>
INLINE BufArray<T,BUF_N>::BufArray(const BufArray<T,BUF_N>& that) : BufArray(static_cast<const Array<T>&>(that)) {}

template<typename T, size_t BUF_N>
INLINE BufArray<T,BUF_N>::BufArray(const Array<T>& that) : BufArray() {
	// Default constructor was called, so we can delegate to copy assignment.
	*this = that;
}

template<typename T, size_t BUF_N>
INLINE BufArray<T,BUF_N>& BufArray<T,BUF_N>::operator=(BufArray<T,BUF_N>&& that) {
	// Delegate to the more general move assignment operator that takes an Array,
	// since there's no significant benefit to taking a BufArray.
	*this = std::move(static_cast<Array<T>&&>(that));
	return *this;
}

template<typename T, size_t BUF_N>
INLINE BufArray<T,BUF_N>& BufArray<T,BUF_N>::operator=(Array<T>&& that) {
	// TODO: We could restore the local buffer if that's size is BUF_N or less,
	// but for now, we'll just delegate to the Array move assignment operator.
	static_cast<Array<T>&>(*this) = std::move(that);
	return *this;
}

template<typename T, size_t BUF_N>
INLINE BufArray<T,BUF_N>& BufArray<T,BUF_N>::operator=(const BufArray<T,BUF_N>& that) {
	// Delegate to the more general copy assignment operator that takes an Array,
	// since there's no significant benefit to taking a BufArray.
	*this = std::move(static_cast<Array<T>&&>(that));
	return *this;
}

template<typename T, size_t BUF_N>
INLINE BufArray<T,BUF_N>& BufArray<T,BUF_N>::operator=(const Array<T>& that) {
	// TODO: We could restore the local buffer if that's size is BUF_N or less,
	// but for now, we'll just delegate to the Array copy assignment operator.
	static_cast<Array<T>&>(*this) = that;
	return *this;
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
