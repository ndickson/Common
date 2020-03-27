#pragma once

#include "Set.h"
#include "Types.h"

#include <utility>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// This is an array-based hash map, mostly compatible with std::unordered_map,
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
// Also unlike std::unordered_map, Hasher is required to contain the static functions:
// uint64 hash(const KEY_T&);
// uint64 hash(const std::pair<KEY_T,VALUE_T>&);
// bool equals(const KEY_T&, const KEY_T&);
// bool equals(const std::pair<KEY_T,VALUE_T>&, const KEY_T&);
// bool equals(const std::pair<KEY_T,VALUE_T>&, const std::pair<KEY_T,VALUE_T>&);
// The hash and equals functions using a std::pair must treat it as equivalent to just
// its first component, the key.
template<typename KEY_T, typename VALUE_T, typename Hasher = DefaultMapHasher<KEY_T,VALUE_T>>
class Map : private Set<std::pair<KEY_T,VALUE_T>,Hasher> {
	using MapPair = std::pair<KEY_T,VALUE_T>;
	using Base = Set<MapPair,Hasher>;
	using TablePair = typename Base::TablePair;

	using Base::data;
	using Base::size_;
	using Base::capacity;

	friend Base;

	using Base::increaseCapacity;

public:

	using const_iterator = typename Base::const_iterator;
	using iterator = typename Base::template iterator_base<std::pair<const KEY_T,VALUE_T>,TablePair>;

	using key_type = KEY_T;
	using mapped_type = VALUE_T;
	using value_type = std::pair<const KEY_T,VALUE_T>;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = value_type*;
	using const_pointer = const value_type*;

	using hasher = Hasher;

private:

	// This default-constructs a VALUE_T if insertion occurs.
	template<typename KEY_REF_T,typename ITERATOR_T>
	std::pair<ITERATOR_T,bool> insertKey(KEY_REF_T key) noexcept {
		uint64 hashCode = Hasher::hash(key);

		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, hashCode, key, index, targetIndex);
		if (found) {
			// It's already in the set, so value was not inserted.
			return std::make_pair(
				ITERATOR_T(data.get() + index, data.get() + capacity),
				false
			);
		}

		++size_;

		// If there's no collision, no need to increase the capacity, unless the capacity is zero.
		if ((size_ > capacity) || ((index != targetIndex) && (size_ > (capacity>>1)))) {
			increaseCapacity();
			hash::findInTable<false, void>(data.get(), capacity, hashCode, key, index, targetIndex);
		}

		hash::insertIntoTable(data.get(), capacity, std::make_pair(std::move(key), VALUE_T()), index, targetIndex);

		return std::make_pair(
			ITERATOR_T(data.get() + index, data.get() + capacity),
			true
		);
	}

	template<typename KEY_REF_T,typename VALUE_REF_T,bool ALWAYS_ASSIGN>
	std::pair<iterator,bool> insertKeyValue(KEY_REF_T key, VALUE_REF_T value) noexcept {
		uint64 hashCode = Hasher::hash(key);

		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, hashCode, key, index, targetIndex);
		if (found) {
			// It's already in the set, so value was not inserted.
			if (ALWAYS_ASSIGN) {
				data[index].first.second = std::move(value);
			}
			return std::make_pair(
				ITERATOR_T(data.get() + index, data.get() + capacity),
				false
			);
		}

		++size_;

		// If there's no collision, no need to increase the capacity, unless the capacity is zero.
		if ((size_ > capacity) || ((index != targetIndex) && (size_ > (capacity>>1)))) {
			increaseCapacity();
			hash::findInTable<false, void>(data.get(), capacity, hashCode, key, index, targetIndex);
		}

		hash::insertIntoTable(data.get(), capacity, std::make_pair(std::move(key), std::move(value)), index, targetIndex);

		return std::make_pair(
			ITERATOR_T(data.get() + index, data.get() + capacity),
			true
		);
	}

