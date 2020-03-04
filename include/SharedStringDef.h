#pragma once

#include "SharedArray.h"
#include "SharedArrayDef.h"
#include "SharedString.h"
#include "text/TextFunctions.h"

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

inline SharedString::SharedString(const char* text, size_t size, uint64 hash) {
	if (size > 8) {
		SharedArrayHeader* data = (SharedArrayHeader*)malloc(sizeof(SharedArrayHeader) + size+1);
		data->refCount.store(1, std::memory_order_relaxed);
		data->size = size;
		data->hashCode = hash;
		char* dataText = reinterpret_cast<char*>(data+1);
		for (size_t i = 0; i < size; ++i) {
			dataText[i] = text[i];
		}
		dataText[size] = 0;
		data_ = data;
		return;
	}

	// Size is 8 or less, so the text fits entirely in the hash code.
	SharedArrayHeader* data = (SharedArrayHeader*)malloc(sizeof(SharedArrayHeader) + size_t(size == 8));
	data->refCount.store(1, std::memory_order_relaxed);
	data->size = size;
	// The text is contained in the hash, so just assign the hash.
	data->hashCode = hash;
	if (size == 8) {
		// Need to add a zero terminator if size is exactly 8.
		char* dataText = reinterpret_cast<char*>(data+1);
		dataText[0] = 0;
	}
	data_ = data;
}

INLINE SharedString::SharedString(const char* text, size_t size) : SharedString(text, size, computeHash(text, size)) {}
INLINE SharedString::SharedString(const char* text) : SharedString(text, text::stringSize(text)) {}
INLINE SharedString::SharedString(const ShallowString& that) : SharedString(that.data(), that.size(), that.hash()) {}

INLINE SharedString::~SharedString() {
	// Base class destructor does decRef on data_.
}

INLINE SharedString& SharedString::operator=(SharedString&& that) {
	if (data_ != nullptr) {
		data_->decRef();
	}
	data_ = that.data_;
	that.data_ = nullptr;
	return *this;
}

INLINE SharedString& SharedString::operator=(const SharedString& that) {
	if (data_ != nullptr) {
		data_->decRef();
	}
	data_ = that.data_;
	data_->incRef();
	return *this;
}

[[nodiscard]] inline bool SharedString::operator==(const SharedString& that) const {
	const SharedArrayHeader* thatData = that.data_;
	if (data_ == thatData) {
		return true;
	}
	if (data_ == nullptr || thatData == nullptr) {
		return false;
	}
	if (data_->hashCode != thatData->hashCode) {
		return false;
	}
	size_t size = data_->size;
	if (size != thatData->size) {
		return false;
	}
	if (size <= 8) {
		// Content (apart from zero terminator) is contained entirely in hash code.
		return true;
	}
	return text::areEqualSizeStringsEqual(
		reinterpret_cast<const char*>(data_+1),
		reinterpret_cast<const char*>(thatData+1),
		size);
}

[[nodiscard]] INLINE bool SharedString::operator!=(const SharedString& that) const {
	return !(*this == that);
}

[[nodiscard]] constexpr INLINE uint64 SharedString::computeHash(const char* text, size_t size) {
	return text::stringHash(text, size);
}

constexpr INLINE ShallowString::ShallowString(const char* text_, const size_t size) : ShallowString(text_, size, SharedString::computeHash(text_, size)) {}
constexpr INLINE ShallowString::ShallowString(const char* text_) : ShallowString(text_, text::stringSize(text_)) {
	static_assert(std::is_pod<ShallowString>::value);
}

[[nodiscard]] constexpr inline bool ShallowString::operator==(const ShallowString& that) const {
	if (text == that.text) {
		return true;
	}
	if (text == nullptr || that.text == nullptr) {
		return false;
	}
	if (hashCode != that.hashCode || size_ != that.size_) {
		return false;
	}
	if (size_ <= 8) {
		// Strings of 8 bytes or less are uniquely determined by their hash code.
		return true;
	}
	return text::areEqualSizeStringsEqual(text, that.text, size_);
}

[[nodiscard]] constexpr INLINE bool ShallowString::operator!=(const ShallowString& that) const {
	return !(*this == that);
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
