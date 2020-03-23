#pragma once

#include "Types.h"
#include "HashUtil.h"

#include <assert.h>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// This is an array-based hash set, mostly compatible with std::unordered_set,
// but using a single, large allocation, instead of many, small allocations.
// It does not offer the same worst-case performance guarantees,
// but should have better average-case performance.
template<typename VALUE_T, typename Hasher = DefaultHasher<VALUE_T>>
class Set {
protected:
	using TablePair = hash::Pair<VALUE_T>;

	std::unique_ptr<TablePair[]> data;
	size_t capacity;
	size_t size_;

	template<typename ITERATOR_VALUE_T>
	class iterator_base {
		ITERATOR_VALUE_T* current;
		const TablePair* end;

		INLINE iterator_base(ITERATOR_VALUE_T* current_, const TablePair* end_) noexcept : current(current_), end(end_) {}

		friend Set;
	public:
		using ThisType = iterator_base<ITERATOR_VALUE_T>;

		INLINE iterator_base() noexcept = default;
		INLINE iterator_base(ThisType&& that) noexcept = default;
		INLINE iterator_base(const ThisType&) noexcept = default;
		INLINE ~iterator_base() noexcept = default;

		INLINE ThisType& operator=(ThisType&& that) noexcept = default;
		INLINE ThisType& operator=(const ThisType&) noexcept = default;

		using difference_type = ptrdiff_t;
		using value_type = decltype(current->first);
		using pointer = decltype(current->first)*;
		using reference = decltype(current->first)&;
		using iterator_category = std::forward_iterator_tag;

		// Prefix increment: use this instead of postfix increment.
		INLINE iterator_base& operator++() {
			assert(current < end);
			do {
				++current;
			} while(current != end && current->second != hash::EMPTY_INDEX);
			return *this;
		}

		// Postfix increment: DO NOT USE THIS!!!  It's unnecessary and a performance hit.
		// It's marked as nodiscard to ensure that in most cases where it's
		// accidentally used, it'll generate a compile error.
		[[nodiscard]] iterator_base operator++(int) {
			assert(false && "DO NOT USE POSTFIX INCREMENT");
			iterator_base ret = *this;
			++(*this);
			return ret;
		}

		[[nodiscard]] INLINE bool operator==(const ThisType& that) const {
			assert(current <= end);
			assert(that.current <= that.end);
			assert(end == that.end);
			return (current == that.current);
		}
		[[nodiscard]] INLINE bool operator!=(const ThisType& that) const {
			return !(*this == that);
		}

		[[nodiscard]] INLINE reference operator*() const {
			assert(current < end);
			return current->first;
		}
		[[nodiscard]] INLINE pointer operator->() const {
			assert(current < end);
			return &(current->first);
		}

		[[nodiscard]] INLINE bool isEnd() const {
			assert(current <= end);
			return current == end;
		}
	};

public:
	// The set interface doesn't allow modifying items that are in the set,
	// in case changing them would change the hash code or make them equal
	// to other items in the set, so there is no difference between iterator
	// and const_iterator.
	using const_iterator = iterator_base<const TablePair>;
	using iterator = const_iterator;

	using key_type = VALUE_T;
	using value_type = VALUE_T;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = value_type*;
	using const_pointer = const value_type*;

protected:

	template<typename VALUE_REF_T,typename ITERATOR_T>
	std::pair<ITERATOR_T,bool> insertCommon(VALUE_REF_T value) {
		uint64 hashCode = Hasher::hash(value);

		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, hashCode, value, index, targetIndex);
		if (found) {
			// It's already in the set, so value was not inserted.
			return std::make_pair(
				ITERATOR_T(data.get() + index, data.get() + capacity),
				false
			);
		}

		++size_;

