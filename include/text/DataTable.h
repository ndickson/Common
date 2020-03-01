#pragma once

// This file contains definitions of functions and structures for
// reading data from tables of text.

#include "../Types.h"
#include "../Array.h"
#include "../SharedString.h"

#include <memory>
#include <functional>

OUTER_NAMESPACE_BEGIN
namespace text {

using namespace COMMON_LIBRARY_NAMESPACE;

struct TableData {
	enum DataType {
		UNDETERMINED = -1,
		INT64,
		FLOAT64,
		STRING
	};
	struct NamedData {
		SharedString name;
		DataType type;
		uint32 numComponents;
		size_t typeArrayIndex;

		constexpr static size_t INVALID_INDEX = ~size_t(0);
	};

	size_t numDataPoints = 0;
	
	Array<NamedData> metadata;
	Array<Array<int64>> intArrays;
	Array<Array<double>> doubleArrays;
	Array<Array<SharedString>> stringArrays;

	Array<size_t> majorSeriesStarts;
};

struct TableOptions {
	// If FIRST_COLUMNS and numIndependentCols > 0, there will be "independent" data in the first
	// column(s) of the table and the names will be in dataNames or in the first line of the table.
	// If UNIFORM and independentLinearName is non-null, there will be an independent data array
	// created with linearly increasing values for each data row.
	//
	// Independent data are data that apply to potentially multiple data points in
	// the same row, e.g. time, if there are multiple time series in different columns.
	// Independent data values are copied for each data point in a row,
	// so this may not be desirable if you have each time series under a different
	// data name.
	enum IndependentType {
		FIRST_COLUMNS,
		LINEAR
	};
	IndependentType independentType = FIRST_COLUMNS;

	size_t numIndependentCols = 0;

	const char* independentLinearName = nullptr;
	double independentLinearBegin = 0.0;
	double independentLinearIncrement = 1.0;

	// If non-null, this specifies the names of the data in the table,
	// (both "independent" and "dependent").
	// If null, the first line of the table specifies the names.
	const char* dataNames = nullptr;

	// Any new arrays will be given this type.  If UNDETERMINED,
	// the type will be determined based on the content of the
	// text for the first value associated with each new array.
	TableData::DataType defaultType = TableData::UNDETERMINED;

	// If true, different sets of dependent columns in a single row will be stored
	// contiguously in the arrays.  If false, all of the data of one column will be
	// stored before the data for the next column corresponding with the same array.
	bool rowMajor = true;

	// If true, an integer array named "RowSeries" is created,
	// where each row containing data corresponds with a new row series number.
	bool createRowSeries = false;

	// If true, an integer array named "ColSeries" is created,
	// where each set of dependent columns corresponds with a new column series number.
	bool createColSeries = false;

	const char* columnSeparators = " \t,;";
	bool treatMultipleSeparatorsAsOne = true;
	const size_t* columnWidths = nullptr;
	size_t numColumnWidths = 0;

	// If provided, this is a functor for preprocessing each line before anything else.
	// The functor should return true if the line is to be kept in some form,
	// or false if the line is to be fully discarded.
	// If the line is to be kept but changed, newText should be filled with the
	// new text of the line, else it should be cleared.
	std::function<bool(const char* line, const char* lineEnd, Array<char>& newText)> preprocessFunction;
};

COMMON_LIBRARY_EXPORTED bool ReadTableText(const char* text, const char*const textEnd, const TableOptions& options, TableData& table);

COMMON_LIBRARY_EXPORTED bool ReadTableFile(const char* filename, const TableOptions& options, TableData& table);


} // namespace text
OUTER_NAMESPACE_END
