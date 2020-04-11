#pragma once

// This file contains declarations of functions for parsing text
// representing mathematical expressions into a simple in-memory format.
// Definitions of the functions are in Expression.cpp

#include "../Types.h"

OUTER_NAMESPACE_BEGIN
namespace math {

using namespace COMMON_LIBRARY_NAMESPACE;

enum class ItemType {
	KEYWORD,
	TYPE,
	FUNCTION,
	VARIABLE,
	IDENTIFIER,

	INTEGER_LITERAL,
	FLOAT_LITERAL,
	IMAGINARY_LITERAL,
	STRING_LITERAL,
	CHARACTER_LITERAL,

	// While parsing: (param)
	// Not kept after parsing.
	// Replaced with FUNCTION_CALL: param0(param1,param2,param3,param4)
	// Replaced with TYPE_CAST: (param0)param1
	PARENTHESES,
	// While parsing: [param]
	// After parsing: param0[param1]
	SQUARE_BARCKETS,
	// While parsing: {param}
	// After parsing: {param0,param1,param2,param3} since all values should be kept
	// if was COMMA inside.
	// Replaced with FUNCTION_CALL: param0{param1,param2,param3,param4}
	// Replaced with TYPE_CAST: param0{param1} if param0 is a TYPE
	CURLY_BRACES,
	// <param>
	ANGLE_BRACKETS,

	// param0; param1; param2; param3;
	SEMICOLON,

	// param0: param1
	COLON,

	// param0, param1, param2, param3
	// If FUNCTION(COMMA), replaced with FUNCTION_CALL: param0(param1,param2,param3,param4)
	COMMA,

	// param0 +/- param1
	ERRORBAR,

	// param0 = param1
	ASSIGN,
	// param0 ? param1 : param2
	TERNARY,
	// param0 += param1
	ASSIGN_ADD,
	// param0 -= param1
	ASSIGN_SUBTRACT,
	// param0 *= param1
	ASSIGN_MULTIPLY,
	// param0 /= param1
	ASSIGN_DIVIDE,
	// param0 *= param1
	ASSIGN_MODULUS,
	// param0 <<= param1
	ASSIGN_SHIFT_LEFT,
	// param0 >>= param1
	ASSIGN_SHIFT_RIGHT,
	// param0 &= param1
	ASSIGN_AND,
	// param0 ^= param1
	ASSIGN_XOR,
	// param0 |= param1
	ASSIGN_OR,

	// param0 || param1
	LOGICAL_OR,

	// param0 && param1
	LOGICAL_AND,

	// param0 | param1
	BITWISE_OR,

	// param0 ^ param1
	BITWISE_XOR,

	// param0 & param1
	BITWISE_AND,

	// param0 == param1
	EQUAL,
	// param0 != param1
	NOT_EQUAL,

	// param0 < param1
	LESS,
	// param0 > param1
	GREATER,
	// param0 <= param1
	LESS_OR_EQUAL,
	// param0 >= param1
	GREATER_OR_EQUAL,

	// param0 <=> param1
	THREE_WAY_COMPARE,

	// param0 << param1
	SHIFT_LEFT,
	// param0 >> param1
	SHIFT_RIGHT,

	// param0 + param1
	ADD,
	// param0 - param1
	SUBTRACT,

	// param0 param1
	IMPLICIT_OPERATOR,

	// param0 * param1
	MULTIPLY,
	// param0 / param1
	DIVIDE,
	// param0 % param1
	MODULUS,

	// param0 ** param1
	POWER,

	// param0.*param1
	DOT_STAR,
	// param0->*param1
	ARROW_STAR,

	// &param0
	PREFIX_AMPERSAND,
	// *param0
	PREFIX_STAR,
	// +param0
	PREFIX_PLUS,
	// -param0
	PREFIX_MINUS,
	// !param0
	PREFIX_LOGICAL_NOT,
	// ~param0
	PREFIX_BITWISE_NOT,
	// ++param0
	PREFIX_INCREMENT,
	// --param0
	PREFIX_DECREMENT,

	// param0.param1
	DOT,
	// param0->param1
	ARROW,
	// param0'
	COMPLEX_CONJUGATE,

	// If param0 is a TYPE:
	// (param0)param1 has prefix precedence
	// param0(param1) has postfix precedence, like a function call
	// param0{param1} has postfix precedence, like a function call
	TYPE_CAST,

	// param0::param1
	SCOPE
};

struct Item {
	ItemType type;
	size_t numParams;
	const char* text;
	const char* end;
};

COMMON_LIBRARY_EXPORTED bool parseExpression(const char* begin, const char* end, Array<Item>& output);

} // namespace math
OUTER_NAMESPACE_END
