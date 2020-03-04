#pragma once

// This file declares the class SharedString, a string class where the contents
// are reference-counted and have a cached hash code.
// Implementations of functions requiring calls to memory allocation or text processing
// are in SharedStringDef.h, so to call them, you must include SharedStringDef.h, too.

#include "SharedArray.h"
#include "Bits.h"
#include "Types.h"

#include <type_traits>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

class ShallowString;

struct StructWithHash {
	uint64 hashCode;
};

// A reference-counted shared string class caching the size and a a hash code.
// The text is always zero-terminated, just in case it's needed, even though
// the size is stored.
class SharedString : private SharedArray<char,StructWithHash> {
public:
	constexpr INLINE SharedString() : SharedArray() {}

	// Move constructor
	// This will take ownership of the memory owned by that.
	constexpr INLINE SharedString(SharedString&& that) : SharedArray(std::move(that)) {}

	// Copy constructor
	// This is marked explicit to reduce accidental string copying,
	// even though this array type is reference counted.
	explicit INLINE SharedString(const SharedString& that) : SharedArray(that) {}

	inline SharedString(const char* text, size_t size, uint64 hash);
	INLINE SharedString(const char* text, size_t size);
	explicit INLINE SharedString(const char* text);
	explicit INLINE SharedString(const ShallowString& that);

	inline ~SharedString();

	// Move assignment operator
	inline SharedString& operator=(SharedString&& that);
	// Copy assignment operator
	inline SharedString& operator=(const SharedString& that);

	[[nodiscard]] inline bool operator==(const SharedString& that) const;
	[[nodiscard]] inline bool operator!=(const SharedString& that) const;

	// This is useful for fast comparison if it's known that all SharedString
	// objects have been reduced to single equivalence class representatives
	// in advance.
	[[nodiscard]] constexpr INLINE bool arePointersEqual(const SharedString& that) const {
		return data_ == that.data_;
	}
	[[nodiscard]] constexpr INLINE bool arePointersEqual(const ShallowString& that) const;

	[[nodiscard]] constexpr INLINE const char* data() const {
		if (data_ == nullptr) {
			return nullptr;
		}
		if (data_->size <= 8) {
			// Strings 8 characters or less overlap with the hash code.
			return reinterpret_cast<const char*>(&(data_->hashCode));
		}
		return reinterpret_cast<const char*>(data_+1);
	}
	[[nodiscard]] constexpr INLINE uint64 hash() const {
		return data_ ? data_->hashCode : uint64(0);
	}
	[[nodiscard]] constexpr INLINE size_t size() const {
		return data_ ? data_->size : size_t(0);
	}

	// Implemented in SharedStringDef.h, using text::stringHash function in text/TextFunctions.h
	[[nodiscard]] static constexpr INLINE uint64 computeHash(const char* text, size_t size);
};

// SharedString does not contain self-pointers, so can be realloc'd.
template<>
struct is_trivially_relocatable<SharedString> : public std::true_type {};

// Unlike SharedString, ShallowString does not own the string or allocate any memory.
// This text is not necessarily zero-terminated.
class ShallowString {
	const char* text;
	size_t size_;
	uint64 hashCode;
public:
	// NOTE: This default constructor does nothing, in order to make ShallowString
	// a POD type.
	INLINE ShallowString() = default;
	constexpr INLINE ShallowString(const ShallowString& that) = default;
	constexpr INLINE ShallowString(ShallowString&& that) = default;
	explicit constexpr INLINE ShallowString(const SharedString& that) : text(that.data()), size_(that.size()), hashCode(that.hash()) {}

	constexpr INLINE ShallowString(const char* text_, const size_t size, const uint64 hash) : text(text_), size_(size), hashCode(hash) {}
	constexpr INLINE ShallowString(const char* text_, const size_t size);
	constexpr INLINE ShallowString(const char* text_);

	constexpr INLINE ShallowString& operator=(const ShallowString& that) = default;
	constexpr INLINE ShallowString& operator=(ShallowString&& that) = default;

	[[nodiscard]] constexpr inline bool operator==(const ShallowString& that) const;
	[[nodiscard]] constexpr inline bool operator!=(const ShallowString& that) const;

	[[nodiscard]] constexpr INLINE const char* data() const {
		return text;
	}

	[[nodiscard]] constexpr INLINE size_t size() const {
		return size_;
	}

	[[nodiscard]] constexpr INLINE uint64 hash() const {
		return hashCode;
	}

	[[nodiscard]] constexpr INLINE bool arePointersEqual(const ShallowString& that) const {
		return text == that.text;
	}
	[[nodiscard]] constexpr INLINE bool arePointersEqual(const SharedString& that) const {
		return text == that.data();
	}
};

[[nodiscard]] constexpr INLINE bool operator==(const ShallowString& a, const SharedString& b) {
	return a == ShallowString(b);
}
[[nodiscard]] constexpr INLINE bool operator==(const SharedString& a, const ShallowString& b) {
	return ShallowString(a) == b;
}

[[nodiscard]] constexpr INLINE bool operator!=(const ShallowString& a, const SharedString& b) {
	return a != ShallowString(b);
}
[[nodiscard]] constexpr INLINE bool operator!=(const SharedString& a, const ShallowString& b) {
	return ShallowString(a) != b;
}

[[nodiscard]] constexpr INLINE bool SharedString::arePointersEqual(const ShallowString& that) const {
	return data() == that.data();
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
