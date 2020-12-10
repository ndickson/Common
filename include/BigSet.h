#pragma once

#include "Types.h"
#include "HashUtil.h"
#include "ThreadUtil.h"

#include <assert.h>
#include <atomic>
#include <memory>
#include <type_traits>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// This is a hash set designed for very large numbers of items
// being looked up and added from many threads simultaneously.
// It has a very large unfront memory footprint, so is best suited
// to situations where there is a single large set, like a global set
// of unique string representatives.
template<typename VALUE_T, typename Hasher = DefaultHasher<VALUE_T>>
class BigSet {
protected:
	using TablePair = hash::Pair<VALUE_T>;

	// To reduce memory contention, make sure
	// that each of the atomic values is in a separate
	// 64-byte primary cache line.
	struct alignas(64) SubTable {
		volatile std::atomic<intptr_t> numReaders;
		volatile size_t size;
		volatile size_t capacity;

		// The size_t of each pair is the current target index in this sub-table.
		TablePair*volatile data;

		constexpr static size_t EMPTY_INDEX = hash::EMPTY_INDEX;

		SubTable() : numReaders(0), size(0), capacity(0), data(nullptr) {}

		~SubTable() {
			assert(numReaders.load(std::memory_order_relaxed) == 0);
			delete [] data;
		}

		void startReading() {
			// If there is already a writer (-1), wait with backoff until there
			// is not.  If the number of readers is negative, there's a writer
			// waiting to write, so wait with backoff until the number of readers
			// is zero or higher and then add 1.
			bool success;
			intptr_t value = numReaders.load(std::memory_order_relaxed);
			size_t attempt = 0;
			do {
				if (value < 0) {
					backOff(attempt);
					success = false;
					value = numReaders.load(std::memory_order_relaxed);
				}
				else {
					intptr_t newValue = value + 1;
					success = numReaders.compare_exchange_strong(value, newValue, std::memory_order_acq_rel);
				}
			} while (!success);
		}

		void stopReading() {
			assert(numReaders.load(std::memory_order_relaxed) != -1 && numReaders.load(std::memory_order_relaxed) != 0);
			bool success;
			intptr_t value = numReaders.load(std::memory_order_relaxed);
			do {
				intptr_t newValue = value + ((value < 0) ? 1 : -1);
				success = numReaders.compare_exchange_strong(value, newValue, std::memory_order_acq_rel);
			} while (!success);
		}

		void startWriting() {
			// If there is already a writer (-1), wait with backoff until there
			// is not.  If the number of readers is zero, set it to -1.
			// If the number of readers is positive, first, replace it with
			// -1-numReaders.  Then, wait with backoff for it to reach -1.
			bool success;
			intptr_t value = numReaders.load(std::memory_order_relaxed);
			size_t attempt = 0;
			do {
				if (value < 0) {
					backOff(attempt);
					success = false;
					value = numReaders.load(std::memory_order_relaxed);
				}
				else if (value == 0) {
					success = numReaders.compare_exchange_strong(value, intptr_t(-1), std::memory_order_acq_rel);
				}
				else {
					intptr_t newValue = intptr_t(-1) - value;
					success = numReaders.compare_exchange_strong(value, newValue, std::memory_order_acq_rel);
					if (success) {
						// Wait for the value to become -1 when all readers finish.
						while (numReaders.load(std::memory_order_acquire) != intptr_t(-1))
						{
							backOff(attempt);
						}
					}
				}
			} while (!success);
		}

		void stopWriting() {
			assert(numReaders.load(std::memory_order_relaxed) == -1);
			numReaders.store(0, std::memory_order_release);
		}

		// NOTE: Changing from write to read is safe, because
		// write access is exclusive.
		void changeFromWriteToRead() {
			assert(numReaders.load(std::memory_order_relaxed) == -1);
			numReaders.store(1, std::memory_order_release);
		}

