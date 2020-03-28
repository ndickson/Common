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
// but should have better average-case performance, especially for small sets.
//
// This currently does not have emplace, any functions accepting iterator hints,
// or the erase signature accepting two iterators, though these may be added later.
//
// It also does not have bucket iterator functions, node manipulation functions,
// allocator-object-related functions, or hasher-object-related functions,
// because these are not applicable.
//
// Also unlike std::unordered_set, Hasher is required to contain two static functions:
// uint64 hash(const VALUE_T&);
// bool equals(const VALUE_T&, const VALUE_T&);
template<typename VALUE_T, typename Hasher = DefaultHasher<VALUE_T>>
class Set {
protected:
	using TablePair = hash::Pair<VALUE_T>;

	std::unique_ptr<TablePair[]> data;
	size_t capacity;
	size_t size_;

	template<typename ITERATOR_VALUE_T,typename INTERNAL_T>
	class iterator_base {
	protected:
		INTERNAL_T* current;
		const TablePair* end;

		INLINE iterator_base(INTERNAL_T* current_, const TablePair* end_) noexcept : current(current_), end(end_) {
			// The iterator should either be an end iterator or point to a non-empty item slot.
			assert(current == end || current->second != hash::EMPTY_INDEX);
		}

		friend Set;
	public:
		using ThisType = iterator_base<ITERATOR_VALUE_T,INTERNAL_T>;

		INLINE iterator_base() noexcept = default;
		INLINE iterator_base(ThisType&& that) noexcept = default;
		INLINE iterator_base(const ThisType&) noexcept = default;
		INLINE ~iterator_base() noexcept = default;

		INLINE ThisType& operator=(ThisType&& that) noexcept = default;
		INLINE ThisType& operator=(const ThisType&) noexcept = default;

		using difference_type = ptrdiff_t;
		using value_type = ITERATOR_VALUE_T;
		using pointer = value_type*;
		using reference = value_type&;
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
		// This allows comparison between iterator and const_iterator,
		// where they differ in subclasses.
		template<typename OTHER_VALUE_T,typename OTHER_INTERNAL_T>
		[[nodiscard]] INLINE bool operator==(const iterator_base<OTHER_VALUE_T,OTHER_INTERNAL_T>& that) const {
			assert(current <= end);
			assert(that.current <= that.end);
			assert(end == that.end);
			return (current == that.current);
		}
		[[nodiscard]] INLINE bool operator!=(const ThisType& that) const {
			return !(*this == that);
		}
		template<typename OTHER_VALUE_T,typename OTHER_INTERNAL_T>
		[[nodiscard]] INLINE bool operator!=(const iterator_base<OTHER_VALUE_T,OTHER_INTERNAL_T>& that) const {
			return !(*this == that);
		}

