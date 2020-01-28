// This file contains definitions of functions for parsing and generating
// text strings representing numbers.

#include "text/NumberText.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Bits.h"
#include "Types.h"
#include <limits>
#include <cmath>

OUTER_NAMESPACE_BEGIN
namespace text {

using namespace Common;

static size_t textToDoubleWithPrecision(const char* text, const char*const end, size_t bits, double& value) {
	if (text == end) {
		return 0;
	}

	char c = *text;
	const char* currentText = text;

	// Handle optional sign character.
	const bool negative = (c == '-');
	if (c == '+' || c == '-') {
		// Move past the sign character.
		++currentText;
		if (currentText == end) {
			// A number can't be just a sign character.
			return 0;
		}
		c = *currentText;
	}

	// Handle "inf", "infinity", and "nan" in any capitalization.
	char cLower = c | char(0x20);
	if (cLower == 'i') {
		if ((end - currentText) >= 3 &&
			(currentText[1] | char(0x20)) == 'n' &&
			(currentText[2] | char(0x20)) == 'f'
		) {
			value = std::numeric_limits<double>::infinity();
			if (negative) {
				value = -value;
			}
			if ((end - currentText) >= 8 &&
				(currentText[3] | char(0x20)) == 'i' &&
				(currentText[4] | char(0x20)) == 'n' &&
				(currentText[5] | char(0x20)) == 'i' &&
				(currentText[6] | char(0x20)) == 't' &&
				(currentText[7] | char(0x20)) == 'y'
			) {
				// Full "infinity"
				return (currentText + 8) - text;
			}
			// Just "inf"
			return (currentText + 3) - text;
		}
	}
	else if (cLower == 'n') {
		if ((end - currentText) >= 3 &&
			(currentText[1] | char(0x20)) == 'a' &&
			(currentText[2] | char(0x20)) == 'n'
		) {
			value = std::numeric_limits<double>::quiet_NaN();
			if (negative) {
				// -value may or may not change the sign of the NaN,
				// so use copysign, which is required to support NaN.
				value = std::copysign(value, -1.0);
			}
			return (currentText + 3) - text;
		}
	}

	// After the optional sign character, the number must start with
	// a digit character or decimal point.
	if (!isDigit<10>(c) && c != '.') {
		return 0;
	}

	// Find the bounds of the integer part in the string, if present.
	const char* integerPartBegin = (c == '.') ? nullptr : currentText;
	while (isDigit<10>(c)) {
		++currentText;
		if (currentText == end) {
			c = 0;
			break;
		}
		c = *currentText;
	}
	const char* integerPartEnd = (integerPartBegin == nullptr) ? nullptr : currentText;

	// Find the bounds of the fractional part in the string, if present.
	const char* fractionalPartBegin = nullptr;
	const char* fractionalPartEnd = nullptr;
	if (c == '.') {
		++currentText;
		if (currentText != end) {
			c = *currentText;
			if (isDigit<10>(c)) {
				fractionalPartBegin = currentText;
				do {
					++currentText;
					if (currentText == end) {
						c = 0;
						break;
					}
					c = *currentText;
				} while (isDigit<10>(c));
				fractionalPartEnd = currentText;
			}
			else if (integerPartBegin == nullptr) {
				// No integer or fractional part, so not a valid number.
				return 0;
			}
		}
	}

	// Find the exponent in the string, if present.
	bool exponentNegative = false;
	int64 exponent = 0;
	if ((c | char(0x20)) == 'e') {
		// Don't iterate using currentText here, since we
		// don't include this as part of the number if it's not valid.
		const char* exponentText = currentText+1;
		if (exponentText != end) {
			c = *exponentText;
			if (c == '+' || c == '-') {
				exponentNegative = (c == '-');
				++exponentText;
				if (exponentText != end) {
					c = *exponentText;
				}
			}
			if (isDigit<10>(c)) {
				// We have a valid exponent, so can use currentText again.
				currentText = exponentText;

				do {
					// If exponent is this large, it would take an
					// impossibly long integer or fractional part
					// to counteract its effect to keep the double in bounds.
					// Ignore digits after this to prevent integer overflow.
					constexpr int64 inconceivableExponent = (int64(1)<<59);
					if (exponent < inconceivableExponent) {
						int64 digit;
						digitToInteger(c, digit);
						exponent = (exponent*10) + digit;
					}
					++currentText;
					if (currentText == end) {
						c = 0;
						break;
					}
					c = *currentText;
				} while (isDigit<10>(c));

				if (exponentNegative) {
					exponent = -exponent;
				}
			}
		}
	}

	// Remove leading zeros from integer part.
	while (integerPartBegin != integerPartEnd && *integerPartBegin == '0') {
		++integerPartBegin;
	}

	// Remove trailing zeros from fractional part.
	while (fractionalPartBegin != fractionalPartEnd && *(fractionalPartEnd-1) == '0') {
		--fractionalPartEnd;
	}

	// If both eliminated, value is zero.  (e.g. "0.00e-5")
	if (integerPartBegin == integerPartEnd && fractionalPartBegin == fractionalPartEnd) {
		value = std::copysign(0.0, (negative ? -1.0 : +1.0));
		return currentText - text;
	}

	// If no remaining fractional part, remove trailing zeros
	// from integer part and adjust exponent.
	if (fractionalPartBegin == fractionalPartEnd) {
		while (integerPartBegin != integerPartEnd && *(integerPartEnd-1) == '0') {
			--integerPartEnd;
			++exponent;
		}
	}
	else {
		// Adjust exponent to make everything an integer.
		exponent -= (fractionalPartEnd - fractionalPartBegin);

		// If no remaining integer part, remove leading zeros.
		if (integerPartBegin == integerPartEnd) {
			while (fractionalPartBegin != fractionalPartEnd && *fractionalPartBegin == '0') {
				++fractionalPartBegin;
			}
		}
	}

	const int64 numDigits = (integerPartEnd - integerPartBegin) + (fractionalPartEnd - fractionalPartBegin);

	// These bounds don't need to be tight, and should be loose
	// to be on the safe side to account for rounding.
	// This just prevents very slow multiplication or division
	// by very large numbers, when they would just result in
	// infinity or zero.
	// No non-infinite double can represent a number as large as 1.0 * 10^310.
	constexpr int64 decimalExponentMax = 310;
	// No non-zero double can represent a number as small as 9.999... * 10^-326.
	constexpr int64 decimalExponentMin = -326;
	if (exponent + numDigits > decimalExponentMax) {
		// Exponent much too high: pre-emptively make infinity.
		value = std::copysign(std::numeric_limits<double>::infinity(), (negative ? -1.0 : 1.0));
		return currentText - text;
	}
	if (exponent + numDigits < decimalExponentMin) {
		// Exponent much too small: pre-emptively make zero.
		value = std::copysign(0.0, (negative ? -1.0 : +1.0));
		return currentText - text;
	}

	// Gather digits from integer and fractional parts into an array representing a single large integer.
	// This uses uint32, instead of uint64, since multiplying two uint64's together
	// wouldn't fit in a uint64.
	BufArray<uint32,16> integer;

	constexpr uint64 oneBillion = 1000000000;

	uint32 localBlock = 0;
	size_t localDigits = 0;
	while (integerPartBegin != integerPartEnd) {
		uint32 digit;
		digitToInteger(*integerPartBegin, digit);
		++integerPartBegin;
		localBlock = 10*localBlock + digit;
		++localDigits;
		if (localDigits == 9) {
			// Multiply integer by 10^9 and add localBlock.
			uint32 carry = localBlock;
			for (size_t i = 0, n = integer.size(); i < n; ++i) {
				uint64 v = uint64(integer[i])*oneBillion + carry;
				integer[i] = uint32(v);
				carry = uint32(v>>32);
			}
			if (carry != 0) {
				integer.append(carry);
			}
			localBlock = 0;
			localDigits = 0;
		}
	}
	while (fractionalPartBegin != fractionalPartEnd) {
		uint32 digit;
		digitToInteger(*fractionalPartBegin, digit);
		++fractionalPartBegin;
		localBlock = 10*localBlock + digit;
		++localDigits;
		if (localDigits == 9) {
			// Multiply integer by 10^9 and add localBlock.
			uint32 carry = localBlock;
			for (size_t i = 0, n = integer.size(); i < n; ++i) {
				uint64 v = uint64(integer[i])*oneBillion + carry;
				integer[i] = uint32(v);
				carry = uint32(v>>32);
			}
			if (carry != 0) {
				integer.append(carry);
			}
			localBlock = 0;
			localDigits = 0;
		}
	}

	constexpr uint32 powersOfTen[9] = {
		1,
		10,
		100,
		1000,
		10000,
		100000,
		1000000,
		10000000,
		100000000
	};

	if (localDigits != 0) {
		// Add the remaining digits into integer.

		// Multiply integer by final power of 10 and add localBlock.
		uint32 carry = localBlock;
		const uint64 finalMultiplier = powersOfTen[localDigits];
		for (size_t i = 0, n = integer.size(); i < n; ++i) {
			uint64 v = uint64(integer[i])*finalMultiplier + carry;
			integer[i] = uint32(v);
			carry = uint32(v>>32);
		}
		if (carry != 0) {
			integer.append(carry);
		}
	}

	if (exponent > 0) {
		// Multiply by 10^exponent.
		while (exponent >= 9) {
			uint32 carry = 0;
			for (size_t i = 0, n = integer.size(); i < n; ++i) {
				uint64 v = uint64(integer[i])*oneBillion + carry;
				integer[i] = uint32(v);
				carry = uint32(v>>32);
			}
			if (carry != 0) {
				integer.append(carry);
			}
			exponent -= 9;
		}
		if (exponent != 0) {
			uint32 carry = 0;
			const uint64 finalMultiplier = powersOfTen[exponent];
			for (size_t i = 0, n = integer.size(); i < n; ++i) {
				uint64 v = uint64(integer[i])*finalMultiplier + carry;
				integer[i] = uint32(v);
				carry = uint32(v>>32);
			}
			if (carry != 0) {
				integer.append(carry);
			}
		}
	}

	if (exponent >= 0) {
		// integer is now the exact value to be approximately represented as a double.
		const size_t numBlocks = integer.size();
		const size_t localTopBit = bitScanR32(integer.last());
		const size_t topBit = (numBlocks-1)*32 + localTopBit;
		uint64 mainBits;
		uint32 bitBelow;
		bool nonzeroBelow;
		if (numBlocks == 1) {
			if (topBit < bits) {
				// Small integer that can be represented exactly.
				value = double(integer[0]);
				if (negative) {
					value = -value;
				}
				return currentText - text;
			}
			mainBits = integer[0]>>(topBit-bits+1);
			bitBelow = (integer[0]>>(topBit-bits)) & 1;
			nonzeroBelow = (integer[0] & ((uint32(1)<<(topBit-bits))-1)) != 0;
		}
		else {
			assert(numBlocks > 1);

			nonzeroBelow = false;
			if (numBlocks == 2) {
				mainBits = integer[0] | (uint64(integer[1])<<32);
				if (topBit < bits) {
					// Small integer that can be represented exactly.
					value = double(mainBits);
					if (negative) {
						value = -value;
					}
					return currentText - text;
				}

				mainBits <<= (63-topBit);
			}
			else if (localTopBit == 31) {
				mainBits = integer[numBlocks-2] | (uint64(integer[numBlocks-1])<<32);
				uint32 combined = integer[0];
				for (size_t i = 1; i < numBlocks-2; ++i) {
					combined |= integer[i];
				}
				nonzeroBelow = (combined != 0);
			}
			else {
				mainBits =
					(uint64(integer[numBlocks-3])>>(localTopBit+1)) |
					(uint64(integer[numBlocks-2])<<(31-localTopBit)) |
					(uint64(integer[numBlocks-1])<<(63-localTopBit));

				uint32 combined = (integer[numBlocks-3]<<(31-localTopBit));
				for (size_t i = 0; i < numBlocks-3; ++i) {
					combined |= integer[i];
				}
				nonzeroBelow = (combined != 0);
			}

			bitBelow = (mainBits>>(63-bits)) & 1;
			nonzeroBelow |= ((mainBits & ((uint64(1)<<(63-bits))-1)) != 0);
			mainBits >>= (63-bits+1);
		}

		// Round half to even
		mainBits += (bitBelow && ((mainBits&1) || nonzeroBelow));

		uint64 mantissa = (mainBits<<(53-bits));
		// If the rounding rounded up to the next power of two,
		// we need to be careful.
		uint64 binaryExponent = topBit;
		if (mantissa == (uint64(1)<<53)) {
			++binaryExponent;
			mantissa >>= 1;
		}
		// Remove the implicit 1 bit at the top of mantissa.
		mantissa -= (uint64(1)<<52);

		if (binaryExponent >= 1024) {
			// Overflow, so infinity.
			value = std::copysign(std::numeric_limits<double>::infinity(), negative ? -1.0 : 1.0);
		}
		else {
			static_assert(sizeof(double) == sizeof(uint64));
			union {
				double d;
				uint64 i;
			} resultUnion;

			resultUnion.i = mantissa | ((binaryExponent + 1023)<<52) | (uint64(negative)<<63);
			value = resultUnion.d;
		}
		return currentText - text;
	}

	// exponent < 0

	constexpr uint32 powersOfFive[14] = {
		1,
		5,
		25,
		125,
		625,
		625*5,
		625*25,
		625*125,
		625*625,
		625*625*5,
		625*625*25,
		625*625*125,
		625*625*625,
		625*625*625*5
	};

	// Negative exponent, so first, compute the integer 10^(-exponent),
	// which will be the denominator.
	size_t power = size_t(-exponent);
	BufArray<uint32,16> denominator;
	size_t startPower = power % 13;
	power -= startPower;
	denominator.append(powersOfFive[startPower]);
	for (; power > 0; power -= 13) {
		// Multiply by 5^13 (1220703125) until power is fully applied.
		size_t n = denominator.size();
		uint64 v = uint64(denominator[0])*powersOfFive[13];
		denominator[0] = uint32(v);
		uint32 carry = uint32(v>>32);
		for (size_t i = 1; i < n; ++i) {
			v = uint64(denominator[i])*powersOfFive[13] + carry;
			denominator[i] = uint32(v);
			carry = uint32(v>>32);
		}
		if (carry) {
			denominator.append(carry);
		}
	}

	BufArray<uint32,16> quotient;

	// Since we've only put powers of 5 into the denominator,
	// compensate for them not being powers of 10.
	int64 binaryExponent = exponent;

	// Divide integer by denominator, computing as many 32-bit chunks
	// as are required to determine the correct rounding on the top "bits" bits.

	// First, shift denominator up such that its highest set bit is
	// bit 0 of some block.
	if (denominator.last() != 1) {
		assert(denominator.last() != 0);
		uint32 localTopBit = bitScanR32(denominator.last());
		assert(localTopBit >= 1);
		uint32 prev = denominator[0];
		denominator[0] <<= (32-localTopBit);
		for (size_t i = 1, n = denominator.size(); i < n; ++i) {
			const uint32 next = denominator[i];
			denominator[i] = (next << (32-localTopBit)) | (prev >> localTopBit);
			prev = next;
		}
		assert(prev >> localTopBit == 1);
		denominator.append(1);

		// We added (32-localTopBit) powers of two to the denominator, which
		// is equivalent to removing that many powers of two from the quotient,
		// so we need to add them back to compensate.
		binaryExponent += (32-localTopBit);
	}

	bool roundUpDenominator = false;
	for (size_t i = 0, n = denominator.size(); i < n-2; ++i) {
		roundUpDenominator |= (denominator[i] != 0);
	}
	// This reduced denominator is the same for every iteration.
	// It is between 2^32 and 2^33, both inclusive, and is guaranteed
	// to be an upper bound on the top 33 bits of denominator.
	const uint64 simpleDenominatorRoundDown = (uint64(1)<<32) | uint64(denominator[denominator.size()-2]);
	const uint64 simpleDenominatorRoundUp = simpleDenominatorRoundDown + uint64(roundUpDenominator);

	if (integer.size() > denominator.size()) {
		// Add zeros to low blocks of denominator.
		size_t numBlocksToAdd = integer.size() - denominator.size();
		size_t origSize = denominator.size();
		denominator.setSize(origSize + numBlocksToAdd);
		for (size_t i = denominator.size()-1; i >= numBlocksToAdd; --i) {
			denominator[i] = denominator[i-numBlocksToAdd];
		}
		for (size_t i = 0; i < numBlocksToAdd; ++i) {
			denominator[i] = 0;
		}
		binaryExponent += 32*numBlocksToAdd;
	}

	while (true) {
		// Shift integer up such that its highest
		// set bit is bit 31 of the block one higher than denominator.
		if ((integer.last() >> 31) == 0) {
			assert(integer.last() != 0);
			uint32 localTopBit = bitScanR32(integer.last());
			uint32 prev = integer[0];
			integer[0] <<= (31-localTopBit);
			for (size_t i = 1, n = integer.size(); i < n; ++i) {
				const uint32 next = integer[i];
				integer[i] = (next << (31-localTopBit)) | (prev >> (localTopBit+1));
				prev = next;
			}
			assert(prev >> (localTopBit+1) == 0);

			// We added 31-localTopBit powers of two to the numerator, which
			// is equivalent to adding that many powers of two to the quotient,
			// so we need to remove them to compensate.
			binaryExponent -= (31-localTopBit);

			if (quotient.size() != 0) {
				// Shift quotient by the same amount.
				prev = quotient[0];
				quotient[0] <<= (31-localTopBit);
				for (size_t i = 1, n = quotient.size(); i < n; ++i) {
					const uint32 next = quotient[i];
					quotient[i] = (next << (31-localTopBit)) | (prev >> (localTopBit+1));
					prev = next;
				}
				prev >>= (localTopBit+1);
				if (prev != 0) {
					quotient.append(prev);
				}
			}
		}

		if (integer.size() < denominator.size()) {
			// Add zeros to low blocks of integer.
			size_t numBlocksToAdd = denominator.size() - integer.size();
			integer.setSize(integer.size() + numBlocksToAdd);
			for (size_t i = integer.size()-1; i >= numBlocksToAdd; --i) {
				integer[i] = integer[i-numBlocksToAdd];
			}
			for (size_t i = 0; i < numBlocksToAdd; ++i) {
				integer[i] = 0;
			}
			binaryExponent -= 32*numBlocksToAdd;

			if (quotient.size() != 0) {
				// Add the same number of zeros to quotient.
				quotient.setSize(quotient.size() + numBlocksToAdd);
				for (size_t i = quotient.size()-1; i >= numBlocksToAdd; --i) {
					quotient[i] = quotient[i-numBlocksToAdd];
				}
				for (size_t i = 0; i < numBlocksToAdd; ++i) {
					quotient[i] = 0;
				}
			}
		}

		// Do the division for this iteration
		uint64 localNumerator = (uint64(integer.last())<<32) | uint64(integer[integer.size()-2]);
		uint32 localQuotient = uint32(localNumerator / simpleDenominatorRoundUp);

		// Add localQuotient to quotient.
		if (quotient.size() == 0) {
			quotient.append(localQuotient);
		}
		else {
			quotient[0] += localQuotient;
		}
		uint32 carry = (quotient[0] < localQuotient);
		for (size_t i = 1, n = quotient.size(); carry && i < n; ++i) {
			++quotient[i];
			carry = (quotient[i] == 0);
		}
		if (carry) {
			quotient.append(1);
		}

		// Subtract localQuotient*denominator from integer.
		carry = 0;
		for (size_t i = 0, n = denominator.size(); i < n; ++i) {
			uint64 product = uint64(denominator[i])*uint64(localQuotient) + uint64(carry);
			uint32 lowerPart = uint32(product);
			uint32 origInteger = integer[i];
			uint32 newInteger = origInteger - lowerPart;
			integer[i] = newInteger;
			carry = uint32(product>>32) + (newInteger > origInteger);
		}
		assert(integer.last() >= carry);
		integer.last() -= carry;

		// Remove zeros at the top of integer.
		while (integer.size() > 0 && integer.last() == 0) {
			integer.setSize(integer.size()-1);
		}


		bool certainRoundDown = false;
		bool certainRoundUp = false;
		size_t numBlocks = quotient.size();
		size_t localTopBit = bitScanR32(quotient[numBlocks-1]);
		size_t topBit = 32*(numBlocks-1) + localTopBit;
		size_t numBits = topBit + 1;
		size_t roundingBitNumber = topBit - bits;
		size_t roundingBitBlock = (roundingBitNumber >> 5);
		size_t roundingBitIndex = (roundingBitNumber & 0x1F);
		if (integer.size() == 0) {
			// The remainder is zero, so we're done.
			if (topBit < bits || !(quotient[roundingBitBlock] & (uint32(1)<<roundingBitIndex))) {
				// Rounding bit is 0, so round down.
				certainRoundDown = true;
			}
			else {
				// Rounding bit is 1, so check if half and even for round half to even.
				size_t parityBitNumber = roundingBitNumber+1;
				size_t parityBitBlock = (parityBitNumber >> 5);
				size_t parityBitIndex = (parityBitNumber & 0x1F);
				bool odd = (quotient[parityBitBlock] & (uint32(1)<<parityBitIndex)) != 0;
				if (odd) {
					// Round up to even, whether half or above.
					certainRoundUp = true;
				}
				else {
					uint32 mask = (uint32(1)<<roundingBitIndex)-1;
					certainRoundUp = (quotient[roundingBitBlock] & mask) != 0;
					while (!certainRoundUp && (roundingBitBlock > 0)) {
						--roundingBitBlock;
						certainRoundUp = (quotient[roundingBitBlock] != 0);
					}
					// If no bits were 1, we round half down to even.
					certainRoundDown = !certainRoundUp;
				}
			}
		}
		else {
			// The +6 is just so that most of the time, more bits aren't needed,
			// and so that if bits is 24, one iteration is often enough.
			// It also needs to be at least a couple, so that there's a rounding bit
			// and so that the margin of error is below the rounding bit.
			if (numBits >= bits + 6) {
				// localQuotient is below the true (shifted) local quotient by less than 2:
				// trueLocalQuotient - localQuotient
				// = n/d - floor(floor(n)/ceil(d))
				// < n/d - floor(n)/ceil(d) + 1
				// < n/d - (n-1)/(d+1) + 1
				// = (n(d+1) - nd)/(d(d+1)) + 1/(d+1) + 1
				// = n/(d(d+1)) + 1/(d+1) + 1
				// < (d^2)/(d(d+1)) + 1/(d+1) + 1; because n < d^2
				// = d/(d+1) + 1/(d+1) + 1
				// = (d+1)/(d+1) + 1
				// = 2
				// This gives us an upper bound with which to check for convergence.
				// This means that the true quotient is above quotient and
				// strictly less than quotient+2.

				// If the rounding bit is one, it's definitely rounding up,
				// since there's still a remainder, so we're done.
				if (quotient[roundingBitBlock] & (uint32(1)<<roundingBitIndex)) {
					certainRoundUp = true;
				}
				else {
					// If the rounding bit and the bits below are at most
					// 01111...11110, it's definitely rounding down, since
					// the true quotient isn't 2 more than quotient, so we're done.
					uint32 mask = (uint32(1)<<roundingBitIndex)-1;
					certainRoundDown = (quotient[roundingBitBlock] & mask) != mask;
					while (!certainRoundDown && (roundingBitBlock > 0)) {
						--roundingBitBlock;
						certainRoundDown = (quotient[roundingBitBlock] != ~uint32(0));
					}

					// That means, the only ambiguous case left is
					// 01111...11111, in which case, we keep iterating.
				}
			}
		}

		if (certainRoundDown || certainRoundUp) {
			uint64 mainBits = uint64(quotient[numBlocks-1]);
			binaryExponent += 32*(numBlocks-1);
			if (localTopBit+1 > bits) {
				mainBits >>= (localTopBit+1-bits);
				binaryExponent += (localTopBit+1-bits);
			}
			else if (localTopBit+1 < bits) {
				mainBits <<= (bits-(localTopBit+1));
				binaryExponent -= (bits-(localTopBit+1));
				// Bounds check in case numerator ran out on first iteration.
				if (numBlocks >= 2) {
					uint64 next = uint64(quotient[numBlocks-2]);
					if (localTopBit+1+32 > bits) {
						mainBits |= (next >> (localTopBit+1+32-bits));
					}
					else if (localTopBit+1+32 == bits) {
						mainBits |= next;
					}
					else {
						mainBits |= (next << (bits - (localTopBit+1+32)));
						if (numBlocks >= 3) {
							next = uint64(quotient[numBlocks-3]);
							mainBits |= (next >> (localTopBit+1+64-bits));
						}
					}
				}
			}
			if (certainRoundUp) {
				// Round up
				++mainBits;
			}

			binaryExponent += (bits-1);
			uint64 mantissa = (mainBits<<(53-bits));
			// If the rounding rounded up to the next power of two,
			// we need to be careful.
			if (mantissa == (uint64(1)<<53)) {
				++binaryExponent;
				mantissa >>= 1;
			}
			assert(mantissa < (uint64(1)<<53));
			assert(mantissa >= (uint64(1)<<52));
			// Remove the implicit 1 bit at the top of mantissa.
			mantissa -= (uint64(1)<<52);

			if (binaryExponent >= 1024) {
				// Overflow, so infinity.
				value = std::copysign(std::numeric_limits<double>::infinity(), negative ? -1.0 : 1.0);
			}
			else if (binaryExponent <= -1023) {
				// Possibly denormal or zero.
				// If it's denormal, we may need to fix the rounding.
				// FIXME: Implement this correctly, instead of just making it zero!!!
				value = std::copysign(0.0, negative ? -1.0 : 1.0);
			}
			else {
				// Normal number.
				static_assert(sizeof(double) == sizeof(uint64));
				union {
					double d;
					uint64 i;
				} resultUnion;

				resultUnion.i = mantissa | ((binaryExponent + 1023)<<52) | (uint64(negative)<<63);
				value = resultUnion.d;
			}

			// Done!
			break;
		}
	}

	return currentText - text;
}

size_t textToFloat(const char* text, const char*const end, float& value) {
	double doubleValue;
	size_t numberLength = textToDoubleWithPrecision(text, end, 24, doubleValue);
	if (numberLength != 0) {
		value = float(doubleValue);
	}
	return numberLength;
}

size_t textToDouble(const char* text, const char*const end, double& value) {
	return textToDoubleWithPrecision(text, end, 53, value);
}

static void doubleToTextWithPrecision(const double value, size_t bits, Array<char>& text) {
	const uint64& valueInt = *reinterpret_cast<const uint64*>(&value);
	const bool negative = (valueInt >> 63) != 0;
	const int64 exponent = int64((valueInt >> 52) & 0x7FF) - 0x3FF;
	uint64 mantissa = (valueInt & ((uint64(1)<<52)-1));

	const size_t initialTextSize = text.size();

	if (exponent == 0x400) {
		// Infinity or NaN
		constexpr const char* infinityText = "-infinity";
		constexpr const char* NaNText = "NaN";
		const char* textToCopy;
		size_t copySize;
		if (mantissa != 0) {
			// NaN
			textToCopy = NaNText;
			copySize = 3;
		}
		else if (negative) {
			// -infinity
			textToCopy = infinityText;
			copySize = 9;
		}
		else {
			// infinity: Skip the negative sign.
			textToCopy = infinityText+1;
			copySize = 8;
		}
		text.setSize(initialTextSize + copySize);
		for (size_t i = 0; i < copySize; ++i) {
			text[initialTextSize + i] = textToCopy[i];
		}
		return;
	}

	if (exponent == 0) {

	}

	mantissa |= (uint64(1)<<52);

	//FIXME: Implement this!!!
}

void floatToText(float value, Array<char>& text) {
	doubleToTextWithPrecision(double(value), 24, text);
}

void doubleToText(double value, Array<char>& text) {
	doubleToTextWithPrecision(double(value), 53, text);
}

} // namespace text
OUTER_NAMESPACE_END
