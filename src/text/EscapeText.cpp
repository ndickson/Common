// This file contains definitions of functions for escaping and unescaping
// text strings in various styles.

#include "text/EscapeText.h"
#include "text/UTF.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Types.h"
#include <limits>

OUTER_NAMESPACE_BEGIN
namespace text {

using namespace Common;

void escapeBackslash(const char* inBegin, const char*const inEnd, Array<char>& outText, const BackslashEscapeStyle style) {
	// NOTE: This function intentionally does not clear outText, instead just
	// appending to it, to allow easier processing of text in context.

	if (inBegin == nullptr) {
		// Treat a null input string as an empty string.
		return;
	}

	// inEnd being null indicates a null-terminated string.
	const bool isNullTerminated = (inEnd == nullptr);

	while (isNullTerminated ? (*inBegin != 0) : (inBegin != inEnd)) {
		const char c = *inBegin;
		if ((c & 0x80) == 0) {
			if (c < ' ') {
				// Control character, 0x00 through 0x1F
				char next;
				bool isTwo = true;
				if (c == '\t') {      // Horizontal Tab (0x09)
					next = 't';
				}
				else if (c == '\n') { // Line Feed (0x0A)
					next = 'n';
				}
				else if (c == '\r') { // Carriage Return (0x0D)
					next = 'r';
				}
				else if (c == '\f') { // Form Feed (0x0C)
					next = 'f';
				}
				else if (c == '\b') { // Backspace (0x08)
					next = 'b';
				}
				else if (c == '\v' && style == BackslashEscapeStyle::C) {
					next = 'v';       // Vertical Tab (0x0B)
				}
				// NOTE: The \0 escape sequence is excluded, because it's
				// a special case of octal escape sequences, so it may not
				// be valid to represent it that way if it's followed
				// by a number character from '0'-'7'.
				//else if (c == 0 && style == BackslashEscapeStyle::C) {
				//	next = '0';
				//}
				else {
					isTwo = false;
					outText.append('\\');
					outText.append('u');
					outText.append('0');
					outText.append('0');
					// Hex digits for c in range from 00 to 1F
					outText.append('0' | (c>>4));
					char cLow = char(c & 0xF);
					cLow += ((cLow < 10) ? '0' : ('A'-char(10)));
					outText.append(cLow);
				}
				if (isTwo) {
					outText.append('\\');
					outText.append(next);
				}
			}
			else if (c == '\\' || c == '\"' || (style == BackslashEscapeStyle::C && c == '\'')) {
				// Quote or backslash
				outText.append('\\');
				outText.append(char(c));
			}
			else {
				// Single byte regular UTF-8 code point
				outText.append(char(c));
			}

			// Only used a single byte of the input
			++inBegin;
		}
		else {
			// Multi-byte UTF-8 code point
			size_t utf8Length = inEnd - inBegin;
			uint32 utf32;
			size_t numBytesUsed = UTF8ToUTF32Single(inBegin, utf8Length, utf32);
			if (utf32 != UTF32_INVALID_CODE_POINT) {
				if (utf32 <= 0xFFFF) {
					outText.append('\\');
					outText.append('u');
					char cs[4];
					for (size_t i = 0; i < 4; ++i) {
						cs[i] = (utf32 & 0xF);
						utf32 >>= 4;
					}
					for (size_t i = 0; i < 4; ++i) {
						// 4 hexadecimal characters in big-endian order
						char outc = cs[3-i];
						outc += ((outc < 10) ? '0' : ('A'-char(10)));
						outText.append(outc);
					}
				}
				else if (style == BackslashEscapeStyle::C) {
					outText.append('\\');
					outText.append('U');
					char cs[8];
					for (size_t i = 0; i < 8; ++i) {
						cs[i] = (utf32 & 0xF);
						utf32 >>= 4;
					}
					for (size_t i = 0; i < 8; ++i) {
						// 8 hexadecimal characters in big-endian order
						char outc = cs[7-i];
						outc += ((outc < 10) ? '0' : ('A'-char(10)));
						outText.append(outc);
					}
				}
				else {
					// JSON doesn't have escape sequences for unicode code points
					// beyond 0xFFFF, but does support UTF-8 directly,
					// so higher code points can be left unescaped.
					for (size_t i = 0; i < numBytesUsed; ++i) {
						outText.append(inBegin[i]);
					}
				}
			}
			else if (isNullTerminated && numBytesUsed > 1 && (inBegin[numBytesUsed-1] == 0)) {
				// If a zero byte is encountered in the middle of a multi-byte unicode
				// character, that's not valid, and should also terminate the string.
				return;
			}

			// Skip bytes used from the input.
			inBegin += numBytesUsed;
		}
	}
}

static INLINE uint32 hexCharToValue(char c) {
	if (c >= '0' && c <= '9') {
		return uint32(c - '0');
	}
	// Convert lowercase ASCII to uppercase
	c &= ~0x20;
	if (c >= 'A' && c <= 'F') {
		return uint32(c - ('A'-char(10)));
	}
	// Invalid hex character, so return invalid value above 15.
	return uint32(-1);
}

void unescapeBackslash(const char* inBegin, const char*const inEnd, Array<char>& outText, const char*const stopToken, const BackslashEscapeStyle style) {
	// NOTE: This function intentionally does not clear outText, instead just
	// appending to it, to allow easier processing of text in context.

	if (inBegin == nullptr) {
		// Treat a null input string as an empty string.
		return;
	}

	// inEnd being null indicates a null-terminated string.
	const bool isNullTerminated = (inEnd == nullptr);

	const bool hasStopToken = (stopToken != nullptr) && (stopToken[0] != 0);

	while (isNullTerminated ? (*inBegin != 0) : (inBegin != inEnd)) {
		char c = *inBegin;
		if (hasStopToken && c == stopToken[0]) {
			// Compare with rest of stopToken
			size_t numEqual = 1;
			while (stopToken[numEqual] != 0 && (isNullTerminated || inBegin+numEqual != inEnd) && inBegin[numEqual] == stopToken[numEqual]) {
				++numEqual;
			}
			// TODO: If stopToken ever might be very long and have large overlaps
			// with the text and contain repeats, optimize for that, but it's
			// probably not worth it right now, since stopToken should usually
			// be either short or very unique.

			// Matched full stopToken iff at its end.
			if (stopToken[numEqual] == 0) {
				// Encountered stopToken, so return.
				return;
			}
		}
		++inBegin;
		if (c != '\\') {
			// Unescaped character
			outText.append(c);
			continue;
		}

		// Backslash encountered, so check next character.

		if (isNullTerminated ? (*inBegin == 0) : (inBegin == inEnd)) {
			// End of string with no following character
			return;
		}
		// NOTE: DO NOT check stopToken here,
		// e.g. \" is an escaped double-quote even if stopToken is "

		c = *inBegin;
		++inBegin;
		char outc;
		bool isSingle = true;
		uint32 outc32;
		switch (c) {
			case '\\': outc = '\\'; break;
			case '/':  outc = '/';  break;
			case '\"': outc = '\"'; break;
			case '\'': outc = '\''; break;
			case 't':  outc = '\t'; break;
			case 'n':  outc = '\n'; break;
			case 'r':  outc = '\r'; break;
			case 'f':  outc = '\f'; break;
			case 'b':  outc = '\b'; break;
			case 'v':  outc = '\v'; break;
			case 'a':  outc = 0x07; break;
			case 'e':  outc = 0x1B; break;
			case '?':  outc = '?';  break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7': {
				// 1, 2, or 3 octal digits
				outc32 = (c - '0');
				bool isEnd = isNullTerminated ? (*inBegin == 0) : (inBegin == inEnd);
				if (!isEnd && *inBegin >= '0' && *inBegin <= '7') {
					// 2nd octal digit
					c = *inBegin;
					++inBegin;
					outc32 = (outc32<<3) | (c - '0');
					isEnd = isNullTerminated ? (*inBegin == 0) : (inBegin == inEnd);
					if (!isEnd && *inBegin >= '0' && *inBegin <= '7') {
						// 3rd octal digit
						c = *inBegin;
						++inBegin;
						outc32 = (outc32<<3) | (c - '0');
					}
				}
				// Check if fits in a single byte UTF-8 character.
				if (outc32 < 0x80) {
					outc = char(outc32);
				}
				else {
					isSingle = false;
				}
				break;
			}
			case 'x':   // Should be 2 or more, but accept 1 or more hexadecimal digits
			case 'u':   // Should be 4, but accept 1-4 hex digits
			case 'U': { // Should be 8, but accept 1-8 hex digits
				// If there are none, though, just output 'x'/'u'/'U'.
				bool isEnd = isNullTerminated ? (*inBegin == 0) : (inBegin == inEnd);
				outc = c;

				if (isEnd) {
					// End of string.
					// Fall back to just 'x'/'u'/'U'.
					break;
				}
				uint32 value = hexCharToValue(*inBegin);
				if (value >= 16) {
					// Not a hex character.
					// Fall back to just 'x'/'u'/'U'.
					break;
				}
				// First valid value
				outc32 = value;
				++inBegin;
				size_t charCount = 1;
				const size_t maxCharCount =
					(c == 'u') ? 4 : (
						(c == 'U') ? 8 : std::numeric_limits<size_t>::max()
					);
				while (isNullTerminated ? (*inBegin != 0) : (inBegin != inEnd)) {
					value = hexCharToValue(*inBegin);
					if (value >= 16) {
						// Not a hex character.
						break;
					}
					outc32 = (outc32 << 4) | value;
					++inBegin;
					++charCount;
					if (charCount == maxCharCount) {
						break;
					}
				}

				// Check if fits in a single byte UTF-8 character.
				if (outc32 < 0x80) {
					outc = char(outc32);
				}
				else {
					isSingle = false;
				}
				break;
			}
			default:   outc = c;    break;
		}
		if (isSingle) {
			// Single byte character case
			outText.append(outc);
			continue;
		}

		// Multi-byte character case
		size_t currentSize = outText.size();
		outText.setSize(currentSize + UTF32ToUTF8SingleBytes(outc32));
		UTF32ToUTF8Single(outc32, outText.data() + currentSize);
	}
}

} // namespace text
OUTER_NAMESPACE_END
