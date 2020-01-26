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

template<typename CHAR_T>
static inline bool isJSONWordEnd(CHAR_T c) {
	return c <= ' ' || c == ',' || c == ':' || c == ']' || c == '}' || c == '[' || c == '{' || c == '\"' || c == '\'';
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

	// text::textToDouble accepts more numbers than valid JSON,
	// e.g. valid JSON doesn't allow numbers starting with '+' or '.',
	// and doesn't allow "infinity" or "NaN", but
	// it's okay to be more permissive here.
	double doubleValue;
	size_t numberLength = text::textToDouble(begin, end, doubleValue);
	if (numberLength != 0) {
		const char*const numberEnd = begin + numberLength;
		// Make sure that the number ends with a valid end.
		if (numberEnd == end || isJSONWordEnd(*numberEnd)) {
			NumberValue* numberValue = new NumberValue();
			output.reset(numberValue);

			// Copy the number text
			size_t numberTextLength = numberEnd-begin;
			numberValue->text.setSize(numberTextLength+1);
			for (size_t i = 0; i < numberTextLength; ++i) {
				numberValue->text[i] = begin[i];
			}
			numberValue->text[numberTextLength] = 0;

			// Compute the number value and write it into numberValue->value
			numberValue->value = doubleValue;

			return numberEnd;
		}
	}

	char cLower = (c | char(0x20));
	if (cLower == 'f') {
		// Check for "false".
		if ((end - begin) >= 5 &&
			(begin[1]|char(0x20)) == 'a' &&
			(begin[2]|char(0x20)) == 'l' &&
			(begin[3]|char(0x20)) == 's' &&
			(begin[4]|char(0x20)) == 'e' &&
			(begin+5 == end || isJSONWordEnd(begin[5]))
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
			(begin[3]|char(0x20)) == 'e' &&
			(begin+4 == end || isJSONWordEnd(begin[4]))
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
			(begin[3]|char(0x20)) == 'l' &&
			(begin+4 == end || isJSONWordEnd(begin[4]))
		) {
			output.reset(new SpecialValue(Special::NULL_));
			return begin+4;
		}
		// Check for "nan" (or any capitlization thereof).
		if ((end - begin) >= 3 && 
			(begin[1]|char(0x20)) == 'a' &&
			(begin[2]|char(0x20)) == 'n' &&
			(begin+3 == end || isJSONWordEnd(begin[3]))
		) {
			NumberValue* numberValue = new NumberValue();
			output.reset(numberValue);
			numberValue->text.setSize(4);
			numberValue->text[0] = begin[0];
			numberValue->text[1] = begin[1];
			numberValue->text[2] = begin[2];
			numberValue->text[3] = 0;
			numberValue->value = std::numeric_limits<double>::quiet_NaN();
			return begin+3;
		}
	}
	else if (cLower == 'i') {
		// Check for "inf" or "infinity".
		if ((end - begin) >= 3 && 
			(begin[1]|char(0x20)) == 'n' &&
			(begin[2]|char(0x20)) == 'f'
		) {
			if (begin+3 == end || isJSONWordEnd(begin[3])) {
				NumberValue* numberValue = new NumberValue();
				output.reset(numberValue);
				numberValue->text.setSize(4);
				numberValue->text[0] = begin[0];
				numberValue->text[1] = begin[1];
				numberValue->text[2] = begin[2];
				numberValue->text[3] = 0;
				numberValue->value = std::numeric_limits<double>::infinity();
				return begin+3;
			}
			else if ((end - begin) >= 8 && 
				(begin[3]|char(0x20)) == 'i' &&
				(begin[4]|char(0x20)) == 'n' &&
				(begin[5]|char(0x20)) == 'i' &&
				(begin[6]|char(0x20)) == 't' &&
				(begin[7]|char(0x20)) == 'y' &&
				(begin+8 == end || isJSONWordEnd(begin[8]))
			) {
				NumberValue* numberValue = new NumberValue();
				output.reset(numberValue);
				numberValue->text.setSize(8);
				for (size_t i = 0; i < 8; ++i) {
					numberValue->text[i] = begin[i];
				}
				numberValue->text[8] = 0;
				numberValue->value = std::numeric_limits<double>::infinity();
				return begin+8;
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
	} while (!isJSONWordEnd(c));

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

static void generateBinaryJSON(const Value& value, Array<char>& output) {

}

static void generateTextJSON(const Value& value, Array<char>& output, size_t firstLineTabs, size_t nestingLevel);

static inline void addIndent(Array<char>& output, size_t numTabs) {
	if (numTabs != 0) {
		size_t origSize = output.size();
		output.setSize(origSize + numTabs);
		for (size_t tabi = 0; tabi < numTabs; ++tabi) {
			output[origSize + tabi] = '\t';
		}
	}
}
static inline void generateNumberText(int8 v, Array<char>& output) {
	text::integerToText(int64(v), output);
}
static inline void generateNumberText(int16 v, Array<char>& output) {
	text::integerToText(int64(v), output);
}
static inline void generateNumberText(int32 v, Array<char>& output) {
	text::integerToText(int64(v), output);
}
static inline void generateNumberText(int64 v, Array<char>& output) {
	text::integerToText(v, output);
}
static inline void generateNumberText(float v, Array<char>& output) {
	text::floatToText(v, output);
}
static inline void generateNumberText(double v, Array<char>& output) {
	text::doubleToText(v, output);
}

static void generateTextJSONString(const StringValue& stringValue, Array<char>& output) {
	assert(stringValue.type == Type::STRING);
	output.append('\"');
	text::escapeBackslash(stringValue.text.begin(), nullptr, output, text::BackslashEscapeStyle::JSON);
	output.append('\"');
}
static inline void generateTextJSONSpecial(const Special value, Array<char>& output) {
	constexpr const char* strings[3] = {
		"false", "true", "null"
	};
	constexpr size_t lengths[3] = {
		5, 4, 4
	};
	size_t index = size_t(value);
	const char* string = strings[index];
	size_t length = lengths[index];
	size_t n = output.size();
	output.setSize(n + length);
	for (size_t i = 0; i < length; ++i) {
		output[n+i] = string[i];
	}
}
static void generateTextJSONObject(const ObjectValue& objectValue, Array<char>& output, size_t firstLineTabs = 0, size_t nestingLevel = 0) {
	assert(objectValue.type == Type::OBJECT);
	assert(objectValue.names.size() == objectValue.values.size());

	// Add indent
	addIndent(output, firstLineTabs);

	size_t n = objectValue.names.size();
	output.append('{');
	output.append('\n');
	for (size_t i = 0; i < n; ++i) {
		addIndent(output, nestingLevel+1);
		generateTextJSONString(objectValue.names[i], output);
		output.append(':');
		auto& valuePtr = objectValue.values[i];
		if (valuePtr.get() != nullptr) {
			generateTextJSON(*valuePtr, output, 1, nestingLevel+1);
		}
		else {
			output.append('\t');
			generateTextJSONSpecial(Special::NULL_, output);
		}
		output.append(',');
		output.append('\n');
	}
	addIndent(output, nestingLevel);
	output.append('}');
}
static void generateTextJSONArrayAny(const ArrayValueAny& arrayValue, Array<char>& output, size_t firstLineTabs = 0, size_t nestingLevel = 0) {
	assert(arrayValue.type == Type::ARRAY);

	// Add indent
	addIndent(output, firstLineTabs);

	size_t n = arrayValue.values.size();
	output.append('[');
	output.append('\n');
	for (size_t i = 0; i < n; ++i) {
		auto& valuePtr = arrayValue.values[i];
		if (valuePtr.get() != nullptr) {
			generateTextJSON(*valuePtr, output, nestingLevel+1, nestingLevel+1);
		}
		else {
			addIndent(output, nestingLevel+1);
			generateTextJSONSpecial(Special::NULL_, output);
		}
		output.append(',');
		output.append('\n');
	}
	addIndent(output, nestingLevel);
	output.append(']');
}
template<typename T>
static void generateTextJSONArray(const Array<T>& array, Array<char>& output) {
	size_t n = array.size();
	output.append('[');
	if (n > 0) {
		generateNumberText(array[0], output);
		for (size_t i = 0; i < n; ++i) {
			output.append(',');
			output.append(' ');
			generateNumberText(array[i], output);
		}
	}
	output.append(']');
}
static void generateTextJSONNumber(const NumberValue& numberValue, Array<char>& output) {
	if (numberValue.text.size() > 0) {
		size_t length = numberValue.text.size();
		size_t n = output.size();
		output.setSize(n + length);
		for (size_t i = 0; i < length; ++i) {
			output[n + i] = numberValue.text[i];
		}
		return;
	}

	text::doubleToText(numberValue.value, output);
}
static void generateTextJSON(const Value& value, Array<char>& output, size_t firstLineTabs, size_t nestingLevel) {
	// Add indent
	addIndent(output, firstLineTabs);

	switch (value.type) {
		case Type::OBJECT: {
			const ObjectValue& objectValue = static_cast<const ObjectValue&>(value);
			generateTextJSONObject(objectValue, output, 0, nestingLevel);
			break;
		}
		case Type::ARRAY: {
			const ArrayValueAny& arrayValue = static_cast<const ArrayValueAny&>(value);
			generateTextJSONArrayAny(arrayValue, output, 0, nestingLevel);
			break;
		}
		case Type::STRING: {
			const StringValue& stringValue = static_cast<const StringValue&>(value);
			generateTextJSONString(stringValue, output);
			break;
		}
		case Type::NUMBER: {
			const NumberValue& numberValue = static_cast<const NumberValue&>(value);
			generateTextJSONNumber(numberValue, output);
			break;
		}
		case Type::SPECIAL: {
			const SpecialValue& specialValue = static_cast<const SpecialValue&>(value);
			generateTextJSONSpecial(specialValue.value, output);
			break;
		}
		case Type::INT8ARRAY: {
			const ArrayValue<int8>& arrayValue = static_cast<const ArrayValue<int8>&>(value);
			generateTextJSONArray(arrayValue.values, output);
			break;
		}
		case Type::INT16ARRAY: {
			const ArrayValue<int16>& arrayValue = static_cast<const ArrayValue<int16>&>(value);
			generateTextJSONArray(arrayValue.values, output);
			break;
		}
		case Type::INT32ARRAY: {
			const ArrayValue<int32>& arrayValue = static_cast<const ArrayValue<int32>&>(value);
			generateTextJSONArray(arrayValue.values, output);
			break;
		}
		case Type::INT64ARRAY: {
			const ArrayValue<int64>& arrayValue = static_cast<const ArrayValue<int64>&>(value);
			generateTextJSONArray(arrayValue.values, output);
			break;
		}
		case Type::FLOAT32ARRAY: {
			const ArrayValue<float>& arrayValue = static_cast<const ArrayValue<float>&>(value);
			generateTextJSONArray(arrayValue.values, output);
			break;
		}
		case Type::FLOAT64ARRAY: {
			const ArrayValue<double>& arrayValue = static_cast<const ArrayValue<double>&>(value);
			generateTextJSONArray(arrayValue.values, output);
			break;
		}
	}
}

bool WriteJSONFile(const char* filename, const Value& value, bool binary) {
	Array<char> contents;
	if (binary) {
		generateBinaryJSON(value, contents);
	}
	else {
		generateTextJSON(value, contents, 0, 0);
		contents.append('\n');
	}

	bool success = WriteWholeFile(filename, contents.data(), contents.size());
	return success;
}

} // namespace json
OUTER_NAMESPACE_END