		if ((size_ > capacity) || ((index != targetIndex) && (size_ > (capacity>>1)))) {
			// Allocate a new table with a larger capacity and rehash.
			size_t newCapacity = hash::nextPrimeCapacity(capacity+1);
			TablePair* newData = new TablePair[newCapacity];
			for (size_t desti = 0; desti != newCapacity; ++desti) {
				newData[desti].second = hash::EMPTY_INDEX;
			}
			for (size_t sourcei = 0; sourcei != capacity; ++sourcei) {
				TablePair& source = data[sourcei];
				if (source.second == hash::EMPTY_INDEX) {
					continue;
				}
				size_t sourceHashCode = Hasher::hash(source.first);
				size_t insertIndex;
				size_t sourceTargetIndex;
				hash::findInTable<false, void>(newData, newCapacity, sourceHashCode, source.first, insertIndex, sourceTargetIndex);
				hash::insertIntoTable(newData, newCapacity, std::move(source.first), insertIndex, sourceTargetIndex);
			}
			hash::findInTable<false, void>(newData, newCapacity, hashCode, value, index, targetIndex);
			data.reset(newData);
			capacity = newCapacity;
		}

		hash::insertIntoTable(data.get(), capacity, std::move(value), index, targetIndex);

		return std::make_pair(
			ITERATOR_T(data.get() + index, data.get() + capacity),
			true
		);
	}

