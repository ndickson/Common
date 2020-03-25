#pragma once

#include "BigSet.h"
#include "Types.h"

#include <utility>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// This is a hash map designed for very large numbers of items
// being looked up and added from many threads simultaneously.
// It has a very large unfront memory footprint, so is best suited
// to situations where there is a single large map, like a global map
// of unique string representatives mapping to unique IDs.
template<typename KEY_T, typename VALUE_T, typename Hasher = DefaultMapHasher<KEY_T,VALUE_T>>
class BigMap : private BigSet<std::pair<KEY_T,VALUE_T>,Hasher> {
	using MapPair = std::pair<KEY_T,VALUE_T>;
	using Base = BigSet<MapPair,Hasher>;

	friend Base;

public:

	using const_accessor = typename Base::template accessor_base<false, const MapPair, const typename Base::TablePair>;
	using accessor = typename Base::template accessor_base<true, std::pair<const KEY_T,VALUE_T>, typename Base::TablePair>;

	INLINE BigMap() : BigSet<MapPair,Hasher>() {}

	// Find the key in the map and acquire a const_accessor to its item.
	// If there is an equal key in the map, this returns true, else false.
	INLINE bool find(const_accessor& accessor, const KEY_T& key) const {
		return Base::findCommon(*this, accessor, key);
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	INLINE bool find(const_accessor& accessor, const OTHER_T& key) const {
		return Base::findCommon(*this, accessor, key);
	}

	// Find the key in the map and acquire an accessor to its item.
	// If there is an equal key in the map, this returns true, else false.
	INLINE bool find(accessor& accessor, const KEY_T& key) {
		return Base::findCommon(*this, accessor, key);
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	INLINE bool find(accessor& accessor, const OTHER_T& key) {
		return Base::findCommon(*this, accessor, key);
	}

	// Insert the value into the map, when an accessor is not needed.
	// If the insertion succeeded, this returns true.
	// If there was already an equal item in the set, this returns false.
	INLINE bool insert(KEY_T&& key, VALUE_T&& value) {
		return Base::insertCommon(static_cast<const_accessor*>(nullptr), std::make_pair(std::move(key), std::move(value)));
	}
	INLINE bool insert(std::pair<KEY_T,VALUE_T>&& value) {
		return Base::insertCommon(static_cast<const_accessor*>(nullptr), std::move(value));
	}

	// Insert the value into the map and acquire a const_accessor to it.
	// If the insertion succeeded, this returns true.
	// If there was already an equal item in the map, a const_accessor
	// to the existing item is acquired, and this returns false.
	INLINE bool insert(const_accessor& accessor, KEY_T&& key, VALUE_T&& value) {
		return Base::insertCommon(&accessor, std::make_pair(std::move(key), std::move(value)));
	}
	INLINE bool insert(const_accessor& accessor, std::pair<KEY_T,VALUE_T>&& value) {
		return Base::insertCommon(&accessor, std::move(value));
	}

	// Insert the value into the map and acquire an accessor to it.
	// If the insertion succeeded, this returns true.
	// If there was already an equal item in the map, an accessor
	// to the existing item is acquired, and this returns false.
	//
	// The signatures that take only a key can set the value afterward,
	// via the accessor.
	INLINE bool insert(accessor& accessor, KEY_T&& key) {
		return Base::insertCommon(&accessor, std::make_pair(std::move(key), VALUE_T()));
	}
	INLINE bool insert(accessor& accessor, const KEY_T& key) {
		return Base::insertCommon(&accessor, std::make_pair(key, VALUE_T()));
	}
	INLINE bool insert(accessor& accessor, KEY_T&& key, VALUE_T&& value) {
		return Base::insertCommon(&accessor, std::make_pair(std::move(key), std::move(value)));
	}
	INLINE bool insert(accessor& accessor, std::pair<KEY_T,VALUE_T>&& value) {
		return Base::insertCommon(&accessor, std::move(value));
	}

	// Remove the item referenced by the accessor from the set.
	// If there was an item referenced by the accessor, this returns true.
	// If there was no item referenced by the accessor,
	// (and so no item was removed), this returns false.
	// Afterwards, the accessor is always not referencing an item.
	INLINE bool erase(accessor& accessor) {
		return Base::eraseAccessor(accessor);
	}

	// Remove any item from the set that is equal to the given value.
	// If an item was removed, this returns true, else false.
	INLINE bool erase(const KEY_T& key) {
		return Base::eraseInternal(key);
	}

	// This signature requires both Hasher::hash(const OTHER_T&) and
	// Hasher::equals(const VALUE_T&,const OTHER_T&)
	template<typename OTHER_T>
	INLINE bool erase(const OTHER_T& key) {
		return Base::eraseInternal(key);
	}
};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
