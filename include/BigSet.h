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
template<typename VALUE_T, typename Hasher = DefaultHasher<VALUE_T>>
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
		using Pair = std::pair<VALUE_T,size_t>;
		Pair*volatile data;

		constexpr static size_t EMPTY_INDEX = ~size_t(0);

		SubTable() : numReaders(0), size(0), capacity(0), data(nullptr) {}

		~SubTable() {
			assert(numReaders.load(std::memory_order_relaxed) == 0);
			delete [] data;
		}

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
					// There are other readers, so block new readers and wait for
					// write access.
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

			return true;
		}
	};

	std::unique_ptr<SubTable[]> subTables;

	constexpr static size_t SUB_TABLE_BITS = 12;
	constexpr static size_t NUM_SUB_TABLES = size_t(1) << SUB_TABLE_BITS;
	constexpr static size_t SUB_TABLE_MASK = NUM_SUB_TABLES - 1;

	template<bool WRITE_ACCESS>
	class accessor_base {
		SubTable* subTable = nullptr;
		const typename SubTable::Pair* item = nullptr;

		void init(SubTable* subTable_, const typename SubTable::Pair* item_) {
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
			return item->first;
		}
		const VALUE_T* operator->() const {
			return &(item->first);
		}

		bool empty() const {
			return (item == nullptr);
		}
	};

public:

	using const_accessor = accessor_base<false>;

	// The set interface doesn't allow modifying items that are in the set,
	// in case changing them would change the hash code or make them equal
	// to other items in the map, so this only differs from
	// const_accessor in the ability to call erase, since it has write access.
	using accessor = accessor_base<true>;