public:
	INLINE Set() noexcept : data(nullptr), size_(0), capacity(0) {}
	INLINE ~Set() noexcept = default;

	INLINE Set(Set&& that) noexcept : data(that.data.release()), size_(that.size_), capacity(that.capacity) {
		that.size_ = 0;
		that.capacity = 0;
	}

	INLINE Set& operator=(Set&& that) noexcept {
		data.reset(that.data.release());
		size_ = that.size_;
		capacity = that.capacity_;
		that.size_ = 0;
		that.capacity = 0;
		return *this;
	}

	INLINE void swap(Set& that) noexcept {
		data.swap(that.data);
		size_t s = size_;
		size_ = that.size_;
		that.size_ = s;
		size_t c = capacity;
		capacity = that.capacity;
		that.capacity = c;
	}

	[[nodiscard]] INLINE const_iterator begin() const noexcept {
		const TablePair* p = data.get();
		const TablePair* pend = p + capacity;
		if (size_ == 0) {
			return const_iterator(pend, pend);
		}
		while (p->second == hash::EMPTY_INDEX) {
			++p;
		}
		return const_iterator(p, pend);
	}
	[[nodiscard]] INLINE const_iterator end() const noexcept {
		const TablePair* p = data.get() + capacity;
		return const_iterator(p, p);
	}
	[[nodiscard]] INLINE const_iterator cbegin() const noexcept {
		return begin();
	}
	[[nodiscard]] INLINE const_iterator cend() const noexcept {
		return end();
	}


	// NOTE: This does not empty the set!  It returns true iff the set is already empty.
	// This function is just for compatibility with the standard set interface.
	// Call clear() to empty the set.
	[[nodiscard]] INLINE bool empty() const noexcept {
		return size_ != 0;
	}

	[[nodiscard]] INLINE size_t size() const noexcept {
		return size_;
	}

	[[nodiscard]] static constexpr INLINE size_t max_size() noexcept {
		return std::numeric_limits<size_t>::max();
	}

	void clear() noexcept {
		if (size_ == 0) {
			// Don't bother iterating, since the set is already empty.
			return;
		}
		const TablePair* current = data.get();
		const TablePair* end = current + capacity;
		for (; current != end; ++current) {
			if (current->second != hash::EMPTY_INDEX) {
				// Not empty, so replace with default-constructed element
				// and the empty marker.
				current->first = VALUE_T();
				current->second = hash::EMPTY_INDEX;
			}
		}
		size_ = 0;
	}

	[[nodiscard]] const_iterator find(const VALUE_T& value) const noexcept {
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(value), value, index, targetIndex);
		return const_iterator(
			data.get() + (found ? index : capacity),
			data.get() + capacity
		);
	}
	[[nodiscard]] INLINE bool contains(const VALUE_T& value) const noexcept {
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(value), value, index, targetIndex);
		return found;
	}
	[[nodiscard]] INLINE size_t count(const VALUE_T& value) const noexcept {
		return contains(value) ? 1 : 0;
	}
	
	// There can be at most 1 item equal to value in the set, so this is either
	// an empty range or a range containing 1 item.
	[[nodiscard]] std::pair<const_iterator,const_iterator> equal_range(const VALUE_T& value) const noexcept {
		const_iterator it = find(value);
		if (it.isEnd()) {
			return std::make_pair(it, it);
		}
		const_iterator next = it;
		++next;
		return std::make_pair(it, next);
	}

	std::pair<const_iterator,bool> insert(const VALUE_T& value) noexcept {
		return insertCommon<const VALUE_T&, const_iterator>(value);
	}

	std::pair<const_iterator,bool> insert(VALUE_T&& value) noexcept {
		return insertCommon<VALUE_T&&, const_iterator>(std::move(value));
	}

	template<typename INPUT_ITER>
	void insert(INPUT_ITER it, const INPUT_ITER endIt) noexcept {
		for (; it != endIt; ++it) {
			insert(*it);
		}
	}
	void insert(std::initializer_list<VALUE_T> list) noexcept {
		for (auto it = list.begin(), end = list.end(); it != end; ++it) {
			insert(*it);
		}
	}

	const_iterator erase(const_iterator it) noexcept {
		assert(!it.isEnd() && it.current->second != hash::EMPTY_INDEX);
		TablePair* pbegin = data.get();
		size_t index = it.current - pbegin;
		TablePair* current = pbegin + index;
		hash::eraseFromTable(pbegin, capacity, current, index);
		--size_;

		// Find next item if the current bucket hasn't become occupied
		// by a later item.
		const TablePair*const pend = pbegin + capacity;
		while (current->second == hash::EMPTY_INDEX) {
			++current;
			if (current != pend) {
				break;
			}
		}
		return const_iterator(current, pend);
	}
	size_t erase(const VALUE_T& value) noexcept {
		TablePair* pbegin = data.get();
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(pbegin, capacity, Hasher::hash(value), value, index, targetIndex);
		if (!found) {
			return 0;
		}

		hash::eraseFromTable(pbegin, capacity, pbegin + index, index);
		return 1;
	}

	[[nodiscard]] INLINE size_t bucket_count() const noexcept {
		return capacity;
	}

	[[nodiscard]] static constexpr INLINE size_t max_bucket_count() noexcept {
		return std::numeric_limits<size_t>::max();
	}

	[[nodiscard]] float load_factor() const noexcept {
		return (capacity == 0) ? 0.0f : (float(size_)/float(capacity));
	}
	[[nodiscard]] static constexpr INLINE float max_load_factor() noexcept {
		return 0.5f;
	}

	void rehash(size_t newCapacity) noexcept {
		if (newCapacity < 2*size_) {
			newCapacity = 2*size_;
		}
		TablePair* newData = new TablePair[newCapacity];
		for (size_t desti = 0; desti != newCapacity; ++desti) {
			newData[desti].second = hash::EMPTY_INDEX;
		}
		if (size_ != 0) {
			TablePair* current = data.get();
			const TablePair* end = current + capacity;
			for ( ; current != end; ++current) {
				if (current->second == hash::EMPTY_INDEX) {
					continue;
				}
				TablePair& source = *current;
				size_t sourceHashCode = Hasher::hash(source.first);
				size_t insertIndex;
				size_t sourceTargetIndex;
				hash::findInTable<false, void>(newData, newCapacity, sourceHashCode, source.first, insertIndex, sourceTargetIndex);
				hash::insertIntoTable(newData, newCapacity, std::move(source.first), insertIndex, sourceTargetIndex);
			}
		}
		data.reset(newData);
		capacity = newCapacity;
	}
	INLINE void reserve(size_t targetSize) noexcept {
		rehash(2*targetSize);
	}
};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END