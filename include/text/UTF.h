#pragma once

// This file contains functions for converting between
// UTF-8, UTF-16, and UTF-32 text strings.

#include "../Types.h"

OUTER_NAMESPACE_BEGIN
namespace text {

using namespace COMMON_LIBRARY_NAMESPACE;

// Returns true iff a UTF-8 text string is ASCII compatible,
// i.e. iff there are no multi-byte encoded characters.
constexpr inline bool UTF8IsAllASCII(const char* utf8, size_t utf8Length) {
	for (size_t i = 0; i < utf8Length; ++i) {
		if ((utf8[i] & 0x80) != 0) {
			return false;
		}
	}
	return true;
}

// Returns true iff the provided byte is the first byte of a
// UTF-8 code point, i.e. iff it is not a continuation byte.
constexpr INLINE bool UTF8IsFirstByte(const char utf8Byte) {
	return (utf8Byte & char(0xC0)) != char(0x80);
}

// Given an array of UTF-8 bytes with the specified length (in bytes),
// this examines at most one equivalent UTF-32 code point,
// returning the number of *bytes* used in the code point (or skipped invalid data).
// If there was a valid code point, isValid is set to true,
// else isValid is set to false.
// This function is equivalent to UTF8ToUTF32Single, except not computing the
// code point value, only whether it's valid or not.
constexpr inline size_t UTF8ToUTF32SingleBytes(const char* utf8, size_t utf8Length, bool& isValid) {
	if (utf8Length == 0) {
		// No input text
		isValid = false;
		return 0;
	}
	char c = *utf8;
	const char highBits = (c & 0xC0);
	if (highBits != 0xC0) {
		// Single byte character (0x00), which is valid, or
		// continuation byte (0x80), which is invalid.
		isValid = (highBits == 0x00);
		return 1;
	}

	// First byte of a multi-byte character:
	// highBits == 0xC0

	const char first = c;
	// Number of set (1) high bits indicates the total
	// number of bytes in this codepoint (if valid).
	size_t byteCount = 2;
	char mask = 0x20;
	while (first & mask) {
		++byteCount;
		mask >>= 1;
	}
	if (utf8Length < byteCount) {
		// Reached end of string before end of character.
		isValid = false;
		return utf8Length;
	}

	++utf8;
	size_t remainingByteCount = byteCount - 1;
	do {
		c = *utf8;
		if ((c & 0xC0) != 0x80) {
			// Not a continuation byte, so this is
			// an invalid character encoding.
			isValid = false;
			return (byteCount - remainingByteCount + 1);
		}
		++utf8;
		--remainingByteCount;
	} while (remainingByteCount != 0);

	isValid = true;
	return byteCount;
}

// Given an array of UTF-8 bytes with the specified length (in bytes),
// this returns the number of unicode code points, i.e. the
// length of an array of equivalent UTF-32 text.
// NOTE: This does not count a null terminator.
// If you want a null terminator, add 1.
constexpr inline size_t UTF32Length(const char* utf8, size_t utf8Length) {
	size_t length = 0;
	while (utf8Length != 0) {
		bool isValid = false;
		size_t numBytes = UTF8ToUTF32SingleBytes(utf8, utf8Length, isValid);
		utf8 += numBytes;
		utf8Length -= numBytes;
		length += size_t(isValid);
	}
	return length;
}

// This is used by UTF8ToUTF32Single as an invalid code point marker.
constexpr uint32 UTF32_INVALID_CODE_POINT = 0xFFFFFFFFU;

// Given an array of UTF-8 bytes with the specified length (in bytes),
// this fills in at most one equivalent UTF-32 code point,
// returning the number of *bytes* used in the input.
// If there was a valid code point, it is written to utf32,
// else UTF32_INVALID_CODE_POINT is written to utf32.
constexpr inline size_t UTF8ToUTF32Single(const char* utf8, size_t utf8Length, uint32& utf32) {
	if (utf8Length == 0) {
		// No input text
		utf32 = UTF32_INVALID_CODE_POINT;
		return 0;
	}
	char c = *utf8;
	if ((c & 0x80) == 0) {
		// Single byte character
		utf32 = c;
		return 1;
	}
	if ((c & 0xC0) == 0x80) {
		// Continuation byte: skip it
		utf32 = UTF32_INVALID_CODE_POINT;
		return 1;
	}

	// First byte of a multi-byte character:
	// (c & 0xC0) == 0xC0

	const char first = c;
	// Number of set (1) high bits indicates the total
	// number of bytes in this codepoint (if valid).
	size_t byteCount = 2;
	char mask = 0x20;
	while (first & mask) {
		++byteCount;
		mask >>= 1;
	}
	if (utf8Length < byteCount) {
		// Reached end of string before end of character.
		utf32 = UTF32_INVALID_CODE_POINT;
		return utf8Length;
	}

	uint32 value = first & (0xFF<<byteCount);
	++utf8;
	size_t remainingByteCount = byteCount - 1;
	do {
		c = *utf8;
		if ((c & 0xC0) != 0x80) {
			// Not a continuation byte, so this is
			// an invalid character encoding.
			utf32 = UTF32_INVALID_CODE_POINT;
			return (byteCount - remainingByteCount + 1);
		}
		value = (value << 6) | (c & 0x3F);
		++utf8;
		--remainingByteCount;
	} while (remainingByteCount != 0);

	utf32 = value;
	return byteCount;
}

// Given an array of UTF-8 bytes with the specified length (in bytes),
// this fills in an array with equivalent UTF-32 text,
// and returns the number of unicode code points.
// Call UTF32Length to get the length in advance for preallocation.
// NOTE: This does not add a null terminator.
// If you want a null terminator, write 0 after this output.
constexpr inline size_t UTF8ToUTF32(const char* utf8, size_t utf8Length, uint32* utf32) {
	size_t length = 0;
	while (utf8Length != 0) {
		uint32 value = UTF32_INVALID_CODE_POINT;
		size_t numBytes = UTF8ToUTF32Single(utf8, utf8Length, value);
		utf8 += numBytes;
		utf8Length -= numBytes;
		if (value != UTF32_INVALID_CODE_POINT) {
			*utf32 = value;
			++utf32;
			++length;
		}
	}
	return length;
}

// 6 UTF-8 bytes can only represent 31 bits, so for 32 bits,
// 7 UTF-8 bytes are needed, (6 continuation bytes, each with 6 bits)
constexpr size_t UTF8_MAX_UTF32_BYTES = 7;
// The current maximum valid UTF-32 code point, 0x10FFFF needs 21 bits,
// which can be represented in 4 UTF-8 bytes, (3 bits in byte 0, and
// 6 bits in each of the 3 continuation bytes).
constexpr size_t UTF8_MAX_VALID_UTF32_BYTES = 4;

// Given a UTF-32 code point, this returns the number of bytes
// that the equivalent UTF-8 representation would require.
// The return value should be at least 1 and at most UTF8_MAX_UTF32_BYTES (7),
// or at most UTF8_MAX_VALID_UTF32_BYTES (4) for the highest maximum valid
// UTF-32 code point (0x10FFFF).
// This return value is equivalent to that of UTF32ToUTF8SingleBytes.
constexpr INLINE size_t UTF32ToUTF8SingleBytes(uint32 utf32) {
	if (utf32 < 0x80) {
		return 1;
	}

	size_t numBytes = 2;
	utf32 >>= 11;
	while (utf32 != 0) {
		++numBytes;
		utf32 >>= 5;
	}

	return numBytes;
}

// Given a UTF-32 code point, this convert it to the equivalent UTF-8
// reprensentation and returns the number of bytes used in the UTF-8.
// The number of bytes should be at least 1 and at most UTF8_MAX_UTF32_BYTES (7),
// or at most UTF8_MAX_VALID_UTF32_BYTES (4) for the highest maximum valid
// UTF-32 code point (0x10FFFF).
// This number of bytes is equivalent to the return value of UTF32ToUTF8SingleBytes.
constexpr inline size_t UTF32ToUTF8Single(uint32 utf32, char* utf8) {
	if (utf32 < 0x80) {
		// 7 bits fits in 1 UTF-8 byte
		*utf8 = char(utf32);
		return 1;
	}

	// Multi-byte character:
	// First, compute continuation bytes in reverse order.
	char* temputf8 = utf8;
	uint32 mask = 0x40;
	size_t numBytes = 1;
	do {
		*temputf8 = char(utf32 & 0x3F) | 0x80;
		utf32 >>= 6;
		mask >>= 1;
		++temputf8;
		++numBytes;
	} while(utf32 >= mask);

	// Next, compute first byte.
	// Top bits are bitwise NOT of one less than twice mask,
	// which is equivalent to negation of twice mask.
	const char byte0 = char(utf32 | -int32(2*mask));//~((2*mask)-1));

	// Finally, reverse the bytes.
	*temputf8 = *utf8;
	*utf8 = byte0;
	++utf8;
	--temputf8;

	while (utf8 < temputf8) {
		const char temp = *utf8;
		*utf8 = *temputf8;
		*temputf8 = temp;
		++utf8;
		--temputf8;
	}

	return numBytes;
}

// Given an array of UTF-8 bytes with the specified length (in bytes),
// this returns the number of UTF-16 code units (16-bit integers) needed.
// NOTE: This does not count a byte order marker or null terminator.
// If you want a byte order marker or null terminator, add 1 for each.
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
// NOTE: This does not add a byte order marker or null terminator.
// If you want a byte order marker, write 0xFEFF before this output.
// If you want a null terminator, write 0 after this output.
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

} // namespace text
OUTER_NAMESPACE_END