		[[nodiscard]] INLINE reference operator*() const {
			assert(current < end);
			return reinterpret_cast<reference>(current->first);
		}
		[[nodiscard]] INLINE pointer operator->() const {
			assert(current < end);
			return reinterpret_cast<pointer>(&(current->first));
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
	using const_iterator = iterator_base<const VALUE_T,const TablePair>;
	using iterator = const_iterator;

	using key_type = VALUE_T;
	using value_type = VALUE_T;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = value_type*;
	using const_pointer = const value_type*;

	using hasher = Hasher;

protected:

	void increaseCapacity() noexcept {
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
		data.reset(newData);
		capacity = newCapacity;
	}

	template<typename VALUE_REF_T,typename ITERATOR_T>
	std::pair<ITERATOR_T,bool> insertCommon(VALUE_REF_T value) noexcept {
		uint64 hashCode = Hasher::hash(value);

		hash::Pair<VALUE_T>* p = data.get();
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(p, capacity, hashCode, value, index, targetIndex);
		if (found) {
			// It's already in the set, so value was not inserted.
			return std::make_pair(
				ITERATOR_T(p + index, p + capacity),
				false
			);
		}

		++size_;

		// If there's no collision, no need to increase the capacity, unless the capacity is zero.
		if ((size_ > capacity) || ((index != targetIndex) && (size_ > (capacity>>1)))) {
			increaseCapacity();
			p = data.get();
			hash::findInTable<false, void>(p, capacity, hashCode, value, index, targetIndex);
		}

		if constexpr (std::is_rvalue_reference<VALUE_REF_T>::value) {
			hash::insertIntoTable(p, capacity, std::move(value), index, targetIndex);
		}
		else {
			hash::insertIntoTable(p, capacity, VALUE_T(value), index, targetIndex);
		}

		return std::make_pair(
			ITERATOR_T(p + index, p + capacity),
			true
		);
	}

	template<typename OUTPUT_ITERATOR_T,typename INPUT_ITERATOR_T>
	OUTPUT_ITERATOR_T eraseCommon(const INPUT_ITERATOR_T& it) noexcept {
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
		return OUTPUT_ITERATOR_T(current, pend);
	}

	void copyTableFrom(const TablePair* source) noexcept {
		TablePair* p = data.get();
		const TablePair* pend = p + capacity;
		for (; p != pend; ++p, ++source) {
			*p = *source;
		}
	}
	void markNewEmpty() noexcept {
		TablePair* p = data.get();
		const TablePair* pend = p + capacity;
		for (; p != pend; ++p) {
			p->second = hash::EMPTY_INDEX;
		}
	}

public:
	INLINE Set() noexcept : data(nullptr), size_(0), capacity(0) {}
	INLINE ~Set() noexcept = default;

	INLINE Set(Set&& that) noexcept : data(that.data.release()), size_(that.size_), capacity(that.capacity) {
		that.size_ = 0;
		that.capacity = 0;
	}

	explicit Set(const Set& that) noexcept : data(new TablePair[that.capacity]), size_(that.size_), capacity(that.capacity) {
		copyTableFrom(that.data.get());
	}

	explicit Set(size_t bucketCount) noexcept :
		data((bucketCount == 0) ? nullptr : new TablePair[bucketCount]),
		size_(0), capacity(bucketCount)
	{
		markNewEmpty();
	}

	template<typename INPUT_ITER>
	Set(INPUT_ITER it, const INPUT_ITER endIt, size_t bucketCount = 0) noexcept : Set(bucketCount) {
		insert(it, endIt);
	}

	explicit Set(std::initializer_list<VALUE_T> list, size_t bucketCount = 0) noexcept : Set(bucketCount) {
		insert(list);
	}

	INLINE Set& operator=(Set&& that) noexcept {
		data.reset(that.data.release());
		size_ = that.size_;
		capacity = that.capacity_;
		that.size_ = 0;
		that.capacity = 0;
		return *this;
	}

	Set& operator=(const Set& that) noexcept {
		if (this == &that) {
			return;
		}
		if (that.data.get() == nullptr) {
			assert(that.capacity == 0);
			assert(that.size == 0);
			if (data.get() != nullptr) {
				data.reset();
				capacity = 0;
				size_ = 0;
			}
			return *this;
		}
		if (capacity != that.capacity) {
			assert(that.capacity != 0);
			data.reset(new TablePair[that.capacity]);
			capacity = that.capacity;
		}
		copyTableFrom(that.data.get());
		size_ = that.size_;
		return *this;
	}

	Set& operator=(std::initializer_list<VALUE_T> list) noexcept {
		clear();
		insert(std::move(list));
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

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	[[nodiscard]] const_iterator find(const OTHER_T& value) const noexcept {
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

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	[[nodiscard]] INLINE bool contains(const OTHER_T& value) const noexcept {
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(value), value, index, targetIndex);
		return found;
	}

	[[nodiscard]] INLINE size_t count(const VALUE_T& value) const noexcept {
		return contains(value) ? 1 : 0;
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	[[nodiscard]] INLINE size_t count(const OTHER_T& value) const noexcept {
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

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	[[nodiscard]] std::pair<const_iterator,const_iterator> equal_range(const OTHER_T& value) const noexcept {
		const_iterator it = find(value);
		if (it.isEnd()) {
			return std::make_pair(it, it);
		}
		const_iterator next = it;
		++next;
		return std::make_pair(it, next);
	}

	INLINE std::pair<const_iterator,bool> insert(const VALUE_T& value) noexcept {
		return insertCommon<const VALUE_T&, const_iterator>(value);
	}

	INLINE std::pair<const_iterator,bool> insert(VALUE_T&& value) noexcept {
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

	INLINE const_iterator erase(const const_iterator& it) noexcept {
		return eraseCommon<const_iterator,const_iterator>(it);
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
		--size_;
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

template<typename VALUE_T, typename Hasher>
bool operator==(const Set<VALUE_T,Hasher>& a, const Set<VALUE_T,Hasher>& b) {
	if (&a == &b) {
		return true;
	}
	if (a.size() != b.size()) {
		return false;
	}
	if (a.size() == 0) {
		// No need to iterate if both sets are empty.
		return true;
	}
	for (auto it = a.begin(), endIt = a.end(); it != endIt; ++it) {
		if (!b.contains(*it)) {
			return false;
		}
	}
	return true;
}

template<typename VALUE_T, typename Hasher>
INLINE bool operator!=(const Set<VALUE_T,Hasher>& a, const Set<VALUE_T,Hasher>& b) {
	return !(a == b);
}

// Set does not contain self-pointers, so can be realloc'd.
template<typename VALUE_T, typename Hasher>
struct is_trivially_relocatable<Set<VALUE_T,Hasher>> : public std::true_type {};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
