#pragma once

// This file contains declarations of functions for reading and writing
// arbitrary data from JSON files into a simple in-memory format.
// Definitions of the functions are in JSON.cpp

#include "../Types.h"
#include "../Array.h"

#include <memory>

OUTER_NAMESPACE_BEGIN
namespace json {

using namespace COMMON_LIBRARY_NAMESPACE;

enum class Type {
	OBJECT,
	ARRAY,
	STRING,
	NUMBER,
	SPECIAL,

	INT8ARRAY,
	INT16ARRAY,
	INT32ARRAY,
	INT64ARRAY,
	FLOAT32ARRAY,
	FLOAT64ARRAY
};

struct Value {
	Type type;

	Value(Type type_) : type(type_) {}

	~Value() {
		clear();
	}
	void clear();
};

struct StringValue : public Value {
	Array<char> text;

	StringValue() : Value(Type::STRING) {}

	~StringValue() {
		clear();
		// Avoid double-clearing in Value destructor.
		// Type::SPECIAL requires no clearing.
		type = Type::SPECIAL;
	}
	void clear() {
		text.setCapacity(0);
	}
};

struct ObjectValue : public Value {
	Array<StringValue> names;
	Array<std::unique_ptr<Value>> values;

	ObjectValue() : Value(Type::OBJECT) {}

	~ObjectValue() {
		clear();
		// Avoid double-clearing in Value destructor.
		// Type::SPECIAL requires no clearing.
		type = Type::SPECIAL;
	}
	void clear() {
		names.setCapacity(0);
		values.setCapacity(0);
	}
};

// Type::ARRAY has T of std::unique_ptr<Value>
template<typename T>
struct ArrayValue : public Value {
	Array<T> values;

	ArrayValue() : Value(arrayType()) {}

	constexpr static Type arrayType() {
		if (std::is_same<T,int8>::value) {
			return Type::INT8ARRAY;
		}
		if (std::is_same<T,int16>::value) {
			return Type::INT16ARRAY;
		}
		if (std::is_same<T,int32>::value) {
			return Type::INT32ARRAY;
		}
		if (std::is_same<T,int64>::value) {
			return Type::INT64ARRAY;
		}
		if (std::is_same<T,float>::value) {
			return Type::FLOAT32ARRAY;
		}
		if (std::is_same<T,double>::value) {
			return Type::FLOAT64ARRAY;
		}
		static_assert(std::is_same<T,std::unique_ptr<Value>>::value);
		return Type::ARRAY;
	}

	~ArrayValue() {
		clear();
		// Avoid double-clearing in Value destructor.
		// Type::SPECIAL requires no clearing.
		type = Type::SPECIAL;
	}
	void clear() {
		values.setCapacity(0);
	}
};
using ArrayValueAny = ArrayValue<std::unique_ptr<Value>>;

struct NumberValue : public Value {
	Array<char> text;
	double value;

	NumberValue() : Value(Type::NUMBER) {}

	~NumberValue() {
		clear();
		// Avoid double-clearing in Value destructor.
		// Type::SPECIAL requires no clearing.
		type = Type::SPECIAL;
	}
	void clear() {
		text.setCapacity(0);
		value = 0.0;
	}
};

// NOTE: The trailing underscores are because FALSE, TRUE, and NULL are frequently macros in C/C++.
enum class Special {
	FALSE_,
	TRUE_,
	NULL_
};

struct SpecialValue : public Value {
	Special value;

	SpecialValue(Special value_) : Value(Type::SPECIAL), value(value_) {}

	// This clear() is just here for completeness.
	void clear() {
		value = Special::FALSE_;
	}
};

inline void Value::clear() {
	// Switch on type to call the subclass clear() function.
	switch (type) {
		default:
		case Type::SPECIAL: {
			// Nothing to destruct.
			// NOTE: This is used by subclasses to avoid redundant clearing on destruction.
			return;
		}

		case Type::OBJECT: {
			static_cast<ObjectValue*>(this)->clear();
			return;
		}
		case Type::ARRAY: {
			static_cast<ArrayValueAny*>(this)->clear();
			return;
		}
		case Type::STRING: {
			static_cast<StringValue*>(this)->clear();
			return;
		}
		case Type::NUMBER: {
			static_cast<NumberValue*>(this)->clear();
			return;
		}

		case Type::INT8ARRAY: {
			static_cast<ArrayValue<int8>*>(this)->clear();
			return;
		}
		case Type::INT16ARRAY: {
			static_cast<ArrayValue<int16>*>(this)->clear();
			return;
		}
		case Type::INT32ARRAY: {
			static_cast<ArrayValue<int32>*>(this)->clear();
			return;
		}
		case Type::INT64ARRAY: {
			static_cast<ArrayValue<int64>*>(this)->clear();
			return;
		}
		case Type::FLOAT32ARRAY: {
			static_assert(sizeof(float) == 4);
			static_cast<ArrayValue<float>*>(this)->clear();
			return;
		}
		case Type::FLOAT64ARRAY: {
			static_assert(sizeof(double) == 8);
			static_cast<ArrayValue<double>*>(this)->clear();
			return;
		}
	}
}

COMMON_LIBRARY_EXPORTED std::unique_ptr<Value> ReadJSONFile(const char* filename);

COMMON_LIBRARY_EXPORTED bool WriteJSONFile(const char* filename, const Value& contents, bool binary = false);

} // namespace json
OUTER_NAMESPACE_END
