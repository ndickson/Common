// This file contains definitions of functions for reading and writing
// arbitrary data from JSON files into a simple in-memory format.
// Declarations of the functions are in JSON.h

#include "json/JSON.h"
#include "text/EscapeText.h"
#include "text/NumberText.h"
#include "File.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Types.h"

#include <memory>

OUTER_NAMESPACE_BEGIN
namespace json {

template<typename CHAR_ITER_T, typename END_T>
static inline CHAR_ITER_T skipWhitespace(CHAR_ITER_T p, const END_T end) {
	while (p != end && *p != 0 && *p <= ' ') {
		++p;
	}
	return p;
}

static const char* parseBinaryJSON(const char* begin, const char* end, std::unique_ptr<Value>& output) {
	// FIXME: Implement this!!!
	return begin;
}

static const char* parseTextJSON(const char* begin, const char* end, std::unique_ptr<Value>& output) {
	// 
	output.reset();

	// Skip any initial whitespace
	begin = skipWhitespace(begin, end);
	if (begin == end || *begin == 0) {
		// Only whitespace, so no Value.
		return begin;
	}

	char c = *begin;

	if (c == '{') {
		// Object start
		++begin;
		ObjectValue* objectValue = new ObjectValue();
		output.reset(objectValue);
		Array<StringValue>& names = objectValue->names;
		Array<std::unique_ptr<Value>>& values = objectValue->values;

		while (true) {
			begin = skipWhitespace(begin, end);
			if (begin == end || *begin == 0) {
				// Early end; treat it as the end of the array.
				return begin;
			}
			c = *begin;
			if (c == '}' || c == ']') {
				// Valid end of the object or mismatched end of the object
				++begin;
				return begin;
			}
			if (c == ',' || c == ':') {
				// Just skip over any comma or colon.
				// This allows many invalid JSON objects to be accepted,
				// but hopefully that's okay.
				++begin;
				continue;
			}

			// First, find the name.
			Array<char> nameString;
			const char* nameBegin = begin;
			const char* nameEnd = nullptr;
			if (c == '\"' || c == '\'') {
				// Quoted name string:
				// Single-quote isn't valid JSON, but accept it anyway.

				++begin;

				// Stop on the same quote as the start of the string.
				const char*const stopToken = (c == '\"') ? "\"" : "\'";

				// Preallocate to the correct length, (+1 for null terminator),
				// to avoid reallocation or wasted space.
				size_t unescapedLength;
				size_t inputStringLength = text::backslashEscapedStringLength(begin, end, stopToken, &unescapedLength, text::BackslashEscapeStyle::JSON);
				const char* stringEnd = begin + inputStringLength;
				nameString.setCapacity(unescapedLength+1);

				// Unescape the string
				text::unescapeBackslash(begin, stringEnd, nameString, stopToken, text::BackslashEscapeStyle::JSON);

				// Null terminator
				nameString.append(0);

				// Add one to skip the ending quote, if that's why
				// the string ended, as opposed to reaching the end of the input.
				begin = stringEnd + (stringEnd != end);
			}
			else {
				// Unquoted name string (not valid JSON):
				// end at anything that's not alphanumeric and not '_', '+', '-', '.', '$', '@', '%', or '?'.
				// These are symbols allowed for different variable names / tokens in different languages.
				// They also cover email addresses and numbers.

				const char* stringEnd = begin;
				while (true) {
					char c = *stringEnd;
					if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
						(c == '_') || (c == '+') || (c == '-') || (c == '.') || (c == '$') || (c == '@') || (c == '%') || (c == '?')
					) {
						++stringEnd;
						if (stringEnd == end) {
							break;
						}
					}
					else {
						break;
					}
				}
				// Copy and add null terminator
				size_t unescapedLength = stringEnd - begin;
				nameString.setSize(unescapedLength+1);
				for (size_t i = 0; i < unescapedLength; ++i) {
					nameString[i] = begin[i];
				}
				nameString[unescapedLength] = 0;
				begin = stringEnd;
			}

			begin = skipWhitespace(begin, end);

			// End after just the name
			if (begin == end || *begin == '}' || *begin == ']') {
				if (nameString.size() != 1) {
					// More than just a null terminator for the name.
					// Add the name referencing a nullptr value.
					names.setSize(names.size()+1);
					names.last().text = std::move(nameString);
					values.append(std::unique_ptr<Value>(nullptr));
				}
				return begin + (begin != end);
			}

			// Add name and (tentatively) empty value.
			names.setSize(names.size()+1);
			names.last().text = std::move(nameString);
			values.setSize(values.size()+1);

			// There should be a colon here, so if there is, skip it.
			// Accept a comma, too, just in case.
			char c = *begin;
			if (c == ':' || c == ',') {
				++begin;
				begin = skipWhitespace(begin, end);
				if (begin == end || *begin == '}' || *begin == ']') {
					// Ended object after colon or comma.
					return begin + (begin != end);
				}
				if (*begin == ':' || *begin == ',') {
					// Empty value; start next pair.
					++begin;
					continue;
				}
			}

			// Parse the value for this name-value pair.
			begin = parseTextJSON(begin, end, values.last());
		}
	}

