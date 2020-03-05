// This file contains definitions of functions for reading data from
// tables of text.

#include "text/DataTable.h"
#include "text/EscapeText.h"
#include "text/NumberText.h"
#include "text/TextFunctions.h"
#include "text/UTF.h"
#include "Array.h"
#include "ArrayDef.h"
#include "File.h"
#include "SharedString.h"
#include "SharedStringDef.h"
#include "Span.h"
#include "Types.h"

#include <memory>

OUTER_NAMESPACE_BEGIN
namespace text {

using namespace Common;

static constexpr int componentIndexFromCharacter(const char c) {
	switch (c) {
		case 'x': // xyz
		case 'u': // uvw
		case 'r': // rgba
		case 's': // st
			return 0;

		case 'y':
		case 'v':
		case 'g':
		case 't':
			return 1;

		case 'z':
		case 'w':
		case 'b':
			return 2;

		case 'a':
			return 3;

		default:
			return -1;
	}
}

static constexpr int minComponentsFromCharacter(const char c) {
	switch (c) {
	case 'x': // xy
	case 'y':
	case 'u': // uv
	case 'v':
	case 's': // st
	case 't':
		return 2;

	case 'r': // rgb
	case 'g':
	case 'b':
	case 'z': // xyz
	case 'w': // uvw
		return 3;

	case 'a': // rgba
		return 4;

	default:
		return 1;
	}
}

static void findNameComponentInfo(const char* name, const char*& nameEnd, uint32& componentIndex, uint32& minNumComponents) {
	// Check for components like ".x"
	int localComponentIndex = -1;
	minNumComponents = 1;
	if ((nameEnd-name) >= 3 && *(nameEnd-2) == '.') {
		const char c = *(nameEnd-1);
		localComponentIndex = componentIndexFromCharacter(c);
		if (localComponentIndex >= 0) {
			minNumComponents = uint32(minComponentsFromCharacter(c));
			// Remove the component from the name.
			nameEnd -= 2;
		}
	}
	if (localComponentIndex < 0) {
		componentIndex = 0;
	}
	else {
		componentIndex = uint32(localComponentIndex);
	}
}

static void increaseArraySize(TableData::NamedData& metadata, TableData& table) {
	const size_t newArrarySize = metadata.numComponents*table.numDataPoints;
	if (metadata.type == TableData::INT64) {
		if (metadata.typeArrayIndex == TableData::NamedData::INVALID_INDEX) {
			metadata.typeArrayIndex = table.intArrays.size();
			table.intArrays.setSize(metadata.typeArrayIndex + 1);
		}
		if (newArrarySize != 0) {
			// Set the array size.
			Array<int64>& array = table.intArrays[metadata.typeArrayIndex];
			size_t oldArraySize = array.size();
			array.setSize(newArrarySize);
			// Default to value zero.
			for (size_t i = oldArraySize; i != newArrarySize; ++i) {
				array[i] = int64(0);
			}
		}
	}
	else if (metadata.type == TableData::FLOAT64) {
		if (metadata.typeArrayIndex == TableData::NamedData::INVALID_INDEX) {
			metadata.typeArrayIndex = table.doubleArrays.size();
			table.doubleArrays.setSize(metadata.typeArrayIndex + 1);
		}
		if (newArrarySize != 0) {
			// Set the array size.
			Array<double>& array = table.doubleArrays[metadata.typeArrayIndex];
 			size_t oldArraySize = array.size();
			array.setSize(newArrarySize);
			// Default to value zero.
			for (size_t i = oldArraySize; i != newArrarySize; ++i) {
				array[i] = double(0);
			}
		}
	}
	else if (metadata.type == TableData::STRING) {
		if (metadata.typeArrayIndex == TableData::NamedData::INVALID_INDEX) {
			metadata.typeArrayIndex = table.stringArrays.size();
			table.stringArrays.setSize(metadata.typeArrayIndex + 1);
		}
		if (newArrarySize != 0) {
			// Set the array size.
			Array<SharedString>& array = table.stringArrays[metadata.typeArrayIndex];
			array.setSize(newArrarySize);
			// SharedString is a non-POD type, so will be initialized to represent nullptr.
		}
	}
}

static void findOrAddArray(const char* name, const char* nameEnd, TableData& table, TableData::DataType defaultType, uint32& componentIndex, size_t& arrayIndex) {
	// Check for components like ".x"
	uint32 minNumComponents;
	findNameComponentInfo(name, nameEnd, componentIndex, minNumComponents);

	// Look for the name in existing array names.
	size_t nameSize = nameEnd-name;
	ShallowString shallowName(name, nameSize);
	size_t numArrays = table.metadata.size();
	arrayIndex = numArrays;
	for (size_t arrayi = 0; arrayi < numArrays; ++arrayi) {
		if (table.metadata[arrayi].name == shallowName) {
			arrayIndex = arrayi;
			break;
		}
	}

	if (arrayIndex == numArrays) {
		table.metadata.append(TableData::NamedData{SharedString(shallowName), defaultType, minNumComponents, TableData::NamedData::INVALID_INDEX});
		if (defaultType != TableData::UNDETERMINED) {
			increaseArraySize(table.metadata.last(), table);
		}
	}
	TableData::NamedData& metadata = table.metadata[arrayIndex];
	// Ensure that the number of components is sufficient for this component,
	// unless there are already data points.
	if (metadata.numComponents < minNumComponents && table.numDataPoints == 0) {
		metadata.numComponents = minNumComponents;
	}
}

// Returns true iff the UTF-8 code point starting at text matches a separator code point.
// If there's a match, the text pointer is updated to point after the separator.
static bool matchesASeparator(
	const char*& text,
	const char* textEnd,
	const char* separators,
	const char*const separatorsEnd
) {
	// The caller must ensure that the text isn't empty.
	assert(text != textEnd);

	const char firstCharacter = *text;
	while (separators != separatorsEnd) {
		if (firstCharacter == *separators) {
			const char* localText = text;
			++separators;
			++localText;
			bool match = true;
			// Handle separator code points that may be multiple bytes.
			// Assume that at least each separator starts with a valid
			// first byte for a UTF-8 character encoding.
			while (separators != separatorsEnd && !UTF8IsFirstByte(*separators)) {
				if (localText == textEnd || *localText != *separators) {
					match = false;
					break;
				}
				++separators;
				++localText;
			}
			if (match) {
				// Match found, so update text pointer.
				text = localText;
				return true;
			}
			// Skip the rest of the current separator, since it doesn't match.
			do {
				++separators;
			} while (separators != separatorsEnd && !UTF8IsFirstByte(*separators));
		}
		else {
			// Skip the current separator, since it doesn't match.
			do {
				++separators;
			} while (separators != separatorsEnd && !UTF8IsFirstByte(*separators));
		}
	}

	// No match found.  Don't update text pointer.
	return false;
}

// Finds the spans of each column within line.
// Note that this doesn't modify the text at all; any text processing,
// e.g. unescaping of quoted strings with escape sequences, must be done afterward.
static void splitColumns(
	const char* line,
	const char*const lineEnd,
	Array<Span<size_t>>& columns,
	const char*const separators,
	const char*const separatorsEnd,
	bool treatMultipleSeparatorsAsOne,
	const size_t*const columnWidths,
	size_t numColumnWidths
) {
	columns.setSize(0);
	if (line == lineEnd) {
		return;
	}

	if (separators == nullptr) {
		// Split using fixed column widths.
		assert(numColumnWidths >= 1);

		size_t columni = 0;
		const char*const lineBegin = line;

		for (; line != lineEnd; ++columni) {
			size_t columnBeginIndex = line - lineBegin;
			if (columni >= numColumnWidths) {
				columni = numColumnWidths-1;
			}
			size_t columnWidth = columnWidths[columni];

			if (columnWidth == 0) {
				columns.append(Span<size_t>(columnBeginIndex, columnBeginIndex));
				if (columni == numColumnWidths-1) {
					// Need to break out of the loop if not advancing line.
					break;
				}
				continue;
			}

			// Use UTF-8 code points for counting the width, instead of bytes,
			// since it's much closer to what people would expect, even though it's still not perfect.
			do {
				bool isValid;
				size_t codePointSize = UTF8ToUTF32SingleBytes(line, lineEnd-line, isValid);
				line += codePointSize;
				--columnWidth;
			} while(columnWidth != 0 && line < lineEnd);

			size_t columnEndIndex = line - lineBegin;
			columns.append(Span<size_t>(columnBeginIndex, columnEndIndex));
		}

		return;
	}

	// Split using separators, taking quotes into account.
	const char*const lineBegin = line;
	size_t columnBeginIndex = line - lineBegin;
	while (line != lineEnd) {
		const char c = *line;
		if (c == '\"' || c == '\'') {
			// Find the length of the quoted string with possible escape sequences.
			const char stopToken[2] = {c, 0};
			size_t length = backslashEscapedStringLength(line+1, lineEnd, stopToken);
			// Skip the first quote and the contents.
			line += (1+length);
			// Skip the ending quote if there is one.
			line += (line != lineEnd);
			continue;
		}

		const size_t index = line - lineBegin;
		if (matchesASeparator(line, lineEnd, separators, separatorsEnd)) {
			// Always combine multiple spaces as a single separator.
			if ((!treatMultipleSeparatorsAsOne && c != ' ') || index != columnBeginIndex) {
				columns.append(Span<size_t>(columnBeginIndex, index));
				columnBeginIndex = line - lineBegin;
			}
			continue;
		}

		// Regular character, so include it in the current column.
		++line;
	}
	const size_t index = line - lineBegin;
	if (index != columnBeginIndex) {
		columns.append(Span<size_t>(columnBeginIndex, index));
	}
}

static TableData::DataType guessTypeFromText(const char* text,const char* const textEnd) {
	// Guess the data type based on the content of the first piece of data.
	// If there are only digits, optionally after a negative or positive sign, guess integer.
	// If there is only [+-]?[0-9]*(.[0-9]*)?([Ee][+-]?[0-9]+)? and there's at least one digit, guess floating-point.
	// Otherwise, guess string.
	bool hasSign = false;
	bool hasDigits = false;
	bool hasDecimalPoint = false;
	bool hasExponentChar = false;
	for (const char* current = text; current != textEnd; ++current) {
		const char c = *current;
		if (c == '+' || c == '-') {
			if (hasSign || hasDigits || hasDecimalPoint) {
				// Can't have multiple sign characters,
				// or after digits, or after a decimal point.
				// The above flags get reset after an exponent 'e',
				// so it is valid to have a sign after the 'e'.
				return TableData::STRING;
			}
			hasSign = true;
		}
		else if (c >= '0' && c <= '9') {
			hasDigits = true;
		}
		else if (c == '.') {
			if (hasDecimalPoint || hasExponentChar) {
				// Can't have multiple decimal points or
				// a decimal point in the exponent.
				return TableData::STRING;
			}
			hasDecimalPoint = true;
		}
		else if (c == 'E' || c == 'e') {
			if (hasExponentChar || !hasDigits) {
				// Can't have multiple exponent characters,
				// or an exponent character without any digits.
				return TableData::STRING;
			}
			hasExponentChar = true;
			// After this, can have a sign, must have digits,
			// and can't have a decimal point.
			hasSign = false;
			hasDigits = false;
			hasDecimalPoint = false;
		}
		else {
			// Can't have any invalid characters in a number.
			return TableData::STRING;
		}
	}

	if (!hasDigits) {
		// Can't have a number without digits.
		return TableData::STRING;
	}
	if (!hasDecimalPoint && !hasExponentChar) {
		return TableData::INT64;
	}
	return TableData::FLOAT64;
}

bool ReadTableText(const char* text, const char*const textEnd, const TableOptions& options, TableData& table) {
	Array<Span<size_t>> lines;
	splitLines(text, textEnd, lines, true);

	Array<Array<char>> newLines;
	if (options.preprocessFunction) {
		// NOTE: This can't be a BufArray, since newLines needs to
		// be able to take ownership of the contents.
		Array<char> newText;
		size_t destLinei = 0;
		for (size_t sourceLinei = 0, numSourceLines = lines.size(); sourceLinei < numSourceLines; ++sourceLinei) {
			Span<size_t> span = lines[sourceLinei];
			const char* line = text + span[0];
			const char* lineEnd = text + span[1];
			bool keepLine = options.preprocessFunction(line, lineEnd, newText);
			if (keepLine) {
				if (sourceLinei != destLinei) {
					lines[destLinei] = span;
				}
				++destLinei;

				if (newText.size() != 0) {
					newLines.setSize(destLinei+1);
					newLines[destLinei] = std::move(newText);
				}
			}
		}
		if (lines.size() != destLinei) {
			lines.setSize(destLinei);
		}
	}

	size_t linei = 0;

	// Find the string containing the names of the data in the table.
	const char* dataNames = options.dataNames;
	const char* dataNamesEnd;
	if (dataNames == nullptr) {
		if (linei < lines.size()) {
			if (linei >= newLines.size() || newLines[linei].size() == 0) {
				dataNames = text + lines[linei][0];
				dataNamesEnd = text + lines[linei][1];
			}
			else {
				dataNames = newLines[linei].begin();
				dataNamesEnd = newLines[linei].end();
			}
			++linei;
		}
		else {
			// No lines of data and no names, so nothing to do.
			return false;
		}
	}
	else {
		dataNamesEnd = findFirstCharacter(dataNames, nullptr, 0);
	}

	// If there's a linear independent array to create, create it.
	size_t linearArrayIndex = TableData::NamedData::INVALID_INDEX;
	uint32 linearArrayComponent;
	bool hasLinear = false;
	if (options.independentType == TableOptions::LINEAR) {
		const char* nameBegin = options.independentLinearName;
		const char* nameEnd = nameBegin + text::stringSize(nameBegin);

		TableData::DataType defaultType = options.defaultType;
		if (defaultType == TableData::STRING || defaultType == TableData::UNDETERMINED) {
			defaultType = TableData::FLOAT64;
		}

		uint32 componentIndex;
		size_t arrayIndex;
		findOrAddArray(nameBegin, nameEnd, table, defaultType, componentIndex, arrayIndex);

		TableData::DataType type = table.metadata[arrayIndex].type;
		if (type == TableData::UNDETERMINED) {
			type = TableData::FLOAT64;
			table.metadata[arrayIndex].type = type;
			increaseArraySize(table.metadata[arrayIndex], table);
		}
		if (type == TableData::FLOAT64 || type == TableData::INT64) {
			linearArrayIndex = arrayIndex;
			linearArrayComponent = componentIndex;
			hasLinear = true;
		}
	}

	// Add ColSeries, if requested.
	size_t colSeriesArrayIndex = TableData::NamedData::INVALID_INDEX;
	bool hasColSeries = false;
	if (options.createColSeries) {
		const char* nameBegin = "ColSeries";
		const char* nameEnd = nameBegin + text::stringSize(nameBegin);

		TableData::DataType defaultType = options.defaultType;
		if (defaultType == TableData::STRING || defaultType == TableData::UNDETERMINED) {
			defaultType = TableData::INT64;
		}

		uint32 componentIndex;
		size_t arrayIndex;
		findOrAddArray(nameBegin, nameEnd, table, defaultType, componentIndex, arrayIndex);
		assert(componentIndex == 0);

		TableData::DataType type = table.metadata[arrayIndex].type;
		if (type == TableData::UNDETERMINED) {
			type = TableData::INT64;
			table.metadata[arrayIndex].type = type;
			increaseArraySize(table.metadata[arrayIndex], table);
		}
		if (type == TableData::FLOAT64 || type == TableData::INT64) {
			colSeriesArrayIndex = arrayIndex;
			hasColSeries = true;
		}
	}

	// Add RowSeries, if requested.
	size_t rowSeriesArrayIndex = TableData::NamedData::INVALID_INDEX;
	bool hasRowSeries = false;
	if (options.createRowSeries) {
		const char* nameBegin = "RowSeries";
		const char* nameEnd = nameBegin + text::stringSize(nameBegin);

		TableData::DataType defaultType = options.defaultType;
		if (defaultType == TableData::STRING || defaultType == TableData::UNDETERMINED) {
			defaultType = TableData::INT64;
		}

		uint32 componentIndex;
		size_t arrayIndex;
		findOrAddArray(nameBegin, nameEnd, table, defaultType, componentIndex, arrayIndex);
		assert(componentIndex == 0);

		TableData::DataType type = table.metadata[arrayIndex].type;
		if (type == TableData::UNDETERMINED) {
			type = TableData::INT64;
			table.metadata[arrayIndex].type = type;
			increaseArraySize(table.metadata[arrayIndex], table);
		}
		if (type == TableData::FLOAT64 || type == TableData::INT64) {
			rowSeriesArrayIndex = arrayIndex;
			hasRowSeries = true;
		}
	}

	constexpr static char defaultSeparators[4] = {' ', '\t', ',', ';'};
	constexpr static const char* defaultSeparatorsEnd = defaultSeparators+4;
	// Use the default separators if getting the data names from options,
	// instead of from the table text.
	const char* dataNameSeparators = defaultSeparators;
	const char* dataNameSeparatorsEnd = defaultSeparatorsEnd;
	if (options.dataNames == nullptr) {
		dataNameSeparators = options.columnSeparators;
		if (options.columnSeparators != nullptr) {
			dataNameSeparatorsEnd = findFirstCharacter(options.columnSeparators, nullptr, 0);
		}
		else {
			// No separators specified, so use column widths.
			dataNameSeparatorsEnd = nullptr;
			if (options.numColumnWidths == 0) {
				// No column widths specified, so fall back to default separators.
				dataNameSeparators = defaultSeparators;
				dataNameSeparatorsEnd = defaultSeparatorsEnd;
			}
		}
	}

	BufArray<Span<size_t>,4> columns;
	splitColumns(
		dataNames,
		dataNamesEnd,
		columns,
		dataNameSeparators,
		dataNameSeparatorsEnd,
		options.treatMultipleSeparatorsAsOne,
		options.columnWidths,
		options.numColumnWidths);

	const size_t numUniqueColumns = columns.size();

	BufArray<char,32> tempText;

	BufArray<size_t,4> columnArrayIndices;
	BufArray<uint32,4> columnComponents;
	columnArrayIndices.setSize(numUniqueColumns);
	columnComponents.setSize(numUniqueColumns);

	// Create any named arrays associated with the columns.
	for (size_t columni = 0; columni < numUniqueColumns; ++columni) {
		const auto& columnSpan = columns[columni];
		const char*const textBegin = dataNames + columnSpan[0];
		const char*const textEnd = dataNames + columnSpan[1];

		// Only escape quoted strings if the text starts with a quote and spans the full text.
		size_t textSize = textEnd-textBegin;
		const char* nameBegin = textBegin;
		const char* nameEnd = textEnd;
		if (textSize >= 2 && (*textBegin == '\"' || *textBegin == '\'')) {
			char stopToken[2] = {*textBegin, 0};
			size_t unescapedLength;
			size_t escapedLength = backslashEscapedStringLength(textBegin+1, textEnd, stopToken, &unescapedLength);
			if (escapedLength == textSize-2) {
				// Pre-allocate tempText.
				tempText.setSize(unescapedLength);
				tempText.setSize(0);
				// Don't need stopToken, since we already found it was at textEnd.
				unescapeBackslash(textBegin+1, textEnd-1, tempText);

				nameBegin = tempText.begin();
				nameEnd = tempText.end();
			}
		}

		uint32 componentIndex;
		size_t arrayIndex;
		findOrAddArray(nameBegin, nameEnd, table, options.defaultType, componentIndex, arrayIndex);

		// TODO: Handle the case of multiple columns for the same component of the same array.
		columnArrayIndices[columni] = arrayIndex;
		columnComponents[columni] = componentIndex;
	}

	size_t numIndependentCols = (options.independentType == TableOptions::FIRST_COLUMNS) ? options.numIndependentCols : 0;
	size_t numDependentCols = numUniqueColumns - numIndependentCols;

	// NOTE: Can't create the arrays or update the indices into the array arrays here,
	// since we don't necessarily know all of the data types yet.

	// FIXME: Handle series numbers!!!

	const size_t firstLineIndex = linei;
	const size_t numDataPointsPerColSeries = lines.size() - firstLineIndex;
	const size_t initialNumDataPoints = table.numDataPoints;
	size_t dataPointIndex = initialNumDataPoints;
	size_t numColSeries = 0;
	for (size_t numLines = lines.size(); linei < numLines; ++linei) {
		size_t rowStartDataPointIndex = dataPointIndex;
		size_t currentColSeries = 0;
		size_t previousColSeries;

		const char* line = text + lines[linei][0];
		const char* lineEnd = text + lines[linei][1];
		if (linei < newLines.size() && newLines[linei].size() != 0) {
			line = newLines[linei].begin();
			lineEnd = newLines[linei].end();
		}
		splitColumns(
			line,
			lineEnd,
			columns,
			dataNameSeparators,
			dataNameSeparatorsEnd,
			options.treatMultipleSeparatorsAsOne,
			options.columnWidths,
			options.numColumnWidths);

		size_t numColumnsThisRow = columns.size();

		for (size_t columni = 0; columni < numColumnsThisRow; ++columni) {

			size_t uniqueColumni = columni;
			if (columni >= numUniqueColumns) {
				if (numDependentCols == 0) {
					// Can't have multiple values for independent
					// columns in a single row.
					break;
				}
				uniqueColumni = ((columni - numIndependentCols) % numDependentCols) + numIndependentCols;
				if (uniqueColumni == numIndependentCols) {
					if (options.rowMajor) {
						++dataPointIndex;
					}
					else {
						dataPointIndex += numDataPointsPerColSeries;
					}
					++currentColSeries;
				}
			}

			const size_t origNumDataPoints = table.numDataPoints;
			if (options.rowMajor && dataPointIndex >= table.numDataPoints) {
				table.numDataPoints = initialNumDataPoints + dataPointIndex+1;
			}
			else if (!options.rowMajor && currentColSeries >= numColSeries) {
				numColSeries = currentColSeries+1;
				table.numDataPoints = initialNumDataPoints + numColSeries*numDataPointsPerColSeries;
			}
			if (table.numDataPoints != origNumDataPoints) {
				// Resize all arrays, taking into account the number of components.
				for (auto& otherMeta : table.metadata) {
					// NOTE: otherMeta.type may be UNDETERMINED, in which case,
					// it will be initialized below before the first write to it.
					increaseArraySize(otherMeta, table);
				}
			}

			// Now that the arrays are resized, if we're adding implicit arrays,
			// compute and store the values for them.
			if (columni == 0 || currentColSeries != previousColSeries) {
				size_t rowNumber = (linei - firstLineIndex);
				if (hasLinear) {
					const TableData::NamedData& linearMetadata = table.metadata[linearArrayIndex];
					const double value = options.independentLinearBegin + rowNumber*options.independentLinearIncrement;
					uint32 numComponents = linearMetadata.numComponents;
					if (linearArrayComponent < numComponents) {
						size_t currentIndex = dataPointIndex*numComponents + linearArrayComponent;
						if (linearMetadata.type == TableData::FLOAT64) {
							Array<double>& array = table.doubleArrays[linearMetadata.typeArrayIndex];
							array[currentIndex] = value;
						}
						else {
							assert(linearMetadata.type == TableData::INT64);
							Array<int64>& array = table.intArrays[linearMetadata.typeArrayIndex];
							// Round, in case value isn't exactly an integer.
							int64 intValue = int64(value + ((value >= 0) ? 0.5 : -0.5));
							array[currentIndex] = intValue;
						}
					}
				}
				if (hasRowSeries) {
					// FIXME: Handle initializing RowSeries when column major and missing data in some rows!!!

					const TableData::NamedData& rowMetadata = table.metadata[rowSeriesArrayIndex];
					uint32 numComponents = rowMetadata.numComponents;
					// Row component is always zero.
					size_t currentIndex = dataPointIndex*numComponents;
					if (rowMetadata.type == TableData::FLOAT64) {
						Array<double>& array = table.doubleArrays[rowMetadata.typeArrayIndex];
						array[currentIndex] = double(rowNumber);
					}
					else {
						assert(rowMetadata.type == TableData::INT64);
						Array<int64>& array = table.intArrays[rowMetadata.typeArrayIndex];
						array[currentIndex] = int64(rowNumber);
					}
				}
				if (hasColSeries) {
					// FIXME: Handle initializing ColSeries when column major and missing data in some rows!!!

					const TableData::NamedData& colMetadata = table.metadata[colSeriesArrayIndex];
					uint32 numComponents = colMetadata.numComponents;
					// Row component is always zero.
					size_t currentIndex = dataPointIndex*numComponents;
					if (colMetadata.type == TableData::FLOAT64) {
						Array<double>& array = table.doubleArrays[colMetadata.typeArrayIndex];
						array[currentIndex] = double(currentColSeries);
					}
					else {
						assert(colMetadata.type == TableData::INT64);
						Array<int64>& array = table.intArrays[colMetadata.typeArrayIndex];
						array[currentIndex] = int64(currentColSeries);
					}
				}
				if (columni != 0 && numIndependentCols != 0) {
					// FIXME: Handle initializing independent values when column major and missing data in some rows!!!

					// Copy the independent values to the current data point.
					for (size_t indepCol = 0; indepCol != numIndependentCols; ++indepCol) {
						size_t indepArrayIndex = columnArrayIndices[indepCol];
						uint32 indepComponent = columnComponents[indepCol];
						const TableData::NamedData& metadata = table.metadata[indepArrayIndex];
						if (indepComponent >= metadata.numComponents) {
							continue;
						}

						uint32 numComponents = metadata.numComponents;
						size_t currentIndex = dataPointIndex*numComponents + indepComponent;
						size_t sourceIndex = rowStartDataPointIndex*numComponents + indepComponent;
						TableData::DataType type = metadata.type;
						if (type == TableData::FLOAT64) {
							Array<double>& array = table.doubleArrays[metadata.typeArrayIndex];
							array[currentIndex] = array[sourceIndex];
						}
						else if (type == TableData::INT64) {
							Array<int64>& array = table.intArrays[metadata.typeArrayIndex];
							array[currentIndex] = array[sourceIndex];
						}
						else if (type == TableData::STRING) {
							Array<SharedString>& array = table.stringArrays[metadata.typeArrayIndex];
							array[currentIndex] = array[sourceIndex];
						}
					}
				}
				previousColSeries = currentColSeries;
			}

			// Back to handling the data associated with the current column.
			TableData::NamedData& metadata = table.metadata[columnArrayIndices[uniqueColumni]];

			const char* columnText = line + columns[columni][0];
			const char*const columnEnd = line + columns[columni][1];

			if (metadata.type == TableData::UNDETERMINED) {
				// Guess the data type based on the content of the first piece of data.
				metadata.type = guessTypeFromText(columnText, columnEnd);
				increaseArraySize(metadata, table);
			}

			uint32 component = columnComponents[uniqueColumni];
			uint32 numComponents = metadata.numComponents;
			if (component >= numComponents) {
				continue;
			}

			size_t currentIndex = dataPointIndex*numComponents + component;

			if (metadata.type == TableData::INT64) {
				int64 value;
				textToInteger(columnText, columnEnd, value);

				Array<int64>& array = table.intArrays[metadata.typeArrayIndex];
				array[currentIndex] = value;
			}
			else if (metadata.type == TableData::FLOAT64) {
				double value;
				textToDouble(columnText, columnEnd, value);

				Array<double>& array = table.doubleArrays[metadata.typeArrayIndex];
				array[currentIndex] = value;
			}
			else {//if (metadata.type == TableData::STRING) {
				// Only escape quoted strings if the text starts with a quote and spans the full text.
				ShallowString value;
				size_t textSize = columnEnd-columnText;
				bool isQuotedString = false;
				if (textSize >= 2 && (*columnText == '\"' || *columnText == '\'')) {
					char stopToken[2] = {*columnText, 0};
					size_t unescapedLength;
					size_t escapedLength = backslashEscapedStringLength(columnText+1, columnEnd, stopToken, &unescapedLength);
					if (escapedLength == textSize-2) {
						// Pre-allocate tempText.
						tempText.setSize(unescapedLength);
						tempText.setSize(0);
						// Don't need stopToken, since we already found it was at textEnd.
						unescapeBackslash(columnText+1, columnEnd-1, tempText);
						value = ShallowString(tempText.begin(), tempText.size());
						isQuotedString = true;
					}
				}
				if (!isQuotedString) {
					value = ShallowString(columnText, columnEnd-columnText);
				}

				Array<SharedString>& array = table.stringArrays[metadata.typeArrayIndex];
				array[currentIndex] = std::move(SharedString(value));
			}
		}

		// Move to the next row
		if (!options.rowMajor) {
			dataPointIndex = rowStartDataPointIndex;
		}
		++dataPointIndex;
	}

	return true;
}

bool ReadTableFile(const char* filename, const TableOptions& options, TableData& table) {
	Array<char> contents;
	bool success = ReadWholeFile(filename, contents);
	if (!success || contents.size() == 0) {
		return false;
	}
	return ReadTableText(contents.begin(), contents.end(), options, table);
}

} // namespace text
OUTER_NAMESPACE_END
