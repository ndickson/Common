// This file contains definitions of functions for parsing text
// representing mathematical expressions into a simple in-memory format.
// Declarations of the functions are in Expression.h

#include "math/Expression.h"
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

	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,
	Precedence::NOT_APPLICABLE,

	Precedence::SEMICOLON,

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

		}
		else if (((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c == '_') {


		}
		else if (c == '(' || c == '[' || c == '{') {

		}
		else if (c == ')' || c == ']' || c == '}') {

		}
		else if (c == ';' || c == ',') {

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
				++text;
				if (text == end || *text == 0) {
					// Unexpected end
					return false;
				}

				ItemType type = (c == '+') ? ItemType::PREFIX_PLUS : ItemType::PREFIX_MINUS;

				// Increment and decrement are handled above.
				assert(*text != c);

				// FIXME: Handle if this is at the start of a first function parameter!!!

				operatorStack.append(Item{type,1,tokenStart,text});
			}
			else {
				// Regular add or subtract
				ItemType type = (c == '+') ? ItemType::ADD : ItemType::SUBTRACT;
				appendBinaryOperator(type, output, operatorStack, expectingUnary, false, tokenStart, text);
			}
		}
		else if (c == '*' || c == '/' || c == '%') {

		}
		else if (c == '&' || c == '^' || c == '|') {

		}
		else if (c == '<' || c == '>') {

		}
		else if (c == '?') {

		}
		else if (c == ':') {

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
			else if (expectingUnary) {
				ItemType type = ItemType::PREFIX_LOGICAL_NOT;

				// FIXME: Handle if this is at the start of a first function parameter!!!

				operatorStack.append(Item{type,1,tokenStart,text});
			}
			else {
				// "!" can only be a unary operator.
				unexpectedUnary = true;
			}
		}
		else if (c == '~') {

		}
		else if (c == '\"' || c == '\'') {

		}
		else if (c == '.') {

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


}


} // namespace math
OUTER_NAMESPACE_END
