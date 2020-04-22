// This file contains definitions of functions for parsing text
// representing mathematical expressions into a simple in-memory format.
// Declarations of the functions are in Expression.h

#include "math/Expression.h"
#include "text/EscapeText.h"
#include "text/NumberText.h"
#include "text/TextFunctions.h"
#include "../Types.h"
#include "../Array.h"
#include "../ArrayDef.h"

OUTER_NAMESPACE_BEGIN
namespace math {

static Precedence precedences[] = {
	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,

	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,

	Precedence::BRACKET,
	Precedence::BRACKET,
	Precedence::BRACKET,
	Precedence::BRACKET,

	Precedence::SEMICOLON,

	Precedence::COLON,

	Precedence::COLON,

	Precedence::COMMA,

	Precedence::ERRORBAR,

	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,
	Precedence::ASSIGNMENT,

	Precedence::LOGICAL_OR,

	Precedence::LOGICAL_AND,

	Precedence::BITWISE_OR,

	Precedence::BITWISE_XOR,

	Precedence::BITWISE_AND,

	Precedence::EQUALITY,
	Precedence::EQUALITY,

	Precedence::ORDER,
	Precedence::ORDER,
	Precedence::ORDER,
	Precedence::ORDER,

	Precedence::THREE_WAY_COMPARE,

	Precedence::SHIFT,
	Precedence::SHIFT,

	Precedence::ADDITION,
	Precedence::ADDITION,

	Precedence::IMPLICIT_OPERATOR,

	Precedence::MULTIPLICATION,
	Precedence::MULTIPLICATION,
	Precedence::MULTIPLICATION,

	Precedence::POWER,

	Precedence::MEMBER_POINTER_DEREF,
	Precedence::MEMBER_POINTER_DEREF,

	Precedence::PREFIX,
	Precedence::PREFIX,
	Precedence::PREFIX,
	Precedence::PREFIX,
	Precedence::PREFIX,
	Precedence::PREFIX,
	Precedence::PREFIX,
	Precedence::PREFIX,

	Precedence::PREFIX,

	Precedence::POSTFIX,
	Precedence::POSTFIX,
	Precedence::POSTFIX,
	Precedence::POSTFIX,
	Precedence::POSTFIX,

	Precedence::POSTFIX,
	Precedence::POSTFIX,
	Precedence::POSTFIX,

	Precedence::SCOPE
};

static void popOperatorStack(
	Precedence precedence,
	Array<Item>& output,
	Array<Item>& operatorStack,
	bool isRightToLeft
) {
	while (operatorStack.size() != 0) {
		ItemType stackOp = operatorStack.last().type;
		Precedence stackPrecedence = precedences[size_t(stackOp)];
		if (size_t(precedence) <= size_t(stackPrecedence)-isRightToLeft) {
			if (operatorStack.last().type == ItemType::TERNARY_PARTIAL) {
				// FIXME: This is invalid, so it may be worth producing an error.
			}
			output.append(operatorStack.last());
			operatorStack.removeLast();
			// FIXME: Handle functions!!!
		}
		else {
			break;
		}
	}
}

static bool appendBinaryOperator(
	ItemType op,
	Array<Item>& output,
	Array<Item>& operatorStack,
	bool& expectingUnary,
	bool isRightToLeft,
	const char* begin,
	const char* end
) {
	if (expectingUnary) {
		// Unexpected binary operator
		return false;
	}
	popOperatorStack(precedences[size_t(op)], output, operatorStack, isRightToLeft);
	operatorStack.append(Item{op, 2, begin, end});
	expectingUnary = true;
	return true;
}

bool parseExpression(const char* begin, const char* end, Array<Item>& output) {
	const char* text = begin;

	Array<Item> operatorStack;

	bool expectingUnary = true;

	while (true) {
		text = text::skipWhitespace(text, end);
		if (text == end || *text == 0) {
			break;
		}

		const char* tokenStart = text;

		bool unexpectedUnary = false;
		bool unexpectedBinary = false;

		char c = *text;
		++text;

		if (c >= '0' && c <= '9') {
			// Integer or floating-point literal

			const char* numberBegin = text-1;
			// First, handle hex literals.
			bool isInteger = true;
			bool isHex = false;
			if (text != end && *text != 0 && c == '0') {
				char c1 = *text;
				if ((c1|0x20) == 'x') {
					isHex = true;
					++text;
					numberBegin = text;
					if (text == end || *text == 0) {
						// Unexpected end
						return false;
					}
					c = *text;
				}
			}
			// Skip initial digits.
			while (text != end) {
				char c1 = *text;
				if ((c1 < '0' || c1 > '9') && (!isHex || ((c1|0x20) < 'a' || (c1|0x20) > 'f'))) {
					break;
				}
				++text;
			}
			if (text != end && (*text == '.')) {
				isInteger = false;
				++text;
				// Skip digits after dot.
				while (text != end) {
					char c1 = *text;
					if ((c1 < '0' || c1 > '9') && (!isHex || ((c1|0x20) < 'a' || (c1|0x20) > 'f'))) {
						break;
					}
					++text;
				}
			}
			if (text != end && ((*text | 0x20) == (isHex ? 'p' : 'e'))) {
				isInteger = false;
				++text;
				if (text == end || *text == 0) {
					// Unexpected end
					return false;
				}
				// Skip optional "+" or "-" in exponent.
				if ((*text == '+') || (*text == '-')) {
					++text;
				}
				// Skip exponent digits.
				bool hasExponent = false;
				while (text != end) {
					char c1 = *text;
					if (c1 < '0' || c1 > '9') {
						break;
					}
					hasExponent = true;
					++text;
				}
				if (!hasExponent) {
					// There must be an exponent after p or e.
					return false;
				}
			}

			ItemType type = isInteger ? ItemType::INTEGER_LITERAL : ItemType::FLOAT_LITERAL;

			// FIXME: Handle suffixes!!! (f, F, l, or L for float; u, ul, ull, or case changes thereof for integer)

			if (text != end) {
				char c = *text;
				if (((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c == '_' || (c >= '0' && c <= '9')) {
					// Alphanumeric characters can't immediately follow a numeric literal.
					return false;
				}
			}

			if (!expectingUnary) {
				// Implicit binary operator
				ItemType implicitType = ItemType::IMPLICIT_OPERATOR;
				appendBinaryOperator(implicitType, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			output.append(Item{type, 0, tokenStart, text});
			expectingUnary = false;
		}
		else if (((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c == '_') {
			// Keyword, type, function, variable, or new identifier.

			// Skip over all alphanumeric characters.
			while (text != end && *text != 0) {
				c = *text;
				if (((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c == '_' || (c >= '0' && c <= '9')) {
					++text;
				}
				else {
					break;
				}
			}

			// FIXME: Check the type!!!
			ItemType type = ItemType::IDENTIFIER;

			if (!expectingUnary) {
				// Implicit binary operator
				ItemType implicitType = ItemType::IMPLICIT_OPERATOR;
				appendBinaryOperator(implicitType, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			output.append(Item{type, 0, tokenStart, text});
			expectingUnary = false;
		}
		else if (c == '(' || c == '[' || c == '{') {
			ItemType type = (c == '(') ? ItemType::PARENTHESES :
				((c == '[') ? ItemType::SQUARE_BRACKETS : ItemType::CURLY_BRACES);
			if (!expectingUnary) {
				// Implicit binary operator, with higher precedence
				ItemType implicitType = (c == '(') ? ItemType::FUNCTION_CALL :
					((c == '[') ? ItemType::SUBSCRIPT : ItemType::BRACE_INITIALIZER);
				appendBinaryOperator(implicitType, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			operatorStack.append(Item{type,1,tokenStart,text});
			assert(expectingUnary);
		}
		else if (c == ')' || c == ']' || c == '}') {
			while (operatorStack.size() != 0) {
				ItemType stackOp = operatorStack.last().type;
				Precedence stackPrecedence = precedences[size_t(stackOp)];
				if (stackPrecedence <= Precedence::BRACKET) {
					break;
				}
				output.append(operatorStack.last());
				operatorStack.removeLast();
				// FIXME: Handle functions!!!
			}

			if (operatorStack.size() == 0) {
				// Missing start bracket
				return false;
			}
			ItemType type = (c == ')') ? ItemType::PARENTHESES :
				((c == ']') ? ItemType::SQUARE_BRACKETS : ItemType::CURLY_BRACES);
			ItemType stackOp = operatorStack.last().type;
			if (type != stackOp) {
				// Mismatched brackets
				return false;
			}
			output.append(operatorStack.last());
			operatorStack.removeLast();
		}
		else if (c == ';' || c == ',') {
			// FIXME: Implement this!!!





		}
		else if (c == '=') {
			if (text == end || *text == 0) {
				// Unexpected end
				return false;
			}
			char c1 = *text;
			ItemType type = ItemType::ASSIGN;
			if (c1 == '=') {
				++text;
				type = ItemType::EQUAL;
			}

			const bool isRightToLeft = (type == ItemType::EQUAL);
			unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, isRightToLeft, tokenStart, text);
		}
		else if (c == '+' || c == '-') {
			if (text == end || *text == 0) {
				// Unexpected end
				return false;
			}
			char c1 = *text;
			if (c1 == c) {
				// Increment "++" or decrement "--"
				++text;
				if (expectingUnary) {
					// Prefix
					ItemType type = (c == '+') ? ItemType::PREFIX_INCREMENT : ItemType::PREFIX_DECREMENT;

					// FIXME: Handle if this is at the start of a first function parameter!!!

					operatorStack.append(Item{type,1,tokenStart,text});
				}
				else {
					// Postfix
					ItemType type = (c == '+') ? ItemType::POSTFIX_INCREMENT : ItemType::POSTFIX_DECREMENT;
					Precedence precedence = Precedence::POSTFIX;

					// Postfix never needs to go on the operator stack

					// FIXME: Implement this!!!


				}
				// expectingUnary remains unchanged.
			}
			else if (c1 == '=') {
				// Assign-add "+=" or assign-subtract "-="
				++text;
				ItemType type = (c == '+') ? ItemType::ASSIGN_ADD : ItemType::ASSIGN_SUBTRACT;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, true, tokenStart, text);
			}
			else if (c == '-' && c1 == '>') {
				// Arrow "->" or arrow-star "->*"
				++text;
				if (text == end || *text == 0) {
					// Unexpected end
					return false;
				}
				char c2 = *text;
				ItemType type = ItemType::ARROW;
				if (c2 == '*') {
					++text;
					type = ItemType::ARROW_STAR;
				}

				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			else if (c == '+' && c1 == '/') {
				// Possibly +/-
				++text;
				if (text == end || *text == 0) {
					// Unexpected end
					return false;
				}
				char c2 = *text;
				if (c2 != '-') {
					// Unexpected character
					return false;
				}
				++text;
				unexpectedBinary = !appendBinaryOperator(ItemType::ERRORBAR, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			else if (expectingUnary) {
				// Unary plus or minus
				ItemType type = (c == '+') ? ItemType::PREFIX_PLUS : ItemType::PREFIX_MINUS;
				// Increment and decrement are handled above.
				assert(c != c1);

				// FIXME: Handle if this is at the start of a first function parameter!!!

				operatorStack.append(Item{type,1,tokenStart,text});
			}
			else {
				// Regular add or subtract
				ItemType type = (c == '+') ? ItemType::ADD : ItemType::SUBTRACT;
				appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
		}
		else if (c == '*') {
			if (text == end || *text == 0) {
				// Unexpected end
				return false;
			}
			char c1 = *text;
			if (expectingUnary) {
				// Unary star
				ItemType type = ItemType::PREFIX_STAR;

				// FIXME: Handle if this is at the start of a first function parameter!!!

				operatorStack.append(Item{type,1,tokenStart,text});
			}
			else if (c1 == '=') {
				// Assign-multiply "*="
				++text;
				ItemType type = ItemType::ASSIGN_MULTIPLY;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, true, tokenStart, text);
			}
			else if (c1 == '*') {
				// Power "**"
				++text;
				ItemType type = ItemType::POWER;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			else {
				// Regular multiply "*"
				ItemType type = ItemType::MULTIPLY;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
		}
		else if (c == '/') {
			if (text == end || *text == 0) {
				// Unexpected end
				return false;
			}
			char c1 = *text;
			if (c1 == '=') {
				// Assign-divide "/="
				++text;
				ItemType type = ItemType::ASSIGN_DIVIDE;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, true, tokenStart, text);
			}
			else if (c1 == '/') {
				// Comment "//"

				// FIXME: Implement this!!!


			}
			else if (c1 == '*') {
				// Comment "/*"

				// FIXME: Implement this!!!

			}
			else {
				// Regular divide "/"
				ItemType type = ItemType::DIVIDE;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
		}
		else if (c == '\\') {
			if (text == end || *text == 0) {
				// Unexpected end
				return false;
			}
			char c1 = *text;
			if (c1 == '\\') {
				// Left divide "\\"
				++text;
				ItemType type = ItemType::LEFT_DIVIDE;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			else {
				// Line continuation

				// FIXME: Implement this!!!
			}
		}
		else if (c == '%') {
			if (text == end || *text == 0) {
				// Unexpected end
				return false;
			}
			char c1 = *text;
			if (c1 == '=') {
				// Assign-modulus "%="
				++text;
				ItemType type = ItemType::ASSIGN_MODULUS;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, true, tokenStart, text);
			}
			else {
				// Regular modulus "%"
				ItemType type = ItemType::MODULUS;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
		}
		else if (c == '&' || c == '^' || c == '|') {
			if (expectingUnary) {
				if (c == '&') {
					// Unary ampersand
					ItemType type = ItemType::PREFIX_AMPERSAND;

					// FIXME: Handle if this is at the start of a first function parameter!!!

					operatorStack.append(Item{type,1,tokenStart,text});
				}
				else {
					// "^", "|", "^=", "|=", and "||" can only be binary operators.
					unexpectedBinary = true;
				}
			}
			else {
				if (text == end || *text == 0) {
					// Unexpected end
					return false;
				}
				char c1 = *text;
				if (c1 == '=') {
					// Assign-and "&=", assign-xor "^=", or assign-or "|="
					++text;
					ItemType type = (c == '&') ? ItemType::ASSIGN_AND
						: ((c == '^') ? ItemType::ASSIGN_XOR : ItemType::ASSIGN_OR);
					unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, true, tokenStart, text);
				}
				else if (c1 == c && c != '^') {
					// Logical and "&&" or logical or "||"
					++text;
					ItemType type = (c == '&') ? ItemType::LOGICAL_AND : ItemType::LOGICAL_OR;
					unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
				}
				else {
					// Bitwise and "&", bitwise xor "^", or bitwise or "|"
					ItemType type = (c == '&') ? ItemType::BITWISE_AND
						: ((c == '^') ? ItemType::BITWISE_XOR : ItemType::BITWISE_OR);
					unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
				}
			}
		}
		else if (c == '<' || c == '>') {
			if (text == end || *text == 0) {
				// Unexpected end
				return false;
			}
			char c1 = *text;
			ItemType type = (c == '<') ? ItemType::LESS : ItemType::GREATER;
			if (c1 == '=') {
				++text;
				type = (c == '<') ? ItemType::LESS_OR_EQUAL : ItemType::GREATER_OR_EQUAL;
			}

			// FIXME: Handle angle brackets case!!!

			unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
		}
		else if (c == '?') {
			ItemType type = ItemType::TERNARY;
			unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, true, tokenStart, text);
			if (!unexpectedBinary) {
				// The precedence needs to be lower for the second argument, so that a comma between
				// "?" and ":" will be treated as "condition ? (a, b) : c", instead of "((condition ? a), b) : c",
				// since "condition ? a" isn't a complete expression.
				assert(operatorStack.last().type == ItemType::TERNARY);
				operatorStack.last().type = ItemType::TERNARY_PARTIAL;
			}
		}
		else if (c == ':') {
			if (expectingUnary) {
				unexpectedBinary = true;
			}
			else {
				while (operatorStack.size() != 0) {
					Item& item = operatorStack.last();
					if (item.type == ItemType::TERNARY_PARTIAL) {
						item.type = ItemType::TERNARY;
						++item.numParams;
						break;
					}
					else {
						Precedence precedence = precedences[size_t(item.type)];
						if (precedence < Precedence::COLON) {
							ItemType type = ItemType::COLON;
							output.append(Item{type,2,tokenStart,text});
							break;
						}
						output.append(item);
						operatorStack.removeLast();
					}
				}
			}
		}
		else if (c == '!') {
			if (text == end || *text == 0) {
				// Unexpected end
				return false;
			}
			char c1 = *text;
			if (c1 == '=') {
				++text;
				ItemType type = ItemType::NOT_EQUAL;
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			else {
				// Logical not "!"
				ItemType type = ItemType::PREFIX_LOGICAL_NOT;
				if (!expectingUnary) {
					// "!" can only be a unary operator.
					// Implicit binary operator
					ItemType implicitType = ItemType::IMPLICIT_OPERATOR;
					appendBinaryOperator(implicitType, output, operatorStack, expectingUnary, false, tokenStart, text);
				}
				output.append(Item{type, 1, tokenStart, text});
				assert(expectingUnary);
			}
		}
		else if (c == '~') {
			// Bitwise not "~"
			ItemType type = ItemType::PREFIX_BITWISE_NOT;
			if (!expectingUnary) {
				// "~" can only be a unary operator.
				// Implicit binary operator
				ItemType implicitType = ItemType::IMPLICIT_OPERATOR;
				appendBinaryOperator(implicitType, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			output.append(Item{type, 1, tokenStart, text});
			assert(expectingUnary);
		}
		else if (c == '\"' || c == '\'') {
			// Quoted string
			const char stopToken[2] = {c, 0};
			size_t stringEscapedLength = text::backslashEscapedStringLength(text, end, stopToken);
			if (text + stringEscapedLength == end) {
				// Unexpected end
				return false;
			}
			text += stringEscapedLength+1;

			ItemType type = ItemType::STRING_LITERAL;

			if (!expectingUnary) {
				// Implicit binary operator
				ItemType implicitType = ItemType::IMPLICIT_OPERATOR;
				appendBinaryOperator(implicitType, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
			output.append(Item{type, 0, tokenStart, text});
			expectingUnary = false;
		}
		else if (c == '.') {
			if (text == end || *text == 0) {
				// Unexpected end
				return false;
			}
			char c1 = *text;
			if (c1 >= '0' && c1 <= '9') {
				// Floating-point literal
				// FIXME: Implement a fast function to find the end of a floating-point literal in text!!!
				double value;
				size_t numberTextLength = text::textToDouble(text-1, end, value);
				text += numberTextLength-1;

				// FIXME: Handle suffixes!!!

				ItemType type = ItemType::FLOAT_LITERAL;

				if (!expectingUnary) {
					// Implicit binary operator
					ItemType implicitType = ItemType::IMPLICIT_OPERATOR;
					appendBinaryOperator(implicitType, output, operatorStack, expectingUnary, false, tokenStart, text);
				}
				output.append(Item{type, 0, tokenStart, text});
				expectingUnary = false;
			}
			else {
				ItemType type = ItemType::DOT;
				if (c1 == '*') {
					++text;
					type = ItemType::DOT_STAR;
				}
				unexpectedBinary = !appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
		}

		if (unexpectedUnary) {
			// Unexpected unary operator
			return false;
		}
		if (unexpectedBinary) {
			// Unexpected binary operator
			return false;
		}

	}

	// FIXME: Update operatorStack and output!!!

	return true;
}


} // namespace math
OUTER_NAMESPACE_END