	if (c == '[') {
		// Array start
		++begin;
		ArrayValueAny* arrayValue = new ArrayValueAny();
		output.reset(arrayValue);
		Array<std::unique_ptr<Value>>& contents = arrayValue->values;

		while (true) {
			begin = skipWhitespace(begin, end);
			if (begin == end || *begin == 0) {
				// Early end; treat it as the end of the array.
				return begin;
			}
			c = *begin;
			if (c == ']' || c == '}') {
				// Valid end of the array or mismatched end of the array
				++begin;
				return begin;
			}
			if (c == ',') {
				// Just skip over any comma.
				// This allows many invalid JSON arrays to be accepted,
				// like ["a" "b" "c"] or [,,1234, ,5678,,,  ,]
				// but hopefully that's okay.
				++begin;
				continue;
			}

			std::unique_ptr<Value> element;
			begin = parseTextJSON(begin, end, element);

			// Append any valid value.  Just ignore invalid values.
			if (element.get() != nullptr) {
				contents.append(std::move(element));
			}
		}
	}
	if (c == ',' || c == ':' || c == ']' || c == '}') {
		// Invalid array or object key/value.
		// Skip at least one character, so that it doesn't keep getting visited.
		++begin;
		return begin;
	}

	// Single-quote strings aren't valid JSON,
	// but accept it anyway, to be more permissive.
	if (c == '\"' || c == '\'') {
		++begin;
		StringValue* string = new StringValue();
		output.reset(string);

		// Stop on the same quote as the start of the string.
		const char*const stopToken = (c == '\"') ? "\"" : "\'";

		// Preallocate to the correct length, (+1 for null terminator),
		// to avoid reallocation or wasted space.
		size_t unescapedLength;
		size_t inputStringLength = text::backslashEscapedStringLength(begin, end, stopToken, &unescapedLength, text::BackslashEscapeStyle::JSON);
		const char* stringEnd = begin + inputStringLength;
		string->text.setCapacity(unescapedLength+1);

		// Unescape the string
		text::unescapeBackslash(begin, stringEnd, string->text, stopToken, text::BackslashEscapeStyle::JSON);

		// Null terminator
		string->text.append(0);

		// Add one to skip the ending quote, if that's why
		// the string ended, as opposed to reaching the end of the input.
		return stringEnd + (stringEnd != end);
	}
	if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
		// Valid JSON numbers can't start with '+', '.',
		// or '0' (unless '0' is the only character of the number),
		// but accept them all anyway, to be more permissive.

		bool stillValid = true;
		const char* numberEnd = begin;
		if (c == '+' || c == '-') {
			// Move the end past the sign character.
			++numberEnd;
			if (numberEnd == end) {
				stillValid = false;
			}
			else {
				c = *numberEnd;
				if (!(c >= '0' && c <= '9') && c != '.') {
					stillValid = false;
				}
			}
		}
		// Any sign character is dealt with already.
		bool hasIntegerPart = false;
		if (stillValid) {
			// Handle integer part.
			if (c >= '0' && c <= '9') {
				hasIntegerPart = true;
				++numberEnd;
				while (numberEnd != end && (*numberEnd >= '0' && *numberEnd <= '9')) {
					++numberEnd;
				}
				if (numberEnd != end) {
					c = *numberEnd;
				}
			}
			// Handle decimal point.
			if (c == '.') {
				++numberEnd;
				if (numberEnd != end) {
					c = *numberEnd;
				}
				if (!hasIntegerPart && (numberEnd == end || !(c >= '0' && c <= '9'))) {
					// If there's a dot with no integer part, the next thing *must* be a digit.
					stillValid = false;
				}
			}
		}

