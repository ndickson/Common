#pragma once

// This file primarily declares classes Array, a generic array class,
// and BufArray, a subclass with a local buffer, to reduce heap allocation.
// Implementations of functions requiring calls to malloc, free, or realloc
// are in ArrayDef.h, so to call them, you must include ArrayDef.h, too.

#include "Types.h"
#include <type_traits>

OUTER_NAMESPACE_START
COMMON_LIBRARY_NAMESPACE_START

// This class is effectively an std::unique_ptr, but using
// malloc, free, and realloc, with a provided realloc wrapper function.
template<typename T>
class ArrayPtr {
	T* p;
public:
	constexpr INLINE ArrayPtr() : p(nullptr) {}

	// NOTE: This takes ownership of data.
	explicit constexpr INLINE ArrayPtr(T* p_) : p(p_) {}

	// Unfortunately, non-default destructors cannot be constexpr.
	// Calling this requires including ArrayDef.h
	INLINE ~ArrayPtr();

	explicit constexpr INLINE operator const T*() const {
		return p;
	}
	explicit constexpr INLINE operator T*() {
		return p;
	}
	constexpr INLINE bool operator==(const T*const that) const {
		return (p == that);
	}
	constexpr INLINE bool operator!=(const T*const that) const {
		return (p != that);
	}
	constexpr INLINE bool operator==(const nullptr_t that) const {
		return (p == nullptr);
	}
	constexpr INLINE bool operator!=(const nullptr_t that) const {
		return (p != nullptr);
	}
	constexpr INLINE bool operator==(const ArrayPtr<T>& that) const {
		return (p == that.p);
	}
	constexpr INLINE bool operator!=(const ArrayPtr<T>& that) const {
		return (p != that.p);
	}
	constexpr INLINE const T* operator+(size_t n) const {
		return p + n;
	}
	constexpr INLINE T* operator+(size_t n) {
		return p + n;
	}
	constexpr INLINE const T& operator[](size_t n) const {
		return p[n];
	}
	constexpr INLINE T& operator[](size_t n) {
		return p[n];
	}

	constexpr INLINE const T* get() const {
		return p;
	}
	constexpr INLINE T* get() {
		return p;
	}
	// NOTE: If the pointer was allocated with realloc, it must be freed with free.
	constexpr INLINE T* release() {
		T*const p_ = p;
		p = nullptr;
		return p_;
	}
	constexpr INLINE T* swap(ArrayPtr& that) {
		T* thisp = p;
		p = that.p;
		that.p = thisp;
	}

	// This frees p if non-null, and sets p to null.
	// NOTE: p must be null or have been allocated with malloc, calloc, or realloc.
	// Calling this requires including ArrayDef.h
	INLINE void reset();

	// This frees p if non-null, and sets p to thatp.
	// NOTE: This takes ownership of thatp.
	// NOTE: p must be null or have been allocated with malloc, calloc, or realloc.
	// Calling this requires including ArrayDef.h
	INLINE void reset(T* thatp);

	// This reallocates p with a new capacity (in number of T's, not bytes)
	// NOTE: p must be null or have been allocated with malloc, calloc, or realloc.
	// NOTE: This is safe to call with p being null, and is safe to call with n==0.
	// Calling this requires including ArrayDef.h
	INLINE void realloc(size_t newCapacity);
};

// Array is the main array class, with no local buffer.
//
// However, it takes into account that there is a subclass, BufArray,
// with a local buffer, so that it never tries to free the memory block
// if it is the local buffer.  In order for this check to work efficiently,
// any heap-allocated block that happens to be at the address where a local
// buffer would be must be rejected and reallocated to a new address,
// though it is likely uncommon for this to occur.
//
// This may seem excessive, but it allows one to pass (by reference or pointer)
// a BufArray to a function accepting an Array, and have the function still be able
// to correctly reallocate the buffer, e.g. upon changing the capacity, without
// incurring the expense of virtual function calls or a virtual function table pointer.
template<typename T>
class Array {
protected:
	ArrayPtr<T> data_;
	size_t size_;
	size_t capacity_;

public:
	constexpr INLINE Array() : data_(nullptr), size_(0), capacity_(0) {}

