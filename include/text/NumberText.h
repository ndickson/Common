#pragma once

// This file contains declarations of functions for parsing and generating
// text strings representing numbers.

#include "../Array.h"
#include "../Types.h"
#include <limits>
#include <type_traits>

OUTER_NAMESPACE_BEGIN
namespace text {

using namespace COMMON_LIBRARY_NAMESPACE;

template<uint8 BASE=10,typename CHAR_T>
constexpr INLINE bool isDigit(CHAR_T c) {
	constexpr CHAR_T maxNumDigit = CHAR_T('0'+((BASE <= 10) ? (BASE-1) : 9));
	if (c <= maxNumDigit) {
		return (c >= CHAR_T('0'));
	}

	// Convert alphanumeric characters to uppercase by clearing bit 5.
	const CHAR_T cUpper = (c & ~CHAR_T(0x20));
	constexpr CHAR_T maxAlphaDigit = CHAR_T('A'+(BASE-1 - ((BASE <= 10) ? BASE : 10)));
	return  ((BASE > 10) && (cUpper >= 'A') && (cUpper <= maxAlphaDigit));

}

template<uint8 BASE=10,typename T,typename CHAR_T>
constexpr INLINE bool digitToInteger(CHAR_T c, T& integer) {
	constexpr CHAR_T maxNumDigit = CHAR_T('0'+((BASE <= 10) ? (BASE-1) : 9));
	constexpr CHAR_T maxAlphaDigit = CHAR_T('A'+(BASE-1 - ((BASE <= 10) ? BASE : 10)));
	if ((c >= CHAR_T('0')) && (c <= maxNumDigit)) {
		integer = T(c - CHAR_T('0'));
	}
	else {
		// Convert alphanumeric characters to uppercase by clearing bit 5.
		const CHAR_T cUpper = (c & ~CHAR_T(0x20));
		if ((BASE > 10) && (cUpper >= 'A') && (cUpper <= maxAlphaDigit)) {
			integer = T(c - CHAR_T('A'-10));
		}
		else {
			// Not a number character, so not a number.
			return false;
		}
	}
	return true;
}

// NOTE: digitInteger must be in the range [0,36).
template<typename T>
constexpr INLINE char integerToDigit(const T& digitInteger) {
	return ((digitInteger < 10) ? '0' : ('A'-10)) + digitInteger;
}

// Parses an integer value from text representing the value,
// returning the number of characters of text used by the integer,
// or zero if the text doesn't start with a valid integer.
//
// This function assumes that T can represent numbers up to BASE-1,
// and BASE must be in the range [2,36] (inclusive).
//
// NOTE: This will accept any string *starting* with an integer,
// e.g. if BASE is 10, "2B" will yield the value 2, and return 1,
// for the 1 character of text representing the number.
template<uint8 BASE=10, bool WRAP=true, bool ALLOW_SIGN=true, typename T, typename CHAR_T>
constexpr inline size_t textToInteger(const CHAR_T* text, const CHAR_T*const end, T& integer) {
	integer = T(0);
	static_assert(BASE >= 2 && BASE <= 36, "For base 64, use base64ToInteger.");

	if (text == end || *text == CHAR_T(0)) {
		// Empty string doesn't represent a number.
		return 0;
	}

	const CHAR_T*const origText = text;
	CHAR_T c = *text;
	++text;

	bool negate = false;
	if constexpr (ALLOW_SIGN) {
		if ((c == CHAR_T('+')) || (c == CHAR_T('-'))) {
			negate = (c == CHAR_T('-'));
			if (text == end || *text == CHAR_T(0)) {
				// Just a sign doesn't represent a number.
				return 0;
			}
			c = *text;
			++text;
		}
	}

	bool isCharDigit = digitToInteger<BASE>(c, integer);
	if (!isCharDigit) {
		// No number characters, so not a number.
		return 0;
	}

	while (true) {
		if (text == end || *text == CHAR_T(0)) {
			// Finished the number
			return text - origText;
		}
		c = *text;
		++text;

		CHAR_T digitInteger;
		bool isCharDigit = digitToInteger<BASE>(c, digitInteger);
		if (!isCharDigit) {
			// Not a number character, so done.
			break;
		}

		// Number character, so add in new contribution.
		if constexpr (WRAP) {
			integer = BASE*integer + digitInteger;
		}
		else {
			// Check if would overflow and if so, limit instead.
			T potentialInteger = BASE*integer + digitInteger;
			if (potentialInteger / BASE == integer) {
				integer = std::move(potentialInteger);
			}
			else
			{
				if constexpr (!ALLOW_SIGN) {
					integer = std::numeric_limits<T>::max();
				}
				else {
					if (!negate) {
						integer = std::numeric_limits<T>::max();
					}
					else {
						integer = std::numeric_limits<T>::lowest();
						negate = false;
					}
				}
			}
		}
	}

	// Already incremented text in the loop, so decrement it
	// to exclude the non-number character.
	return text-1 - origText;
}

// This determines the value of a float that is clsoest to the
// decimal value represented by the provided text,
// returning the number of characters of text used by the number,
// or zero if the text doesn't start with a valid number.
//
// This supports "inf", "infinity", or "nan" in any capitalization
// with an optional + or - sign.
//
// NOTE: This will accept any string *starting* with a number,
// e.g. "2B" will yield the value 2, and return 1,
// for the 1 character of text representing the number.
COMMON_LIBRARY_EXPORTED size_t textToFloat(const char* text, const char*const end, float& value);

// This determines the value of a double that is clsoest to the
// decimal value represented by the provided text,
// returning the number of characters of text used by the number,
// or zero if the text doesn't start with a valid number.
//
// This supports "inf", "infinity", or "nan" in any capitalization
// with an optional + or - sign.
//
// NOTE: This will accept any string *starting* with a number,
// e.g. "2B" will yield the value 2, and return 1,
// for the 1 character of text representing the number.
COMMON_LIBRARY_EXPORTED size_t textToDouble(const char* text, const char*const end, double& value);

// Generates text representing the given integer in the given base.
// If T is a signed integer type and integer is negative,
// a negative sign will be added to the beginning.
//
// NOTE: This does not add a null terminator character.
// NOTE: This appends to text, NOT replacing the existing content.
template<uint8 BASE=10, typename T, typename CHAR_T>
constexpr inline void integerToText(const T& integer, Array<CHAR_T>& text) {
	static_assert(BASE >= 2 && BASE <= 36, "For base 64, use integerToBase64.");

	// To correctly handle -2^63 with T being int64_t, or similar cases,
	// we need an extra bit, since simply negating it will not make it positive.
	// The unsigned integer type can always fit the absolute value.
	using UNSIGNED_T = typename std::make_unsigned<T>::type;
	// The value in this initialization is only used for non-negative values,
	// but nonnegativeInteger must be initialized in its declaration statement,
	// else the function can't be constexpr.
	UNSIGNED_T nonnegativeInteger = UNSIGNED_T(integer);

	size_t digitsBegin = text.size();

	// This if constexpr block is to work around a warning on (value < 0)
	// for unsigned types on some compilers.
	if constexpr (!std::is_unsigned<T>::value) {
		if (integer < 0) {
			// Handle negative values
			text.append('-');
			++digitsBegin;

			// NOTE: The bit representations of -2^63 and 2^63 are the same for int64_t/uint64_t,
			// and similar for other integer types, so this will still work for those values.
			nonnegativeInteger = UNSIGNED_T(-integer);
		}
	}

	// nonnegativeInteger is zero or positive, so integer modulus and division
	// should work as needed.

	// First, create the number's text backwards,
	// from low order digits to high order digits.
	// NOTE: This is a do-while loop so that there is always at least one digit;
	// this way, no special case is needed for value zero.
	do {
		// Determine current digit, its character, and write it.
		// Digit is always less than 36, so will fit in a single byte.
		const char digitInteger = char(nonnegativeInteger % BASE);
		// Digits 0 to 9 become '0' to '9'; digits 10-35 become 'A' to 'Z'.
		const char digitCharacter = integerToDigit(digitInteger);
		text.append(digitCharacter);

		// Remove low order digit from nonnegativeInteger.
		// NOTE: Integer division of positive integers always rounds down.
		// If some future architecture decides not to follow this,
		// subtract digit from value before dividing, just in case.
		nonnegativeInteger /= BASE;

		// No more digits if value is zero.
	} while (nonnegativeInteger > 0);

	size_t digitsEnd = text.size();

	// Then, reverse the number's text, since numbers
	// should be written with high order digits first.
	while (digitsEnd-digitsBegin >= 2) {
		// Swap characters while there are at least two left to swap.
		// NOTE: std::swap is not constexpr.
		CHAR_T& a = text[digitsBegin];
		CHAR_T& b = text[digitsEnd-1];
		const CHAR_T temp = a;
		a = b;
		b = temp;
		--digitsEnd;
		++digitsBegin;
	}
}

// This generates the text representation of value with the fewest digits
// that will produce the exact same value when passed into textToFloat.
// For finite numbers, the output will always have a decimal point dot
// and at least one digit after it, to ensure as many places reading it
// as possible will identify the number as floating-point.
//
// Scientific notation (powers of 10) is used for numbers >= 1.0e9 or <= 1.0e-4.
// Positive infinity is "infinity"; negative infinity is "-infinity".
// Any NaN is "NaN".
//
// NOTE: This does not add a null terminator character.
// NOTE: This appends to text, NOT replacing the existing content.
COMMON_LIBRARY_EXPORTED void floatToText(float value, Array<char>& text);

// This generates the text representation of value with the fewest digits
// that will produce the exact same value when passed into textToDouble.
// For finite numbers, the output will always have a decimal point dot
// and at least one digit after it, to ensure as many places reading it
// as possible will identify the number as floating-point.
//
// Scientific notation (powers of 10) is used for numbers >= 1.0e9 or <= 1.0e-4,
// for consistency with floatToText, even though an upper threshold around
// 1.0e18 would be more fitting for double.  Also, it's fairly common to
// have large numbers that are multiples of thousands or millions,
// which will reduce the number of characters when expressed with scientific notation.
// Positive infinity is "infinity"; negative infinity is "-infinity".
// Any NaN is "NaN".
//
// NOTE: This does not add a null terminator character.
// NOTE: This appends to text, NOT replacing the existing content.
COMMON_LIBRARY_EXPORTED void doubleToText(double value, Array<char>& text);

} // namespace text
OUTER_NAMESPACE_END
