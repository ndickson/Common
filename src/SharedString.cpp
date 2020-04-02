// This file defines UniqueString functionality that can't be in the header files
// SharedString.h or SharedStringDef.h and instead must be exported.

#include "SharedString.h"
#include "SharedStringDef.h"
#include "SharedArrayDef.h"
#include "BigSet.h"

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// This is the global set of UniqueString objects for ensuring uniqueness.
static BigSet<UniqueString> uniqueStringSet;

UniqueString::UniqueString(const SharedString& that) noexcept : SharedString() {
	BigSet<UniqueString>::const_accessor accessor;
	bool inserted = uniqueStringSet.insert(accessor, *static_cast<const UniqueString*>(&that));
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
