#pragma once

#include "Types.h"

OUTER_NAMESPACE_BEGIN
COMMON_LIBRARY_NAMESPACE_BEGIN

// Given an array of UTF-8 bytes with the specified length (in bytes),
// this returns the number of unicode code points, i.e. the
// length of an array of equivalent UTF-32 text.
constexpr inline size_t UTF32Length(const char* utf8, size_t utf8Length) {
	size_t length = 0;
	while (utf8Length != 0) {
		char c = *utf8;
		if ((c & 0x80) == 0) {
			// Single byte character
			++length;
			--utf8Length;
			++utf8;
		}
		else if ((c & 0xC0) == 0xC0) {
			// First byte of a multi-byte character
			if (utf8Length < 2) {
				// Reached end of string before end of character.
				return length;
			}
			const char first = c;
			c = utf8[1];
			if ((c & 0xC0) != 0x80) {
				// Not a continuation byte, so skip
				// this character and start over
				continue;
			}
			utf8 += 2;
			utf8Length -= 2;
			char mask = 0x20;
			bool isValid = true;
			while (first & mask) {
				if (utf8Length == 0) {
					// Reached end of string before end of character.
					return length;
				}
				c = *utf8;
				if ((c & 0xC0) != 0x80) {
					// Not a continuation byte, so skip
					// this character and start over
					isValid = false;
					break;
				}

				++utf8;
				--utf8Length;

				// NOTE: A check for mask becoming zero is not needed,
				// since (first & 0) will be 0.
				mask >>= 1;
			}
			if (!isValid) {
				continue;
			}

			++length;
		}
		else {
			// Continuation byte: skip it
			--utf8Length;
			++utf8;
		}
	}
	return length;
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
