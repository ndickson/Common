#pragma once

// This file declares the class SharedString, a string class where the contents
// are reference-counted and have a cached hash code.
// Implementations of functions requiring calls to memory allocation or text processing
// are in SharedStringDef.h, so to call them, you must include SharedStringDef.h, too.

#include "SharedArray.h"
#include "Bits.h"
#include "Types.h"

#include <type_traits>
#include <utility>

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
protected:
	using SharedArray<char,StructWithHash>::data_;
public:
	constexpr INLINE SharedString() noexcept : SharedArray() {}

	// Move constructor
	// This will take ownership of the memory owned by that.
	constexpr INLINE SharedString(SharedString&& that) noexcept : SharedArray(std::move(that)) {}

	// Copy constructor
	// This is marked explicit to reduce accidental string copying,
	// even though this array type is reference counted.
	explicit INLINE SharedString(const SharedString& that) noexcept : SharedArray(that) {}

	inline SharedString(const char* text, size_t size, uint64 hash) noexcept;
	INLINE SharedString(const char* text, size_t size) noexcept;
	explicit INLINE SharedString(const char* text) noexcept;
	explicit INLINE SharedString(const ShallowString& that) noexcept;

	inline ~SharedString();

	// Move assignment operator
	inline SharedString& operator=(SharedString&& that) noexcept;
	// Copy assignment operator
	inline SharedString& operator=(const SharedString& that) noexcept;

	[[nodiscard]] inline bool operator==(const SharedString& that) const noexcept;
	[[nodiscard]] inline bool operator!=(const SharedString& that) const noexcept;

	// This is useful for fast comparison if it's known that all SharedString
	// objects have been reduced to single equivalence class representatives
	// in advance.
	[[nodiscard]] constexpr INLINE bool arePointersEqual(const SharedString& that) const noexcept {
		return data_ == that.data_;
	}
	[[nodiscard]] constexpr INLINE bool arePointersEqual(const ShallowString& that) const noexcept;

	[[nodiscard]] constexpr INLINE const char* data() const noexcept {
		if (data_ == nullptr) {
			return nullptr;
		}
		if (data_->size <= 8) {
			// Strings 8 characters or less overlap with the hash code.
			return reinterpret_cast<const char*>(&(data_->hashCode));
		}
		return reinterpret_cast<const char*>(data_+1);
	}
	[[nodiscard]] constexpr INLINE uint64 hash() const noexcept {
		return data_ ? data_->hashCode : uint64(0);
	}
	[[nodiscard]] constexpr INLINE size_t size() const noexcept {
		return data_ ? data_->size : size_t(0);
	}

	// Implemented in SharedStringDef.h, using text::stringHash function in text/TextFunctions.h
	[[nodiscard]] static constexpr INLINE uint64 computeHash(const char* text, size_t size) noexcept;
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
	INLINE ShallowString() noexcept = default;
	constexpr INLINE ShallowString(const ShallowString& that) noexcept = default;
	constexpr INLINE ShallowString(ShallowString&& that) noexcept = default;
	explicit constexpr INLINE ShallowString(const SharedString& that) noexcept : text(that.data()), size_(that.size()), hashCode(that.hash()) {}

	constexpr INLINE ShallowString(const char* text_, const size_t size, const uint64 hash) noexcept : text(text_), size_(size), hashCode(hash) {}
	constexpr INLINE ShallowString(const char* text_, const size_t size) noexcept;
	constexpr INLINE ShallowString(const char* text_) noexcept;

	constexpr INLINE ShallowString& operator=(const ShallowString& that) noexcept = default;
	constexpr INLINE ShallowString& operator=(ShallowString&& that) noexcept = default;

	[[nodiscard]] constexpr inline bool operator==(const ShallowString& that) const noexcept;
	[[nodiscard]] constexpr inline bool operator!=(const ShallowString& that) const noexcept;

	[[nodiscard]] constexpr INLINE const char* data() const noexcept {
		return text;
	}

	[[nodiscard]] constexpr INLINE size_t size() const noexcept {
		return size_;
	}

	[[nodiscard]] constexpr INLINE uint64 hash() const noexcept {
		return hashCode;
	}

	[[nodiscard]] constexpr INLINE bool arePointersEqual(const ShallowString& that) const noexcept {
		return text == that.text;
	}
	[[nodiscard]] constexpr INLINE bool arePointersEqual(const SharedString& that) const noexcept {
		return text == that.data();
	}
};

