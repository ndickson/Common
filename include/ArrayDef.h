#pragma once

// This file primarily contains definitions of functions for the classes
// Array and BufArray, declared in Array.h, that require malloc, free, or realloc.
// In order to call those functions, you must include this file.

#include "Types.h"
#include "Array.h"

#include <stdlib.h> // For free and realloc
#include <type_traits>

OUTER_NAMESPACE_START
COMMON_LIBRARY_NAMESPACE_START

template<typename T>
INLINE ArrayPtr<T>::~ArrayPtr() {
	if (p != nullptr) {
		free(p);
	}
}

template<typename T>
INLINE void ArrayPtr<T>::reset() {
	if (p != nullptr) {
		free(p);
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
		free(p);
	}
	p = thatp;
}

template<typename T>
INLINE void ArrayPtr<T>::realloc(size_t newCapacity) {
	if (newCapacity == 0) {
		reset();
	}
	else {
		p = (T*)::realloc(p, newCapacity*sizeof(T));
	}
}

template<typename T>
Array<T>::Array(Array<T>&& that) : data_(nullptr) {
	if (that.isLocalBuffer()) {
		// Copy construct, since ownership of the buffer can't be taken.
		size_ = 0;
		capacity_ = 0;
		setCapacity(that.size_);
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
	if (size_ == 0) {
		return;
	}
	// Only allocate up to size_
	setCapacity(size_);
	T* begin = data();
	T* end = begin + size_;
	const T* source = that.data();
	if (!std::is_pod<T>::value) {
		for (T* p = begin; p != end; ++p, ++source) {
			new (p) T(*source);
		}
	}
	else {
		for (T* p = begin; p != end; ++p, ++source) {
			*p = *source;
		}
	}
}

template<typename T>
Array<T>::~Array() {
	if (!std::is_pod<T>::value && size_ > 0) {
		destructSpan(data(), data()+size_);
	}
	if (isLocalBuffer()) {
		data_.release();
	}
	// data_ frees array automatically on destruction
}

template<typename T>
Array<T>& Array<T>::operator=(Array<T>&& that) {
	if (that.isLocalBuffer()) {
		// Copy assign, since ownership of the buffer can't be taken.
		*this = that;
	}
	else {
		if (isLocalBuffer()) {
			data.release();
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
	// Only set capacity if growing to more than current capacity.
	if (that.size_ > capacity_) {
		setCapacity(that.size_);
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
		data_.realloc(newCapacity);

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
		setCapacity((newSize > coarse) ? newSize : coarse);
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
		setCapacity((newSize > coarse) ? newSize : coarse);
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
		setCapacity((newSize > coarse) ? newSize : coarse);
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
void Array<T>::reallocLocalBuffer(size_t newCapacity, bool freeOldBlock) {
	// There's currently a memory block in the address of a local buffer,
	// so allocating another on the heap will yield a location that is in
	// a different address.
	T* oldData = data_.release();
	data_.realloc(newCapacity);
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