		bool hasExponent = false;
		if (stillValid && numberEnd != end) {
			// Handle fractional part.
			while (numberEnd != end && (*numberEnd >= '0' && *numberEnd <= '9')) {
				++numberEnd;
			}
			if (numberEnd != end) {
				c = *numberEnd;
				if ((c|char(0x20)) == 'e') {
					hasExponent = true;
					++numberEnd;
					if (numberEnd == end) {
						stillValid = false;
					}
					else {
						c = *numberEnd;
						if (c == '+' || c == '-') {
							++numberEnd;
							if (numberEnd == end) {
								stillValid = false;
							}
							else {
								c = *numberEnd;
							}
						}
					}
					if (stillValid && !(c >= '0' && c <= '9')) {
						stillValid = false;
					}
				}
			}
		}
		if (stillValid && hasExponent) {
			++numberEnd;
			while (numberEnd != end && (*numberEnd >= '0' && *numberEnd <= '9')) {
				++numberEnd;
			}
			if (numberEnd != end) {
				c = *numberEnd;
			}
		}
		if (stillValid && numberEnd != end) {
			// If the character following the number isn't whitespace and isn't
			// a separator, end, or beginning of something, give up.
			if (c > ' ' && c != ',' && c != ':' && c != ']' && c != '}' && c != '[' && c != '{' && c != '\"' && c != '\'') {
				stillValid = false;
			}
		}
		if (stillValid) {
			NumberValue* numberValue = new NumberValue();
			output.reset(numberValue);

			// Copy the number text
			size_t numberTextLength = numberEnd-begin;
			numberValue->text.setSize(numberTextLength+1);
			for (size_t i = 0; i < numberTextLength; ++i) {
				numberValue->text[i] = begin[i];
			}
			numberValue->text[numberTextLength] = 0;

			// FIXME: Compute the number value and write it into numberValue->value!!!

			return numberEnd;
		}
	}
	else {
		char cLower = (c | char(0x20));
		if (cLower == 'f') {
			// Check for "false".
			if ((end - begin) >= 5 &&
				(begin[1]|char(0x20)) == 'a' &&
				(begin[2]|char(0x20)) == 'l' &&
				(begin[3]|char(0x20)) == 's' &&
				(begin[4]|char(0x20)) == 'e'
			) {
				output.reset(new SpecialValue(Special::FALSE_));
				return begin+5;
			}
		}
		else if (cLower == 't') {
			// Check for "true".
			if ((end - begin) >= 4 &&
				(begin[1]|char(0x20)) == 'r' &&
				(begin[2]|char(0x20)) == 'u' &&
				(begin[3]|char(0x20)) == 'e'
			) {
				output.reset(new SpecialValue(Special::TRUE_));
				return begin+4;
			}
		}
		else if (cLower == 'n') {
			// Check for "null".
			if ((end - begin) >= 4 &&
				(begin[1]|char(0x20)) == 'u' &&
				(begin[2]|char(0x20)) == 'l' &&
				(begin[3]|char(0x20)) == 'l'
			) {
				output.reset(new SpecialValue(Special::NULL_));
				return begin+4;
			}
		}
	}

	// Fall through to interpreting the text as a string,
	// (which isn't valid JSON, but we're trying to be permissive),
	// breaking at any whitespace or anything that might be an end
	// or beginning of something else
	const char* textEnd = begin;
	// Hitting the end is already handled above.
	assert(textEnd != end);
	do {
		++textEnd;
		if (textEnd == end) {
			break;
		}
		c = *textEnd;
	} while (c > ' ' && c != ',' && c != ':' && c != ']' && c != '}' && c != '[' && c != '{' && c != '\"' && c != '\'');

	StringValue* string = new StringValue();
	output.reset(string);

	// Copy text with null terminator.
	size_t textLength = textEnd - begin;
	string->text.setSize(textLength+1);
	for (size_t i = 0; i < textLength; ++i) {
		string->text[i] = begin[i];
	}
	string->text[textLength] = 0;
	return textEnd;
}

std::unique_ptr<Value> ReadJSONFile(const char* filename) {
	Array<char> contents;
	bool success = ReadWholeFile(filename, contents);
	if (!success || contents.size() == 0) {
		return std::unique_ptr<Value>();
	}

	size_t size = contents.size();
	const char* data = contents.data();
	const char* end = data+size;

	std::unique_ptr<Value> value;

	constexpr uint64 binarySignature =
		(uint64('J')) |
		(uint64('S')<<8) |
		(uint64('O')<<16) |
		(uint64('N')<<24) |
		(uint64('A')<<32) |
		(uint64('T')<<40) |
		(uint64('Q')<<48) |
		(uint64('F')<<56);
	static_assert(sizeof(uint64) == 8);
	if (size >= sizeof(uint64) && *reinterpret_cast<const uint64*>(data) == binarySignature) {
		parseBinaryJSON(data + sizeof(uint64), end, value);
		return std::move(value);
	}
	parseTextJSON(data, end, value);
	return std::move(value);
}

} // namespace json
OUTER_NAMESPACE_END
