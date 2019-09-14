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
// end of the input string.  Note that stopToken will not be checked-for partway
// through escape sequences.
//
// The return value is the address in the input string where unescaping stopped,
// either because text matching stopToken was reached, or inEnd is nullptr and
// a zero byte was reached, or inEnd is not nullptr and inEnd was reached.
// For example, if stopToken contains just a double-quote, this would be the
// end of the quoted string.
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
// Hexadecimal escape sequences with too few digits will be accepted
// as long as there is at least one hexadecimal digit.
const char* unescapeBackslash(const char* inBegin, const char*const inEnd, Array<char>& outText, const char*const stopToken = nullptr, const BackslashEscapeStyle style = BackslashEscapeStyle::C);

// This function finds the length in bytes of the text string starting at inBegin,
// and ending with either text matching stopToken, or a zero byte if inEnd is nullptr,
// or inEnd if inEnd is not nullptr, taking into account that stopToken cannot
// be matched starting partway through an escape sequence.  This is
// equivalent to the return value of unescapeBackslash minus inBegin, so can
// be used for finding the ends of strings containing escape sequences.
//
// If unescapedLength is not nullptr, the length of the unescaped equivalent of the
// text string will be computed and written there.  This is equivalent to the
// number of bytes that would be appended to outText in unescapeBackslash,
// so can be used to determine the size of buffer to preallocate, if needed.
//
// See unescapeBackslash for specifics on how the escape sequences are interpreted
size_t backslashEscapedStringLength(const char* inBegin, const char*const inEnd, const char*const stopToken = nullptr, size_t* unescapedLength = nullptr, const BackslashEscapeStyle style = BackslashEscapeStyle::C);

} // namespace text
OUTER_NAMESPACE_END