		bool tryChangeFromReadToWrite() {
			assert(numReaders.load(std::memory_order_relaxed) != 0);
			assert(numReaders.load(std::memory_order_relaxed) != -1);

			// This is similar to startWriting, except that
			// value being negative means that this function must give up,
			// and value being 1 is the value at which to switch to -1,
			// instead of 0 being the value at which to switch to -1.

			bool success;
			intptr_t value = numReaders.load(std::memory_order_relaxed);
			size_t attempt = 0;
			do {
				if (value < -1) {
					// Another thread is waiting for all readers to stop
					// before switching to writing, so this thread can't
					// switch to writing (-1), else the other thread will
					// think that it acquired write access.
					return false;
				}
				// NOTE: value should not be -1 or 0 here, since this thread
				// is supposed to have read access, which would preclude a writer.
				if (value == 1) {
					// This thread is the only reader, so try switching to write.
					success = numReaders.compare_exchange_strong(value, intptr_t(-1), std::memory_order_acq_rel);
				}
				else {
					// This thread should have read access, and value wasn't 1,
					// so there must be at least one more.
					assert(value >= 2);

					// There are other readers, so block new readers and wait for
					// write access.
					// NOTE: The +1 is because this thread gives up its reader status when
					// it becomes the writer, i.e. when the value reaches -1.
					// value is at least 2, so newValue will be less than or equal to -2.
					intptr_t newValue = intptr_t(-1+1) - value;
					success = numReaders.compare_exchange_strong(value, newValue, std::memory_order_acq_rel);
					if (success) {
						// Wait for the value to become -1 when all readers finish.
						while (numReaders.load(std::memory_order_acquire) != intptr_t(-1))
						{
							backOff(attempt);
						}
					}
				}
			} while (!success);

			return true;
		}
	};

	std::unique_ptr<SubTable[]> subTables;

	constexpr static size_t SUB_TABLE_BITS = 12;
	constexpr static size_t NUM_SUB_TABLES = size_t(1) << SUB_TABLE_BITS;
	constexpr static size_t SUB_TABLE_MASK = NUM_SUB_TABLES - 1;

	[[nodiscard]] INLINE SubTable* getSubTableAndCode(uint64& hashCode) const noexcept {
		size_t tablei = size_t(hashCode & SUB_TABLE_MASK);
		hashCode >>= SUB_TABLE_BITS;
		return subTables.get() + tablei;
	}

	template<bool WRITE_ACCESS,typename ACCESSOR_VALUE_T,typename INTERNAL_T>
	class accessor_base {
		SubTable* subTable = nullptr;
		INTERNAL_T* item = nullptr;

		void init(SubTable* subTable_, INTERNAL_T* item_) {
			subTable = subTable_;
			item = item_;
		}

		friend BigSet;
	public:
		INLINE accessor_base() = default;

		INLINE ~accessor_base() {
			release();
		}

		using value_type = ACCESSOR_VALUE_T;
		using pointer = value_type*;
		using reference = value_type&;
		constexpr static bool isWriteAccessType = WRITE_ACCESS;

		void release() {
			if (subTable == nullptr) {
				return;
			}
			if (WRITE_ACCESS) {
				subTable->stopWriting();
			}
			else {
				subTable->stopReading();
			}
			subTable = nullptr;
			item = nullptr;
		}

		reference operator*() const {
			return reinterpret_cast<reference>(item->first);
		}
		pointer operator->() const {
			return reinterpret_cast<pointer>(&(item->first));
		}

		bool empty() const {
			return (item == nullptr);
		}
	};

public:

	using const_accessor = accessor_base<false, const VALUE_T, const TablePair>;

	// The set interface doesn't allow modifying items that are in the set,
	// in case changing them would change the hash code or make them equal
	// to other items in the set, so this only differs from
	// const_accessor in the ability to call erase, since it has write access.
	using accessor = accessor_base<true, const VALUE_T, TablePair>;

protected:
	template<typename SET_T,typename ACCESSOR_T,typename KEY_T>
	static bool findCommon(SET_T& set, ACCESSOR_T& accessor, const KEY_T& key) {
		uint64 hashCode = Hasher::hash(key);
		SubTable* subTable = set.getSubTableAndCode(hashCode);

		// If the sub-table is null or empty, we can even avoid getting read access,
		// since this should never happen, even temporarily, if an item is in the sub-table.
		if (subTable->data == nullptr || subTable->size == 0) {
			return false;
		}

		if (!ACCESSOR_T::isWriteAccessType) {
			subTable->startReading();
		}
		else {
			subTable->startWriting();
		}

		const size_t capacity = subTable->capacity;
		TablePair* data = subTable->data;

		// NOTE: findInTable will check for null again, which is necessary.
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data, capacity, hashCode, key, index, targetIndex);
		if (!found) {
			if (!ACCESSOR_T::isWriteAccessType) {
				subTable->stopReading();
			}
			else {
				subTable->stopWriting();
			}
			return false;
		}

