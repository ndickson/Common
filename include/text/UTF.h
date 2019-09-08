#pragma once

// This file contains functions for converting between
// UTF-8, UTF-16, and UTF-32 text strings.

#include "../Types.h"

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

// Given an array of UTF-8 bytes with the specified length (in bytes),
// this fills in an array with equivalent UTF-32 text,
// and returns the number of unicode code points.
// Call UTF32Length to get the length in advance for preallocation.
constexpr inline size_t UTF8ToUTF32(const char* utf8, size_t utf8Length, uint32* utf32) {
	size_t length = 0;
	while (utf8Length != 0) {
		char c = *utf8;
		if ((c & 0x80) == 0) {
			// Single byte character
			*utf32 = c;
			++utf32;
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
			// Number of set (1) high bits indicates the total
			// number of bytes in this codepoint (if valid).
			size_t byteCount = 2;
			char mask = 0x20;
			while (first & mask) {
				++byteCount;
				mask >>= 1;
			}
			uint32 value = first & (0xFF<<byteCount);
			++utf8;
			--utf8Length;
			--byteCount;
			bool isValid = true;
			do {
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
				value = (value << 6) | (c & 0x3F);
				++utf8;
				--utf8Length;
				--byteCount;
			} while (byteCount != 0);

			if (!isValid) {
				continue;
			}

			*utf32 = value;
			++utf32;
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

// Given an array of UTF-8 bytes with the specified length (in bytes),
// this returns the number of UTF-16 code units (16-bit integers) needed.
// NOTE: This does not count a byte order marker.  If you want a byte order marker, add 1.
constexpr inline size_t UTF16Length(const char* utf8, size_t utf8Length) {
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
			// Number of set (1) high bits indicates the total
			// number of bytes in this codepoint (if valid).
			size_t byteCount = 2;
			char mask = 0x20;
			while (first & mask) {
				++byteCount;
				mask >>= 1;
			}
			// The length depends on the value, so we need it,
			// even if the number of bits could be distinguished based on
			// the byte count alone, since the top bits are allowed to be zero
			// in the UTF-8 encoding, which would effectively make for a
			// different number of bits used.
			uint32 value = first & (0xFF<<byteCount);
			++utf8;
			--utf8Length;
			--byteCount;
			bool isValid = true;
			do {
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
				value = (value << 6) | (c & 0x3F);
				++utf8;
				--utf8Length;
				--byteCount;
			} while (byteCount != 0);

			if (!isValid) {
				continue;
			}

			if (value < 0x10000) {
				++length;
			}
			else {
				length += 2;
			}
		}
		else {
			// Continuation byte: skip it
			--utf8Length;
			++utf8;
		}
	}
	return length;
}

// Given an array of UTF-8 bytes with the specified length (in bytes),
// this fills in an array with equivalent UTF-16 text,
// and returns the number of UTF-16 code units (16-bit integers).
// Call UTF16Length to get the length in advance for preallocation.
// NOTE: This does not add a byte order marker.  If you want a byte order marker, write 0xFEFF before this output.
constexpr inline size_t UTF8ToUTF16(const char* utf8, size_t utf8Length, uint16* utf16) {
	size_t length = 0;
	while (utf8Length != 0) {
		char c = *utf8;
		if ((c & 0x80) == 0) {
			// Single byte character
			*utf16 = c;
			++utf16;
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
			// Number of set (1) high bits indicates the total
			// number of bytes in this codepoint (if valid).
			size_t byteCount = 2;
			char mask = 0x20;
			while (first & mask) {
				++byteCount;
				mask >>= 1;
			}
			uint32 value = first & (0xFF<<byteCount);
			++utf8;
			--utf8Length;
			--byteCount;
			bool isValid = true;
			do {
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
				value = (value << 6) | (c & 0x3F);
				++utf8;
				--utf8Length;
				--byteCount;
			} while (byteCount != 0);

			if (!isValid) {
				continue;
			}

			if (value < 0x10000) {
				*utf16 = uint16(value);
				++utf16;
				++length;
			}
			else {
				value -= 0x10000;
				utf16[0] = uint16(0xD800 | (value & 0x3FF));
				utf16[1] = uint16(0xDC00 | ((value>>10) & 0x3FF));
				utf16 += 2;
				length += 2;
			}
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
