#pragma once

#include "Types.h"

#include <assert.h>
#include <atomic>
#include <memory>
#include <utility>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// This is a hash set designed for very large numbers of items
// being looked up and added from many threads simultaneously.
// It has a very large unfront memory footprint, so is best suited
// to situations where there is a single large set, like a global set
// of unique string representatives.
template<typename VALUE_T, typename Hasher>
class BigSet {
protected:
	// To reduce memory contention, make sure
	// that each of the atomic values is in a separate
	// 64-byte primary cache line.
	struct alignas(64) SubTable {
		volatile std::atomic<intptr_t> numReaders;
		volatile size_t size;
		volatile size_t capacity;

		// The size_t of each pair is the current target index in this sub-table.
		volatile std::pair<VALUE_T,size_t>*volatile data;

		constexpr static size_t EMPTY_INDEX = ~size_t(0);

		static void backOff(size_t& attempt) {
			if (attempt < 16) {
				++attempt;
				return;
			}
			ReleaseTimeSlice();
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
	};

	std::unique_ptr<SubTable[]> subTables;

	constexpr static size_t SUB_TABLE_BITS = 12;
	constexpr static size_t NUM_SUB_TABLES = size_t(1) << SUB_TABLE_BITS;
	constexpr static size_t SUB_TABLE_MASK = NUM_SUB_TABLES - 1;

	template<bool WRITE_ACCESS>
	class accessor_base {
		SubTable* subTable = nullptr;
		const VALUE_T* item = nullptr;

		void init(SubTable* subTable_, const VALUE_T* item_) {
			subTable = subTable_;
			item = item_;
		}
	public:
		INLINE accessor_base() = default;

		INLINE ~accessor_base() {
			release();
		}

		using value_type = const VALUE_T;

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

		const VALUE_T& operator*() const {
			return *item;
		}
		const VALUE_T* operator->() const {
			return item;
		}

		bool empty() const {
			return (item == nullptr);
		}
	};

	static size_t findInTable(const std::pair<VALUE_T,size_t>*const begin, size_t capacity, uint64 hashCode, const VALUE_T& value) {
		if (begin == nullptr) {
			assert(capacity == 0);
			return 0;
		}
		assert(capacity != 0);

		const size_t targetIndex = size_t(hashCode % capacity);
		size_t currentIndex = targetIndex;

		const std::pair<VALUE_T,size_t>* current = begin + currentIndex;
		size_t currentTargetIndex = current->second;

		// The most common cases are immediately hitting an empty slot
		// or immediately finding a match.
		if (currentTargetIndex == SubTable::EMPTY_INDEX) {
			// Empty slot, so not found
			return capacity;
		}
		if ((currentTargetIndex == targetIndex) && Hasher::equals(current->first, value)) {
			// Found an equal item, so return its index.
			return currentIndex;
		}

		// If the starting index is in a wrap-around region, skip it
		// by finding the first item with a lower target index.
		bool insideWrapAround = (currentTargetIndex > targetIndex);
		if (insideWrapAround) {
			do {
				++current;
				++currentIndex;

				// NOTE: currentIndex should never reach the capacity here,
				// since a wrap-around region should never reach the end.
				assert(currentIndex != capacity);
				// NOTE: currentIndex also should never reach targetIndex,
				// since it doesn't wrap back there
				assert(currentIndex != targetIndex);

				size_t newTargetIndex = current->second;
				insideWrapAround = (newTargetIndex >= currentTargetIndex);
				currentTargetIndex = newTargetIndex;
			} while (insideWrapAround);
		}
		else {
			++current;
			++currentIndex;
			if (currentIndex == capacity) {
				currentIndex = 0;
				current = begin;
			}
			if (currentIndex == targetIndex) {
				// Wrapped all the way around, (table must be capacity 1),
				// so item wasn't found.
				return capacity;
			}
			currentTargetIndex = current->second;
		}

		// NOTE: The empty slot index condition is actually redundant,
		// since the value of EMPTY_INDEX is gerater than any targetIndex.
		while (currentTargetIndex <= targetIndex && currentTargetIndex != SubTable::EMPTY_INDEX) {
			if ((currentTargetIndex == targetIndex) && Hasher::equals(current->first, value)) {
				// Found an equal item, so return its index.
				return currentIndex;
			}

			++current;
			++currentIndex;
			if (currentIndex == capacity) {
				currentIndex = 0;
				current = begin;
			}
			if (currentIndex == targetIndex) {
				// Wrapped all the way around, (table must be completely full),
				// so item wasn't found.
				return capacity;
			}
			currentTargetIndex = current->second;
		}


		// Not in a wrap around and reached something that belongs
		// after the query value or empty slot.
		return capacity;
	}