		accessor.init(subTable, data + index);
		return true;
	}

	template<typename VALUE_REF_T,typename ACCESSOR_T>
	bool insertCommon(ACCESSOR_T* accessor, VALUE_REF_T value) {
		uint64 hashCode = Hasher::hash(value);
		SubTable* subTable = getSubTableAndCode(hashCode);

		size_t capacity;
		TablePair* data = subTable->data;
		size_t index;
		size_t targetIndex = SubTable::EMPTY_INDEX;
		bool changedFromReadToWrite = false;

		if (!ACCESSOR_T::isWriteAccessType && data != nullptr) {
			// First, check if the item is in the map via reading,
			// so that if it's very common that the item is already in the set,
			// multiple threads can check at the same time.
			subTable->startReading();

			// Re-read subTable->data, now that we have read access.
			data = subTable->data;
			capacity = subTable->capacity;

			// NOTE: findInTable will check for null again, which is necessary.
			// Use the search from findInTable to find the location
			// where an item should be inserted and also check if an equal value
			// is already in the set.
			bool found = hash::findInTable<true, Hasher>(data, capacity, hashCode, value, index, targetIndex);

			if (found) {
				if (accessor == nullptr) {
					subTable->stopReading();
				}
				else {
					accessor->init(subTable, data + index);
				}

				// It's already in the set, so value was not inserted.
				return false;
			}

			// Unfortunately, it's not always feasible to change read access into
			// write access, because another thread may be already
			// trying to acquire write access, in which case, this reader
			// keeping read access would cause a deadlock.  However, it's often
			// feasible to do so, saving some atomic operations in the common case,
			// with a fallback to stopping reading and then starting writing.
			changedFromReadToWrite = subTable->tryChangeFromReadToWrite();
			if (!changedFromReadToWrite) {
				subTable->stopReading();
			}
		}

		// The assumption is that we'll probably be inserting, but we still need
		// to check again for an inserted item if !changedFromReadToWrite,
		// since we had to remove the read access temporarily.
		if (!changedFromReadToWrite) {
			subTable->startWriting();

			capacity = subTable->capacity;
			data = subTable->data;

			bool found = hash::findInTable<true, Hasher>(data, capacity, hashCode, value, index, targetIndex);
			if (found) {
				if (accessor == nullptr) {
					subTable->stopWriting();
				}
				else {
					accessor->init(subTable, data + index);
					if (!ACCESSOR_T::isWriteAccessType) {
						subTable->changeFromWriteToRead();
					}
				}

				// It's already in the set, so value was not inserted.
				return false;
			}
		}
		assert(subTable->numReaders.load(std::memory_order_relaxed) == -1);

		size_t size = subTable->size;
		++size;
		subTable->size = size;

		if ((size > capacity) || ((index != targetIndex) && (size > (capacity>>1)))) {
			// Allocate a new table with a larger capacity and rehash.
			size_t newCapacity = hash::nextPrimeCapacity(capacity+1);
			TablePair* newData = new TablePair[newCapacity];
			for (size_t desti = 0; desti != newCapacity; ++desti) {
				newData[desti].second = SubTable::EMPTY_INDEX;
			}
			for (size_t sourcei = 0; sourcei != capacity; ++sourcei) {
				TablePair& source = data[sourcei];
				if (source.second == SubTable::EMPTY_INDEX) {
					continue;
				}
				size_t sourceHashCode = Hasher::hash(source.first);
				size_t insertIndex;
				size_t sourceTargetIndex;
				hash::findInTable<false, void>(newData, newCapacity, sourceHashCode, source.first, insertIndex, sourceTargetIndex);
				hash::insertIntoTable(newData, newCapacity, std::move(source.first), insertIndex, sourceTargetIndex);
			}
			hash::findInTable<false, void>(newData, newCapacity, hashCode, value, index, targetIndex);
			delete [] data;
			data = newData;
			subTable->data = newData;
			capacity = newCapacity;
			subTable->capacity = newCapacity;
		}

		if constexpr (std::is_rvalue_reference<VALUE_REF_T>::value) {
			hash::insertIntoTable(data, capacity, std::move(value), index, targetIndex);
		}
		else {
			hash::insertIntoTable(data, capacity, VALUE_T(value), index, targetIndex);
		}

		if (accessor == nullptr) {
			subTable->stopWriting();
		}
		else {
			accessor->init(subTable, data + index);
			if (!ACCESSOR_T::isWriteAccessType) {
				subTable->changeFromWriteToRead();
			}
		}
		return true;
	}

	template<typename ACCESSOR_T>
	bool eraseAccessor(ACCESSOR_T& accessor) {
		SubTable* subTable = accessor.subTable;
		if (subTable == nullptr) {
			// accessor is not associated with an item, so there's nothing to erase.
			assert(accessor.item == nullptr);
			return false;
		}
		assert(accessor.item != nullptr);

		TablePair* data = subTable->data;
		TablePair* current = accessor.item;
		hash::eraseFromTable(data, subTable->capacity, current, current - data);

		accessor.release();

		return true;
	}

	template<typename KEY_T>
	bool eraseInternal(const KEY_T& key) {
		uint64 hashCode = Hasher::hash(key);
		SubTable* subTable = getSubTableAndCode(hashCode);

		if (subTable->data == nullptr) {
			return false;
		}

		// For simplicity, always get write access up front,
		// since it seems unlikely that callers would be
		// frequently erasing values that are not in the map.
		subTable->startWriting();

		size_t capacity = subTable->capacity;
		TablePair* data = subTable->data;

		size_t index;
		size_t targetIndex;

		// NOTE: findInTable will check for null again, which is necessary.
		bool found = hash::findInTable<true, Hasher>(data, capacity, hashCode, key, index, targetIndex);

		if (found) {
			hash::eraseFromTable(data, capacity, data + index, index);
		}

		subTable->stopWriting();

		return found;
	}