protected:

	template<bool CHECK_EQUAL>
	static bool findInTable(const typename SubTable::Pair*const begin, const size_t capacity, uint64 hashCode, const VALUE_T& value, size_t& index, size_t& targetIndex) {
		if (begin == nullptr) {
			assert(capacity == 0);
			index = SubTable::EMPTY_INDEX;
			return false;
		}
		assert(capacity != 0);

		targetIndex = size_t(hashCode % capacity);
		size_t currentIndex = targetIndex;

		const typename SubTable::Pair* current = begin + currentIndex;
		size_t currentTargetIndex = current->second;

		// The most common cases are immediately hitting an empty slot
		// or immediately finding a match.
		if (currentTargetIndex == SubTable::EMPTY_INDEX) {
			// Empty slot, so not found
			index = currentIndex;
			return false;
		}
		if (CHECK_EQUAL && (currentTargetIndex == targetIndex) && Hasher::equals(current->first, value)) {
			// Found an equal item, so return its index.
			index = currentIndex;
			return true;
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
				// This check for empty index is necessary here,
				// since EMPTY_INDEX is greater than any valid target index.
				if (newTargetIndex == SubTable::EMPTY_INDEX) {
					index = currentIndex;
					return false;
				}
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
				index = currentIndex;
				return false;
			}
			currentTargetIndex = current->second;
		}

		// NOTE: The empty slot index condition is actually redundant,
		// since the value of EMPTY_INDEX is gerater than any targetIndex.
		while (currentTargetIndex <= targetIndex && currentTargetIndex != SubTable::EMPTY_INDEX) {
			if (CHECK_EQUAL && (currentTargetIndex == targetIndex) && Hasher::equals(current->first, value)) {
				// Found an equal item, so return its index.
				index = currentIndex;
				return true;
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
				index = currentIndex;
				return false;
			}
			currentTargetIndex = current->second;
		}


		// Not in a wrap around and reached something that belongs
		// after the query value or empty slot.
		index = currentIndex;
		return false;
	}

	static void insertIntoTable(typename SubTable::Pair*const begin, const size_t capacity, VALUE_T&& value, const size_t index, const size_t targetIndex) {
		// Insert (value,targetIndex) at index and shift anything down to make room, if needed.
		assert(index < capacity);

		typename SubTable::Pair* current = begin + index;
		size_t currentIndex = index;
		if (current->second == SubTable::EMPTY_INDEX) {
			// Empty space right away, so just write it.
			current->first = std::move(value);
			current->second = targetIndex;
			return;
		}

		typename SubTable::Pair previous = std::move(*current);
		current->first = std::move(value);
		current->second = targetIndex;

		// Shift pairs forward.
		while (true) {
			++current;
			++currentIndex;
			if (currentIndex == capacity) {
				current = begin;
				currentIndex = 0;
			}
			// If we've reached targetIndex, there's something wrong with the table.
			assert(currentIndex != targetIndex);
			if (current->second == SubTable::EMPTY_INDEX) {
				// Found empty space, so write the final pair.
				*current = std::move(previous);
				break;
			}
			std::swap(previous, *current);
		}
	}

	static void eraseFromTable(typename SubTable::Pair*const begin, const size_t capacity, typename SubTable::Pair* current, size_t currentIndex) {
		assert(current->second != SubTable::EMPTY_INDEX);

		while (true) {
			typename SubTable::Pair* next = current + 1;
			size_t nextIndex = currentIndex + 1;
			if (nextIndex == capacity) {
				nextIndex = 0;
				next = begin;
			}

			size_t nextTargetIndex = next->second;
			if (nextTargetIndex == SubTable::EMPTY_INDEX || nextTargetIndex == nextIndex) {
				// Empty item or item in correct place, so this is the
				// end of the span of displaced items.
				break;
			}

			// Move the next item back.
			*current = std::move(*next);

			current = next;
			currentIndex = nextIndex;
		}

		// Overwrite last item in span to clear it.
		current->first = VALUE_T();
		current->second = SubTable::EMPTY_INDEX;
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
		const typename SubTable::Pair* data = subTable->data;

		// NOTE: findInTable will check for null again, which is necessary.
		size_t index;
		size_t targetIndex;
		bool found = findInTable<true>(data, capacity, hashCode, value, index, targetIndex);
		if (!found) {
			if (std::is_same<ACCESSOR_T,const_accessor>::value) {
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

	bool insertCommon(const_accessor* accessor, const VALUE_T& value) {
		uint64 hashCode = Hasher::hash(value);
		SubTable* subTable = getSubTableAndCode(hashCode);

		size_t capacity;
		const typename SubTable::Pair* data;
		size_t index;
		size_t targetIndex = SubTable::EMPTY_INDEX;
		bool changedFromReadToWrite = false;

		if (subTable->data != nullptr) {
			// First, check if the item is in the map via reading,
			// so that if it's very common that the item is already in the set,
			// multiple threads can check at the same time.
			subTable->startReading();

			capacity = subTable->capacity;
			data = subTable->data;

			// NOTE: findInTable will check for null again, which is necessary.
			// Use the search from findInTable to find the location
			// where an item should be inserted and also check if an equal value
			// is already in the set.
			bool found = findInTable<true>(data, capacity, hashCode, value, index, targetIndex);

			if (found) {
				accessor.init(subTable, data + index);

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

			bool found = findInTable<true>(data, capacity, hashCode, value, index, targetIndex);
			if (found) {
				accessor.init(subTable, data + index);

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
			size_t newCapacity = nextHashPrime(capacity+1);
			typename SubTable::Pair* newData = new typename SubTable::Pair[newCapacity];
			for (size_t desti = 0; desti != newCapacity; ++desti) {
				newData[desti].second = SubTable::EMPTY_INDEX;
			}
			for (size_t sourcei = 0; sourcei != capacity; ++sourcei) {
				typename SubTable::Pair& source = data[sourcei];
				size_t sourceHashCode = Hasher::hash(source);
				size_t insertIndex;
				size_t sourceTargetIndex;
				findInTable<false>(newData, newCapacity, sourceHashCode, source.first, insertIndex, sourceTargetIndex);
				insertIntoTable(newData, newCapacity, std::move(source), insertIndex, sourceTargetIndex);
			}
			findInTable<false>(newData, newCapacity, hashCode, value, index, targetIndex);
			delete [] data;
			data = newData;
			subTable->data = newData;
			capacity = newCapacity;
			subTable->capacity = newCapacity;
		}

		insertIntoTable(data, capacity, std::move(value), index, targetIndex);

		if (accessor == nullptr) {
			subTable->stopWriting();
		}
		else {
			accessor->init(subTable, data + index);
			subTable->changeFromWriteToRead();
		}
	}

	bool eraseInternal(accessor& accessor) {
		SubTable* subTable = accessor.subTable;
		if (subTable == nullptr) {
			// accessor is not associated with an item, so there's nothing to erase.
			assert(accessor.item == nullptr);
			return false;
		}
		assert(accessor.item != nullptr);

		typename SubTable::Pair* data = subTable->data;
		typename SubTable::Pair* current = accessor.item;
		eraseFromTable(data, subTable->capacity, current, current - data);

		accessor.release();

		return true;
	}

	bool eraseInternal(const VALUE_T& value) {
		uint64 hashCode = Hasher::hash(value);
		SubTable* subTable = getSubTableAndCode(hashCode);

		if (subTable->data == nullptr) {
			return false;
		}

		// For simplicity, always get write access up front,
		// since it seems unlikely that callers would be
		// frequently erasing values that are not in the map.
		subTable->startWriting();

		size_t capacity = subTable->capacity;
		typename SubTable::Pair* data = subTable->data;

		size_t index;
		size_t targetIndex;

		// NOTE: findInTable will check for null again, which is necessary.
		bool found = findInTable<true>(data, capacity, hashCode, value, index, targetIndex);

		if (found) {
			eraseFromTable(data, capacity, data + index, index);
		}

		subTable->stopWriting();

		return found;
	}

public:
	BigSet() : subTables(new SubTable[NUM_SUB_TABLES]) {}

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

	// Remove the item referenced by the accessor from the set.
	// If there was an item referenced by the accessor, this returns true.
	// If there was no item referenced by the accessor,
	// (and so no item was removed), this returns false.
	// Afterwards, the accessor is always not referencing an item.
	bool erase(accessor& accessor) {
		return eraseInternal(accessor);
	}

	// Remove any item from the set that is equal to the given value.
	// If an item was removed, this returns true, else false.
	bool erase(const VALUE_T& value) {
		return eraseInternal(value);
	}
};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