	SubTable* getSubTableAndCode(uint64& hashCode) const {
		SubTable* subTable = subTables[hashCode & SUB_TABLE_MASK];
		hashCode >>= SUB_TABLE_BITS;
		return subTable;
	}

	template<typename SET_T,typename ACCESSOR_T>
	static bool findCommon(SET_T& set, ACCESSOR_T& accessor, const VALUE_T& value) {
		uint64 hashCode = Hasher::hash(value);
		SubTable* subTable = set.getSubTableAndCode(hashCode);

		// If the sub-table is null or empty, we can even avoid getting read access,
		// since this should never happen, even temporarily, if an item is in the sub-table.
		if (subTable->data == nullptr || subTable->size == 0) {
			return false;
		}

		if (std::is_same<ACCESSOR_T,const_accessor>::value) {
			subTable->startReading();
		}
		else {
			subTable->startWriting();
		}

		const size_t capacity = subTable->capacity;
		const std::pair<VALUE_T,size_t>* data = subTable->data;

		// NOTE: findInTable will check for null again, which is necessary.
		size_t index = findInTable(data, capacity, hashCode, value);
		if (index >= capacity) {
			if (std::is_same<ACCESSOR_T,const_accessor>::value) {
				subTable->stopReading();
			}
			else {
				subTable->stopWriting();
			}
			return false;
		}

		accessor.init(subTable, &data[index].first);
		return true;
	}

	bool insertCommon(const_accessor* accessor, const VALUE_T& value) {
		assert(0);
		// FIXME: Implement this!!!
	}

public:
	BigSet() : subTables(new SubTable[NUM_SUB_TABLES]) {}

	using const_accessor = accessor_base<false>;

	// The set interface doesn't allow modifying items that are in the set,
	// in case changing them would change the hash code or make them equal
	// to other items in the map, so this only differs from
	// const_accessor in the ability to call erase, since it has write access.
	using accessor = accessor_base<true>;

	// Find the value in the set and acquire a const_accessor to it.
	// If there is an equal item in the set, this returns true, else false.
	bool find(const_accessor& accessor, const VALUE_T& value) const {
		return findCommon(*this, accessor, value);
	}

	// Find the value in the set and acquire an accessor to it.
	// If there is an equal item in the set, this returns true, else false.
	//
	// This only differs from the const_accessor version in the ability to
	// call erase, since the set interface doesn't allow changing items,
	// in case the hash code would change.  Because it aquires write access,
	// it may also be slower if there's contention.
	bool find(accessor& accessor, const VALUE_T& value) {
		return findCommon(*this, accessor, value);
	}

	// Insert the value into the set and acquire a const_accessor to it.
	// If the insertion succeeded, this returns true.
	// If there was already an equal item in the set, a const_accessor
	// to the existing item is acquired, and this returns false.
	bool insert(const_accessor& accessor, const VALUE_T& value) {
		return insertCommon(&accessor, value);
	}

	// Insert the value into the set, when an accessor is not needed.
	// If the insertion succeeded, this returns true.
	// If there was already an equal item in the set, this returns false.
	bool insert(const VALUE_T& value) {
		return insertCommon(nullptr, value);
	}

	// Remove the item referenced by the const_accessor from the set.
	// If there was an item referenced by the const_accessor, this returns true.
	// If there was no item referenced by the const_accessor,
	// (and so no item was removed), this returns false.
	// Afterwards, the const_accessor is always not referencing an item.
	bool erase(accessor& accessor);

	// Remove any item from the set that is equal to the given value.
	// If an item was removed, this returns true, else false.
	bool erase(const VALUE_T& value);
};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
