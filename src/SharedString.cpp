// This file defines UniqueString functionality that can't be in the header files
// SharedString.h or SharedStringDef.h and instead must be exported.

#include "SharedString.h"
#include "SharedStringDef.h"
#include "SharedArrayDef.h"
#include "BigSet.h"

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// This class is primarily to separate LocalUniqueString(const ShallowString&) from
// UniqueString(const ShallowString&), so that a SharedString doesn't need to be
// constructed unnecessarily just to look up a string that's already in the global unique set.
// It also has the advantage of a slightly simpler destructor.
// However, this means having to supply all of the necessary constructors and assignment operators.
class LocalUniqueString : public UniqueString {
protected:
	using UniqueString::data_;
public:
	INLINE LocalUniqueString() noexcept : UniqueString() {}
	INLINE LocalUniqueString(LocalUniqueString&& that) noexcept : UniqueString(std::move(that)) {}
	INLINE LocalUniqueString(const LocalUniqueString& that) noexcept : UniqueString(that) {}
	INLINE LocalUniqueString(UniqueString&& that) noexcept : UniqueString(std::move(that)) {}
	INLINE LocalUniqueString(const UniqueString& that) noexcept : UniqueString(that) {}

	// These must only be called by BigSet::insertCommon when a match has not been found.
	// SharedString and UniqueString have the same memory layout and copying should
	// do what's needed in the case where insertion occurs.
	INLINE LocalUniqueString(SharedString&& that) noexcept : UniqueString(std::move(static_cast<UniqueString&&>(that))) {
		static_assert(sizeof(SharedString) == sizeof(UniqueString));
	}
	INLINE LocalUniqueString(const SharedString& that) noexcept : UniqueString(*static_cast<const UniqueString*>(&that)) {
		static_assert(sizeof(SharedString) == sizeof(UniqueString));
	}

	// This must only be called by BigSet::insertCommon when a match has not been found.
	INLINE LocalUniqueString(const ShallowString& that) noexcept
		: UniqueString(std::move(static_cast<UniqueString&&>(SharedString(that)))) {
		static_assert(sizeof(SharedString) == sizeof(UniqueString));
	}

	INLINE ~LocalUniqueString() noexcept {
		assert(data_->refCount.load(std::memory_order_relaxed) == 1);
		// This should always be the last reference, so decRef and don't
		// bother with the base class destructors
		data_->decRef();
		data_ = nullptr;
	}

	INLINE LocalUniqueString& operator=(LocalUniqueString&& that) noexcept {
		UniqueString::operator=(std::move(that));
		return *this;
	}
	INLINE LocalUniqueString& operator=(const LocalUniqueString& that) noexcept {
		UniqueString::operator=(that);
		return *this;
	}

	friend UniqueString;
};

template<>
struct DefaultHasher<LocalUniqueString> : public DefaultHasher<UniqueString> {};

// This is the global set of UniqueString objects for ensuring uniqueness.
static BigSet<LocalUniqueString> uniqueStringSet;

UniqueString::UniqueString(const SharedString& that) noexcept : SharedString() {
	BigSet<LocalUniqueString>::const_accessor accessor;
	// SharedString and UniqueString have the same memory layout and copying should
	// do what's needed in the case where insertion occurs.
	static_assert(sizeof(SharedString) == sizeof(UniqueString));
	bool inserted = uniqueStringSet.insert(accessor, *static_cast<const UniqueString*>(&that));
	// Get the unique result from the accessor, not from that.
	data_ = accessor->data_;
	// NOTE: incRef must be done before releasing accessor, to avoid a race condition on destruction.
	data_->incRef();
}

UniqueString::UniqueString(SharedString&& that) noexcept : SharedString() {
	BigSet<LocalUniqueString>::const_accessor accessor;
	// SharedString and UniqueString have the same memory layout and copying should
	// do what's needed in the case where insertion occurs.
	static_assert(sizeof(SharedString) == sizeof(UniqueString));
	bool inserted = uniqueStringSet.insert(accessor, std::move(static_cast<UniqueString&&>(that)));
	// Get the unique result from the accessor, not from that.
	data_ = accessor->data_;
	// NOTE: incRef must be done before releasing accessor, to avoid a race condition on destruction.
	data_->incRef();
}

UniqueString::UniqueString(const ShallowString& that) noexcept : SharedString() {
	BigSet<LocalUniqueString>::const_accessor accessor;
	// NOTE: LocalUniqueString prevents this from recursing infinitely via its copy constructor.
	bool inserted = uniqueStringSet.insert(accessor, that);
	data_ = accessor->data_;
	// NOTE: incRef must be done before releasing accessor, to avoid a race condition on destruction.
	data_->incRef();
}

UniqueString::~UniqueString() noexcept {
	// There should be at least one reference here and one reference in the table.
	assert(data_->refCount.load(std::memory_order_relaxed) >= 2);

	size_t newRefCount = data_->decRef();
	if (newRefCount == 1) {
		// Just the table reference is left, so remove it from the table.
		// Even though this reference has already been removed, this
		// should be valid for long enough to look it up in uniqueStringSet.
#ifndef NDEBUG
		bool erased =
#endif
		uniqueStringSet.erase(*this);
		assert(erased);
	}

	// Do not depend on the base class destructor, since it will try to re-decrement.
	data_ = nullptr;
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
