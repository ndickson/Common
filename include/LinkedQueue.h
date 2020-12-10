#pragma once

#include "Types.h"
#include "ThreadUtil.h"
#include <atomic>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// A threadsafe queue for items that can each be in at most one queue
// at a time and that can each have a dedicated "next" pointer.
// Type T must implement the function:
//     T*& nextQueuePointer();
// 
// NOTE: This class does not own any of the items it contains.
template<typename T>
class LinkedQueue {
	std::atomic<T*> head;
	T* volatile tail;

	constexpr static uintptr_t sentinel = ~uintptr_t(0);
public:
	INLINE LinkedQueue() : head(nullptr), tail(nullptr) {}
	INLINE ~LinkedQueue() {
		// The queue doesn't own any of the items, so it should be empty already.
		assert(head.load(std::memory_order_relaxed) == nullptr);
		assert(tail == nullptr);
	}

	T* pop() volatile {
		// Fast case for empty queue, to reduce unnecessary contention.
		T* prevHead = head.load(std::memory_order_acquire);
		if (prevHead == nullptr) {
			// No item to pop.
			return nullptr;
		}
		size_t attempt = 0;
		while (true) {
			T* prevHead = head.exchange(reinterpret_cast<T*>(sentinel));
			if (prevHead == nullptr) {
				// No item to dequeue.
				head.exchange(nullptr);
				return nullptr;
			}
			if (uintptr_t(prevHead) == sentinel) {
				// Another thread is currently modifying the queue.
				backOff(attempt);
				continue;
			}
			T* nextHead = prevHead->nextQueuePointer();
			if (nextHead == nullptr) {
				// No more items left, so set tail to null, too.
				// NOTE: This must be written before releasing head.
				tail = nullptr;
			}
			head.exchange(nextHead);
			prevHead->nextQueuePointer() = nullptr;
			return prevHead;
		}
	}

	void push(T* item) volatile {
		assert(item->nextQueuePointer() == nullptr);
		size_t attempt = 0;
		while (true) {
			// Acquire the head pointer, just to prevent
			// anything from removing the last item while
			// we're trying to modify its next pointer.
			T* prevHead = head.exchange(reinterpret_cast<T*>(sentinel));
			if (prevHead == nullptr) {
				// No previous items, so this is the only item.
				tail = item;
				head.exchange(item);
				return;
			}
			if (uintptr_t(prevHead) == sentinel) {
				// Another thread is currently modifying the queue.
				backOff(attempt);
				continue;
			}
			T* prevTail = tail;
			prevTail->nextQueuePointer() = item;
			tail = item;
			head.exchange(prevHead);
			return;
		}
	}
};

// An alternative to LinkedQueue that might be slightly faster
// if the order of removal doesn't matter.
// NOTE: This class does not own any of the items it contains.
template<typename T>
class LinkedStack {
	std::atomic<T*> head;

	constexpr static uintptr_t sentinel = ~uintptr_t(0);
public:
	INLINE LinkedStack() : head(nullptr) {}
	INLINE ~LinkedStack() {
		// The stack doesn't own any of the items, so it should be empty already.
		assert(head.load(std::memory_order_relaxed) == nullptr);
	}

	T* pop() volatile {
		// Fast case for empty stack, to reduce unnecessary contention.
		T* prevHead = head.load(std::memory_order_acquire);
		if (prevHead == nullptr) {
			// No item to pop.
			return nullptr;
		}
		size_t attempt = 0;
		while (true) {
			T* prevHead = head.exchange(reinterpret_cast<T*>(sentinel));
			if (prevHead == nullptr) {
				// No item to pop.
				head.exchange(nullptr);
				return nullptr;
			}
			if (uintptr_t(prevHead) == sentinel) {
				// Another thread is currently popping the stack.
				backOff(attempt);
				continue;
			}
			T* nextHead = prevHead->nextQueuePointer();
			head.exchange(nextHead);
			prevHead->nextQueuePointer() = nullptr;
			return prevHead;
		}
	}

	void push(T* item) volatile {
		assert(item->nextQueuePointer() == nullptr);
		T* prevHead = head.load(std::memory_order_acquire);
		size_t attempt = 0;
		while (true) {
			if (uintptr_t(prevHead) == sentinel) {
				// Another thread is currently popping the stack.
				backOff(attempt);
				continue;
			}
			item->nextQueuePointer() = prevHead;
			bool success = head.compare_exchange_strong(prevHead, item);
			if (success) {
				return;
			}
			// Another thread modified the stack after we read it.
			backOff(attempt);
		}
	}
};

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