	// NOTE: This takes ownership of data.
	constexpr INLINE Array(T* data, size_t size, size_t capacity) : data_(data), size_(size), capacity_(capacity) {}

	// Move constructor
	// This will take ownership of the memory owned by that, if possible.
	Array(Array<T>&& that);

	// Copy constructor
	// This is marked explicit to reduce accidental array copying.
	explicit Array(const Array<T>& that);

	// The destructor destructs data_, whose destructor is in ArrayDef.h,
	// so this destructor is also in ArrayDef.h
	~Array();

	// Move assignment operator
	Array<T>& operator=(Array<T>&& that);
	// Copy assignment operator
	Array<T>& operator=(const Array<T>& that);

	[[nodiscard]] constexpr INLINE T* data() {
		return data_.get();
	}
	[[nodiscard]] constexpr INLINE const T* data() const {
		return data_.get();
	}
	[[nodiscard]] constexpr INLINE size_t size() const {
		return size_;
	}
	[[nodiscard]] constexpr INLINE size_t capacity() const {
		return capacity_;
	}
	[[nodiscard]] constexpr INLINE T* begin() {
		return data_.get();
	}
	[[nodiscard]] constexpr INLINE const T* begin() const {
		return data_.get();
	}
	[[nodiscard]] constexpr INLINE const T* cbegin() const {
		return data_.get();
	}
	[[nodiscard]] constexpr INLINE T* end() {
		return data()+size_;
	}
	[[nodiscard]] constexpr INLINE const T* end() const {
		return data()+size_;
	}
	[[nodiscard]] constexpr INLINE const T* cend() const {
		return data()+size_;
	}
	[[nodiscard]] constexpr INLINE T& operator[](size_t i) {
		return data_[i];
	}
	[[nodiscard]] constexpr INLINE const T& operator[](size_t i) const {
		return data_[i];
	}
	[[nodiscard]] constexpr INLINE T& last() {
		return data_[size_-1];
	}
	[[nodiscard]] constexpr INLINE const T& last() const {
		return data_[size_-1];
	}

	// This calls realloc on data_, whose implementation is in ArrayDef.h,
	// so this function is also in ArrayDef.h
	void setCapacity(const size_t newCapacity);

	// This is the same as setCapacity, but requires that newCapacity > capacity(),
	// so that checks for destruction can be avoided.
	void increaseCapacity(const size_t newCapacity);

	[[nodiscard]] constexpr static INLINE size_t coarseCapacity(const size_t oldCapacity) {
		// Always increase by at least floor(1.5*oldCapacity) + 1, e.g.:
		// 0, 1, 2, 4, 7, 11, 17, 26, 40, 61, 92, 139, 209, 314, ...
		return oldCapacity + (oldCapacity>>1) + 1;
	}

	// This calls setCapacity, whose implementation is in ArrayDef.h,
	// so this function is also in ArrayDef.h
	INLINE void setSize(const size_t newSize);

	INLINE void append(const T& that, const size_t numCopies=1);
	INLINE void append(T&& that);