public:
	BigSet() : subTables(new SubTable[NUM_SUB_TABLES]) {}

	// Find the value in the set and acquire a const_accessor to it.
	// If there is an equal item in the set, this returns true, else false.
	INLINE bool find(const_accessor& accessor, const VALUE_T& value) const {
		return findCommon(*this, accessor, value);
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	INLINE bool find(const_accessor& accessor, const OTHER_T& value) const {
		return findCommon(*this, accessor, value);
	}

	// Find the value in the set and acquire an accessor to it.
	// If there is an equal item in the set, this returns true, else false.
	//
	// This only differs from the const_accessor version in the ability to
	// call erase, since the set interface doesn't allow changing items,
	// in case the hash code would change.  Because it aquires write access,
	// it may also be slower if there's contention.
	INLINE bool find(accessor& accessor, const VALUE_T& value) {
		return findCommon(*this, accessor, value);
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	INLINE bool find(accessor& accessor, const OTHER_T& value) {
		return findCommon(*this, accessor, value);
	}

	// Insert the value into the set, when an accessor is not needed.
	// If the insertion succeeded, this returns true.
	// If there was already an equal item in the set, this returns false.
	INLINE bool insert(const VALUE_T& value) {
		return insertCommon<const VALUE_T&>(static_cast<const_accessor*>(nullptr), value);
	}
	INLINE bool insert(VALUE_T&& value) {
		return insertCommon<VALUE_T&&>(static_cast<const_accessor*>(nullptr), std::move(value));
	}
	template<typename OTHER_T>
	INLINE bool insert(const OTHER_T& value) {
		return insertCommon<const OTHER_T&>(static_cast<const_accessor*>(nullptr), value);
	}

	// Insert the value into the set and acquire a const_accessor to it.
	// If the insertion succeeded, this returns true.
	// If there was already an equal item in the set, a const_accessor
	// to the existing item is acquired, and this returns false.
	INLINE bool insert(const_accessor& accessor, const VALUE_T& value) {
		return insertCommon<const VALUE_T&>(&accessor, value);
	}
	INLINE bool insert(const_accessor& accessor, VALUE_T&& value) {
		return insertCommon<VALUE_T&&>(&accessor, value);
	}
	template<typename OTHER_T>
	INLINE bool insert(const_accessor& accessor, const OTHER_T& value) {
		return insertCommon<const OTHER_T&>(&accessor, value);
	}

	// Insert the value into the set and acquire an accessor to it.
	// If the insertion succeeded, this returns true.
	// If there was already an equal item in the set, an accessor
	// to the existing item is acquired, and this returns false.
	INLINE bool insert(accessor& accessor, const VALUE_T& value) {
		return insertCommon<const VALUE_T&>(&accessor, value);
	}
	INLINE bool insert(accessor& accessor, VALUE_T&& value) {
		return insertCommon<VALUE_T&&>(&accessor, std::move(value));
	}
	template<typename OTHER_T>
	INLINE bool insert(accessor& accessor, const OTHER_T& value) {
		return insertCommon<const OTHER_T&>(&accessor, value);
	}

	// Remove the item referenced by the accessor from the set.
	// If there was an item referenced by the accessor, this returns true.
	// If there was no item referenced by the accessor,
	// (and so no item was removed), this returns false.
	// Afterwards, the accessor is always not referencing an item.
	INLINE bool erase(accessor& accessor) {
		return eraseAccessor(accessor);
	}

	// Remove any item from the set that is equal to the given value.
	// If an item was removed, this returns true, else false.
	INLINE bool erase(const VALUE_T& value) {
		return eraseInternal(value);
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	INLINE bool erase(const OTHER_T& value) {
		return eraseInternal(value);
	}
};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