public:

	INLINE Map() noexcept : Base() {}
	INLINE ~Map() noexcept = default;

	INLINE Map(Map&& that) noexcept : Base(std::move(that)) {}

	INLINE explicit Map(const Map& that) noexcept : Base(that) {}

	INLINE explicit Map(size_t bucketCount) noexcept : Base(bucketCount) {}

	template<typename INPUT_ITER>
	INLINE Map(INPUT_ITER it, const INPUT_ITER endIt, size_t bucketCount = 0) noexcept : Base(it, endIt, bucketCount) {}

	INLINE explicit Map(std::initializer_list<MapPair> list, size_t bucketCount = 0) noexcept : Base(std::move(list), bucketCount) {}

	INLINE Map& operator=(Map&& that) noexcept {
		static_cast<Base*>(this)->operator=(std::move(that));
		return *this;
	}

	INLINE Map& operator=(const Map& that) noexcept {
		static_cast<Base*>(this)->operator=(that);
		return *this;
	}

	INLINE Map& operator=(std::initializer_list<MapPair> list) noexcept {
		static_cast<Base*>(this)->operator=(std::move(list));
		return *this;
	}

	INLINE void swap(Map& that) noexcept {
		static_cast<Base*>(this)->swap(that);
	}

	using Base::begin;

	[[nodiscard]] INLINE iterator begin() noexcept {
		TablePair* p = data.get();
		const TablePair* pend = p + capacity;
		if (size_ == 0) {
			return iterator(pend, pend);
		}
		while (p->second == hash::EMPTY_INDEX) {
			++p;
		}
		return iterator(p, pend);
	}

	using Base::end;
	using Base::cbegin;
	using Base::cend;

	using Base::size;
	using Base::empty;
	using Base::max_size;
	using Base::clear;

	[[nodiscard]] const_iterator find(const KEY_T& key) const noexcept {
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(key), key, index, targetIndex);
		return const_iterator(
			data.get() + (found ? index : capacity),
			data.get() + capacity
		);
	}
	[[nodiscard]] iterator find(const KEY_T& key) noexcept {
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(key), key, index, targetIndex);
		return iterator(
			data.get() + (found ? index : capacity),
			data.get() + capacity
		);
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const std::pair<KEY_T,VALUE_T>&,const OTHER_T&)
	template<typename OTHER_T>
	[[nodiscard]] const_iterator find(const OTHER_T& key) const noexcept {
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(key), key, index, targetIndex);
		return const_iterator(
			data.get() + (found ? index : capacity),
			data.get() + capacity
		);
	}
	template<typename OTHER_T>
	[[nodiscard]] iterator find(const OTHER_T& key) noexcept {
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(key), key, index, targetIndex);
		return iterator(
			data.get() + (found ? index : capacity),
			data.get() + capacity
		);
	}

	[[nodiscard]] INLINE bool contains(const KEY_T& key) const noexcept {
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(key), key, index, targetIndex);
		return found;
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const std::pair<KEY_T,VALUE_T>&,const OTHER_T&)
	template<typename OTHER_T>
	[[nodiscard]] INLINE bool contains(const OTHER_T& key) const noexcept {
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(key), key, index, targetIndex);
		return found;
	}

	[[nodiscard]] INLINE size_t count(const KEY_T& key) const noexcept {
		return contains(key) ? 1 : 0;
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const std::pair<KEY_T,VALUE_T>&,const OTHER_T&)
	template<typename OTHER_T>
	[[nodiscard]] INLINE size_t count(const OTHER_T& key) const noexcept {
		return contains(key) ? 1 : 0;
	}

	// There can be at most 1 key equal to key in the map, so this is either
	// an empty range or a range containing 1 item.
	[[nodiscard]] std::pair<const_iterator,const_iterator> equal_range(const KEY_T& key) const noexcept {
		const_iterator it = find(key);
		if (it.isEnd()) {
			return std::make_pair(it, it);
		}
		const_iterator next = it;
		++next;
		return std::make_pair(it, next);
	}
	[[nodiscard]] std::pair<iterator,iterator> equal_range(const KEY_T& key) noexcept {
		iterator it = find(key);
		if (it.isEnd()) {
			return std::make_pair(it, it);
		}
		iterator next = it;
		++next;
		return std::make_pair(it, next);
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const std::pair<KEY_T,VALUE_T>&,const OTHER_T&)
	template<typename OTHER_T>
	[[nodiscard]] std::pair<const_iterator,const_iterator> equal_range(const OTHER_T& key) const noexcept {
		const_iterator it = find(key);
		if (it.isEnd()) {
			return std::make_pair(it, it);
		}
		const_iterator next = it;
		++next;
		return std::make_pair(it, next);
	}
	template<typename OTHER_T>
	[[nodiscard]] std::pair<iterator,iterator> equal_range(const OTHER_T& key) noexcept {
		iterator it = find(key);
		if (it.isEnd()) {
			return std::make_pair(it, it);
		}
		iterator next = it;
		++next;
		return std::make_pair(it, next);
	}

	// Unlike std::unordered_map, this will not throw an exception.
	// Do not call it with a key that isn't present.
	[[nodiscard]] const VALUE_T& at(const KEY_T& key) const noexcept {
		size_t index;
		size_t targetIndex;
#ifndef NDEBUG
		bool found =
#endif
		hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(key), key, index, targetIndex);
		assert(found);
		return data[index];
	}
	[[nodiscard]] VALUE_T& at(const KEY_T& key) noexcept {
		size_t index;
		size_t targetIndex;
#ifndef NDEBUG
		bool found =
#endif
		hash::findInTable<true, Hasher>(data.get(), capacity, Hasher::hash(key), key, index, targetIndex);
		assert(found);
		return data[index];
	}
	[[nodiscard]] INLINE VALUE_T& operator[](const KEY_T& key) noexcept {
		auto pair = insertKey<const KEY_T&, iterator>(key);
		return pair.first->second;
	}
	[[nodiscard]] INLINE VALUE_T& operator[](KEY_T&& key) noexcept {
		auto pair = insertKey<KEY_T&&, iterator>(std::move(key));
		return pair.first->second;
	}

	INLINE std::pair<iterator,bool> insert(const KEY_T& key, const VALUE_T& value) noexcept {
		return insertKeyValue<const KEY_T&, const VALUE_T&, false>(key, value);
	}
	INLINE std::pair<iterator,bool> insert(KEY_T&& key, VALUE_T&& value) noexcept {
		return insertKeyValue<KEY_T&&, VALUE_T&&, false>(std::move(key), std::move(value));
	}
	INLINE std::pair<iterator,bool> insert(const MapPair& pair) noexcept {
		return Base::template insertCommon<const MapPair&, iterator>(pair);
	}
	INLINE std::pair<iterator,bool> insert(MapPair&& pair) noexcept {
		return Base::template insertCommon<MapPair&&, iterator>(std::move(pair));
	}
	template<typename INPUT_ITER>
	void insert(INPUT_ITER it, const INPUT_ITER endIt) noexcept {
		for (; it != endIt; ++it) {
			insert(*it);
		}
	}
	void insert(std::initializer_list<MapPair> list) noexcept {
		for (auto it = list.begin(), end = list.end(); it != end; ++it) {
			insert(*it);
		}
	}
	INLINE std::pair<iterator,bool> insert_or_assign(const KEY_T& key, VALUE_T&& value) noexcept {
		return insertKeyValue<const KEY_T&, VALUE_T&&, true>(key, std::move(value));
	}
	INLINE std::pair<iterator,bool> insert_or_assign(KEY_T&& key, VALUE_T&& value) noexcept {
		return insertKeyValue<KEY_T&&, VALUE_T&&, true>(std::move(key), std::move(value));
	}

	INLINE iterator erase(const const_iterator& it) noexcept {
		return Base::template eraseCommon<iterator,const_iterator>(it);
	}
	INLINE iterator erase(const iterator& it) noexcept {
		return Base::template eraseCommon<iterator,iterator>(it);
	}

	size_t erase(const KEY_T& key) noexcept {
		TablePair* pbegin = data.get();
		size_t index;
		size_t targetIndex;
		bool found = hash::findInTable<true, Hasher>(pbegin, capacity, Hasher::hash(key), key, index, targetIndex);
		if (!found) {
			return 0;
		}

		hash::eraseFromTable(pbegin, capacity, pbegin + index, index);
		--size_;
		return 1;
	}

	using Base::bucket_count;
	using Base::max_bucket_count;
	using Base::load_factor;
	using Base::max_load_factor;
	using Base::rehash;
	using Base::reserve;
};

template<typename KEY_T, typename VALUE_T, typename Hasher>
bool operator==(const Map<KEY_T,VALUE_T,Hasher>& a, const Map<KEY_T,VALUE_T,Hasher>& b) {
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
		if (!b.contains(it->first)) {
			return false;
		}
	}
	return true;
}

template<typename KEY_T, typename VALUE_T, typename Hasher>
INLINE bool operator!=(const Map<KEY_T,VALUE_T,Hasher>& a, const Map<KEY_T,VALUE_T,Hasher>& b) {
	return !(a == b);
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
