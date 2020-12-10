#pragma once

// Threading-related utility functions or classes.

#include "Types.h"
#include <thread>
#include <chrono>

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// This is supposed to waste time, so it doesn't need to be
// forced to be inline.
static void backOff(size_t& attempt) {
	if (attempt >= 16) {
		if (attempt < 32) {
			// Give up this timeslice.
			std::this_thread::yield();
		}
		else {
			// Sleep, in case this is waiting for lower priority threads.
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	++attempt;
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
