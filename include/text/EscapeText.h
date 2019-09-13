#pragma once

// This file contains declarations of functions for escaping and unescaping
// text strings in various styles.

#include "../Array.h"
#include "../Types.h"

OUTER_NAMESPACE_BEGIN
namespace text {

using namespace Common;

enum class BackslashEscapeStyle {
	JSON,
	C
};

// This function escapes the text string from inBegin to inEnd using backslashes,
// appending the resulting text string to outText.
// If inEnd is nullptr, inBegin is treated as null-terminated.
// This does nothing if inBegin is nullptr.
//
// In all, this function escapes:
//     backslash as \\ (extra text so this line doesn't end with a backslash)
//     tab (0x09) as \t
//     double-quote as \"
//     line feed (0x0A) as \n
//     carriage return (0x0D) as \r
//     form feed (0x0C) as \f
//     backspace (0x08) as \b
//     single-quote as \' if style is BackslashEscapeStyle::C
//     vertical tab (0x0B) as \v if style is BackslashEscapeStyle::C
//     null byte (0x00) as \u0000 (avoiding issues if \0 were to be followed by a number 0-7)
//     any other control characters as \u00XX
//     multi-byte UTF-8 characters 0000-FFFF as \uXXXX
//     multi-byte UTF-8 characters 10000-.... as \UXXXXXXXX if style is BackslashEscapeStyle::C,
//         else keeping them as is, since JSON supports UTF-8, but not \U escaping.
void escapeBackslash(const char* inBegin, const char*const inEnd, Array<char>& outText, const BackslashEscapeStyle style);

// This function unescapes any backslash escape sequences in the text string
// from inBegin to inEnd, appending the resulting text string to outText.
// If inEnd is nullptr, inBegin is treated as null-terminated.
// This does nothing if inBegin is nullptr.
//
// If stopToken is not nullptr, it must point to a null-terminated UTF-8 text string,
// indicating when to stop early, e.g. a double-quote, making parsing a quoted string
// inside a larger string easier.  The start of the match will be considered the
// end of the input string.  Note that stopToken will not be checked-for inside of
// escape sequences.
//
// In all, this function unescapes:
//     \\ as backslash
//     \/ as forward slash
//     \t as tab
//     \" as double-quote
//     \' as single-quote
//     \n as line feed (0x0A)
//     \r as carriage return (0x0D)
//     \f as form feed (0x0C)
//     \b as backspace (0x08)
//     \v as vertical tab (0x0B)
//     \a as alert bell (0x07)
//     \e as ASCII "escape" character (0x1B)
//     \? as question mark
//     \# \## or \### where # are all octal digits (0-7) as octal UTF code point
//     \x##... where variable number of # are hexadecimal as hexadecimal UTF code point
//     \u#### where # are all hexadecimal as hexadecimal UTF code point
//     \U######## where # are all hexadecimal as hexadecimal UTF code point
// Any other escape sequences are copied as is without the backslash.
void unescapeBackslash(const char* inBegin, const char*const inEnd, Array<char>& outText, const char*const stopToken = nullptr, const BackslashEscapeStyle style = BackslashEscapeStyle::C);

} // namespace text
OUTER_NAMESPACE_END