[[nodiscard]] constexpr INLINE bool operator==(const ShallowString& a, const SharedString& b) noexcept {
	return a == ShallowString(b);
}
[[nodiscard]] constexpr INLINE bool operator==(const SharedString& a, const ShallowString& b) noexcept {
	return ShallowString(a) == b;
}

[[nodiscard]] constexpr INLINE bool operator!=(const ShallowString& a, const SharedString& b) noexcept {
	return a != ShallowString(b);
}
[[nodiscard]] constexpr INLINE bool operator!=(const SharedString& a, const ShallowString& b) noexcept {
	return ShallowString(a) != b;
}

[[nodiscard]] constexpr INLINE bool SharedString::arePointersEqual(const ShallowString& that) const noexcept {
	return data() == that.data();
}

INLINE SharedString operator "" _str(const char* text, size_t size) noexcept {
	return SharedString(text, size);
}

// UniqueString uses a threadsafe global set to ensure that any two UniqueString objects
// that have equal strings are pointing to the exact same text in memory.
// This means that equality comparisons can be done with pointer comparison,
// and also that frequently recurring strings can be stored a single time in memory.
class UniqueString : public SharedString {
public:
	constexpr INLINE UniqueString() noexcept : SharedString() {}

	// Move constructor
	// This will take ownership of the memory owned by that.
	constexpr INLINE UniqueString(UniqueString&& that) noexcept : SharedString(std::move(that)) {}

	// Copy constructor
	// This is marked explicit to reduce accidental string copying,
	// even though this array type is reference counted.
	explicit INLINE UniqueString(const UniqueString& that) noexcept : SharedString(that) {}

	explicit UniqueString(const SharedString& that) noexcept;
	explicit UniqueString(const ShallowString& that) noexcept;
	explicit INLINE UniqueString(const char* that) noexcept : UniqueString(ShallowString(that)) {}
	INLINE UniqueString(const char* that, const size_t size) noexcept : UniqueString(ShallowString(that, size)) {}
	INLINE UniqueString(const char* that, const size_t size, const uint64 hash) noexcept : UniqueString(ShallowString(that, size, hash)) {}

	// If the reference count would reach 1 after decrementing it, (i.e. is 2 at the time of this destructor),
	// this removes the string from the unique string table and frees the string.
	~UniqueString() noexcept;

	INLINE UniqueString& operator=(UniqueString&& that) noexcept {
		SharedString::operator=(std::move(that));
		return *this;
	}
	INLINE UniqueString& operator=(const UniqueString& that) noexcept {
		SharedString::operator=(that);
		return *this;
	}

	[[nodiscard]] constexpr INLINE bool operator==(const UniqueString& that) const noexcept {
		return arePointersEqual(that);
	}
	[[nodiscard]] constexpr INLINE bool operator!=(const UniqueString& that) const noexcept {
		return !arePointersEqual(that);
	}

};

// This supports all combinations of SharedString and ShallowString.
template<>
struct DefaultHasher<SharedString> {
	static INLINE uint64 hash(const SharedString& a) noexcept {
		return a.hash();
	}
	static INLINE uint64 hash(const ShallowString& a) noexcept {
		return a.hash();
	}
	static INLINE bool equals(const SharedString& a, const SharedString& b) noexcept {
		return a == b;
	}
	static INLINE bool equals(const SharedString& a, const ShallowString& b) noexcept {
		return a == b;
	}
	static INLINE bool equals(const ShallowString& a, const SharedString& b) noexcept {
		return a == b;
	}
	static INLINE bool equals(const ShallowString& a, const ShallowString& b) noexcept {
		return a == b;
	}
};

template<>
struct DefaultHasher<ShallowString> : public DefaultHasher<SharedString> {};
template<>
struct DefaultHasher<UniqueString> : public DefaultHasher<SharedString> {};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
