#pragma once

// This file contains various basic functions for processing text.

#include "../Array.h"
#include "../Types.h"
#include "../Bits.h"

#include <type_traits>

OUTER_NAMESPACE_BEGIN
namespace text {

using namespace COMMON_LIBRARY_NAMESPACE;

// NOTE: This function DOES check for a zero-terminator.
template<typename CHAR_ITER_T, typename END_T>
[[nodiscard]] constexpr inline CHAR_ITER_T skipWhitespace(CHAR_ITER_T p, const END_T end) {
	while (p != end && *p != 0 && *p <= ' ') {
		++p;
	}
	// The std::move is in case CHAR_ITER_T is a class that's awkward
	// to copy, e.g. a stream of some sort.
	return std::move(p);
}

// NOTE: This function does NOT check for a zero-terminator.
template<typename CHAR_ITER_T, typename END_T, typename CHAR_T>
[[nodiscard]] constexpr inline CHAR_ITER_T findFirstCharacter(CHAR_ITER_T text, const END_T end, const CHAR_T characterToFind) {
	while (text != end && *text != characterToFind) {
		++text;
	}
	return std::move(text);
}

// Returns the size of a zero-terminated string (excluding the zero).
template<typename CHAR_ITER_T>
[[nodiscard]] constexpr INLINE size_t stringSize(CHAR_ITER_T text) {
	size_t size = 0;
	while (*text != 0) {
		++text;
		++size;
	}
	return size;
}

// NOTE: This function does NOT check for a zero-terminator, but you can include zero in charactersToMatchAnyOf to find it.
template<typename CHAR_ITER_T, typename END_T>
[[nodiscard]] constexpr inline CHAR_ITER_T findFirstAny(CHAR_ITER_T text, const END_T end, const CHAR_ITER_T charactersToMatchAnyOf, const END_T charactersEnd) {
	while (text != end) {
		const CHAR_ITER_T match = findFirstCharacter(charactersToMatchAnyOf, charactersEnd, *text);
		if (match != charactersEnd) {
			break;
		}
		++text;
	}
	return std::move(text);
}

// This acts like a split operation, but without needing to allocate separate strings.
// NOTE: This function DOES check for a zero-terminator.
template<typename CHAR_ITER_T, typename END_T>
constexpr inline void splitLines(CHAR_ITER_T text, const END_T end, Array<Span<size_t>>& lines, bool omitEmptyLines = false) {
	size_t lineBegin = 0;
	size_t lineSize = 0;
	while (text != end) {
		auto c = *text;
		if (c == 0) {
			break;
		}
		if (c == '\n' || c =='\r') {
			size_t lineEnd = lineBegin + lineSize;
			if (lineSize != 0 || !omitEmptyLines) {
				lines.append(Span<size_t>(lineBegin, lineEnd));
				lineSize = 0;
			}
			lineBegin = lineEnd + 1;
			// Skip the carriage return or line feed character.
			++text;
			if (c != '\r') {
				continue;
			}

			// Can skip one line feed character after a carriage return.
			if (text == end) {
				break;
			}
			c = *text;
			if (c == '\n') {
				++text;
				++lineBegin;
			}
			continue;
		}
		++text;
		++lineSize;
	}
	// Final line, even if empty, (unless omitEmptyLines is true).
	if (lineSize != 0 || !omitEmptyLines) {
		size_t lineEnd = lineBegin + lineSize;
		lines.append(Span<size_t>(lineBegin, lineEnd));
	}
}

// This hash function is intended to be fast and somewhat decent for use in
// hash tables, but not for other purposes.
//
// For strings of 8 or fewer bytes, the hash is the corresponding little-endian
// integer value of the bytes, so strings of 8 or fewer bytes are guaranteed
// to all have unique hash codes, but similar strings may be very close in value,
// which can be bad for some hash table implementations, so it may be worth
// scrambling the hash code further for such hash tables.
//
// NOTE: SharedString relies on the hash code for text of size 8 or less being
// equal to the text, since it uses that for storing small strings.
[[nodiscard]] constexpr uint64 stringHash(const char* text, size_t size) {
	// This is (phi-1)*2^64, where phi is the golden ratio.
	constexpr uint64 magicNumber = 11400714819323198486;
	// 43 is 5 bytes and 3 bits.  Numbers and digits differ most
	// in the low 3 bits; 4 would be better on that front, but is an even number,
	// so would result in more overlap for longer strings.
	constexpr int magicBits = 43;

	if (size == 0) {
		return 0;
	}
	uint64 h = 0;
	if (size >= 8) {
		// If the reinterpret_cast's here are ever an issue in constexpr
		// situations, replace them each with 8 reads, some shifts, and some ORs,
		// and hope that the compiler doesn't do the slow thing in non-constexpr
		// settings.
		h = *reinterpret_cast<const uint64*>(text);
		text += 8;
		size -= 8;
		while (size >= 8) {
			h = (rotateLeft(h, magicBits) + magicNumber);
			h ^= *reinterpret_cast<const uint64*>(text);
			text += 8;
			size -= 8;
		}
		if (size == 0) {
			return h;
		}
		h = (rotateLeft(h, magicBits) + magicNumber);
	}
	// Fewer than 8 bytes remaining, apply the rest as if
	// it were zero-padded out to 8 bytes.
	if (size >= 4) {
		// Unfortunately, reinterpret_cast isn't allowed in constexpr code.
		// Hopefully the compiler can figure out the fast way to do this,
		// instead of doing 4 separate reads, some shifts, and some ORs.
		//h ^= uint64(*reinterpret_cast<const uint32*>(text));
		h ^= uint64(
			uint32(text[0]) |
			(uint32(text[1])<<8) |
			(uint32(text[2])<<16) |
			(uint32(text[3])<<24)
		);
		h ^= (uint64((size >= 5) ? text[4] : 0) << 32);
		h ^= (uint64((size >= 6) ? text[5] : 0) << 40);
		h ^= (uint64((size == 7) ? text[6] : 0) << 48);
	}
	else {
		h ^= uint64(text[0]);
		h ^= (uint64((size >= 2) ? text[1] : 0) << 8);
		h ^= (uint64((size == 3) ? text[2] : 0) << 16);
	}
	return h;
}

[[nodiscard]] constexpr bool areEqualSizeStringsEqual(const char* a, const char* b, size_t size) {
	while (size >= 8) {
		// If the reinterpret_cast's here are ever an issue in constexpr
		// situations, replace them each with 8 reads, some shifts, and some ORs,
		// and hope that the compiler doesn't do the slow thing in non-constexpr
		// settings.
		const uint64* a8 = reinterpret_cast<const uint64*>(a);
		const uint64* b8 = reinterpret_cast<const uint64*>(b);
		// Compare 8 bytes at a time, since size >= 8.
		if (*a8 != *b8) {
			return false;
		}
		a += 8;
		b += 8;
		size -= 8;
	}
	while (size != 0) {
		if (*a != *b) {
			return false;
		}
		++a;
		++b;
		--size;
	}
	return true;
}

} // namespace text
OUTER_NAMESPACE_END