	// There must be at least one element in the array for this to be valid.
	INLINE void removeLast() {
		--size_;
		if (!std::is_pod<T>::value) {
			data_[size_].~T();
		}
	}

protected:
	static INLINE void constructSpan(T*const begin, T*const end) {
		if constexpr (!std::is_pod<T>::value) {
			for (T* p = begin; p != end; ++p) {
				new (p) T();
			}
		}
	}
	static INLINE void destructSpan(T*const begin, T*const end) {
		if constexpr (!std::is_pod<T>::value) {
			for (T* p = begin; p != end; ++p) {
				p->~T();
			}
		}
	}
	static INLINE void moveConstructSpan(T*const destBegin, T*const destEnd, T* source) {
		if constexpr (!std::is_pod<T>::value) {
			for (T* p = destBegin; p != destEnd; ++p, ++source) {
				new (p) T(std::move(*source));
			}
		}
		else {
			for (T* p = destBegin; p != destEnd; ++p, ++source) {
				*p = *source;
			}
		}
	}
	static INLINE void copyConstructSpan(T*const destBegin, T*const destEnd, const T* source) {
		if constexpr (!std::is_pod<T>::value) {
			for (T* p = destBegin; p != destEnd; ++p, ++source) {
				new (p) T(*source);
			}
		}
		else {
			for (T* p = destBegin; p != destEnd; ++p, ++source) {
				*p = *source;
			}
		}
	}
	static INLINE void moveAssignSpan(T*const destBegin, T*const destEnd, T* source) {
		if constexpr (!std::is_pod<T>::value) {
			for (T* p = destBegin; p != destEnd; ++p, ++source) {
				*p = std::move(*source);
			}
		}
		else {
			for (T* p = destBegin; p != destEnd; ++p, ++source) {
				*p = *source;
			}
		}
	}
	static INLINE void copyAssignSpan(T*const destBegin, T*const destEnd, const T* source) {
		for (T* p = destBegin; p != destEnd; ++p, ++source) {
			*p = *source;
		}
	}

private:
	constexpr INLINE bool isLocalBuffer() const;

	void reallocLocalBuffer(size_t newCapacity, bool freeOldBlock);
};

// BufArray is an Array that starts with a local buffer that's BUF_N items in capacity.
// This is particularly useful for Array's that are usually small, or that
// may be frequently created and destroyed, like for text string processing.
// It avoids the need for an extra heap allocation if the Array size stays
// within the BUF_N capacity.
template<typename T, size_t BUF_N>
class BufArray : public Array<T> {
	alignas(T) int8 localBuffer[BUF_N*sizeof(T)];

public:
	// NOTE: This leaves buffer_ uninitialized for performance, since it's not used yet.
	INLINE BufArray() : Array<T>((T*)localBuffer, 0, BUF_N) {}

	// Move constructors
	// These only take ownership of the memory block of that if it doesn't fit and
	// the memory block of that isn't local either.
	// The first is redundant, but C++ likes to have an exact type match move constructor.
	BufArray(BufArray<T,BUF_N>&& that);
	BufArray(Array<T>&& that);

	// Copy constructors
	// These are marked explicit to reduce accidental array copying.
	// The first is redundant, but C++ likes to have an exact type match copy constructor.
	explicit BufArray(const BufArray<T,BUF_N>& that);
	explicit BufArray(const Array<T>& that);

	// Move assignment operators
	// The first is redundant, but C++ likes to have an exact type match move assignment operator.
	BufArray<T,BUF_N>& operator=(BufArray<T,BUF_N>&& that);
	BufArray<T,BUF_N>& operator=(Array<T>&& that);
	// Copy assignment operators
	// The first is redundant, but C++ likes to have an exact type match copy assignment operator.
	BufArray<T,BUF_N>& operator=(const BufArray<T,BUF_N>& that);
	BufArray<T,BUF_N>& operator=(const Array<T>& that);

	constexpr INLINE bool isLocalBuffer() const {
		return Array<T>::data_.get() == (const T*)localBuffer;
	}
};

// This implementation is after BufArray, so that BufArray's implementation can be used.
template<typename T>
constexpr INLINE bool Array<T>::isLocalBuffer() const {
	// The local buffer offset should be the same, regardless of (non-zero) BUF_N.
	return static_cast<const BufArray<T,1>*>(this)->isLocalBuffer();
}

// Array does not contain self-pointers, so can be realloc'd.
template<typename T>
struct is_trivially_relocatable<Array<T>> : public std::true_type {};

// BufArray does contain self-pointers, so can't be realloc'd.
template<typename T, size_t BUF_N>
struct is_trivially_relocatable<BufArray<T,BUF_N>> : public std::false_type {};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
