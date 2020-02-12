// This file contains definitions of functions for parsing and generating
// text strings representing numbers.

#include "text/NumberText.h"
#include "Array.h"
#include "ArrayDef.h"
#include "Bits.h"
#include "Types.h"

#include <algorithm> // For std::min
#include <cmath> // For std::copysign
#include <limits> // For std::numeric_limits

OUTER_NAMESPACE_BEGIN
namespace text {

using namespace Common;

constexpr static uint64 oneBillion = 1000000000;

constexpr static uint32 powersOfTen[9] = {
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

static void prependZeros(Array<uint32>& array, size_t numZeros) {
	if (numZeros == 0) {
		return;
	}

	size_t origSize = array.size();
	array.setSize(origSize + numZeros);
	for (size_t i = array.size()-1; i >= numZeros; --i) {
		array[i] = array[i-numZeros];
	}
	for (size_t i = 0; i < numZeros; ++i) {
		array[i] = 0;
	}
}

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
	assert(denominator.last() == 1);

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
		prependZeros(denominator, numBlocksToAdd);
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
			prependZeros(integer, numBlocksToAdd);
			binaryExponent -= 32*numBlocksToAdd;

			if (quotient.size() != 0) {
				// Add the same number of zeros to quotient.
				prependZeros(quotient, numBlocksToAdd);
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

		// At this point, localQuotient is below the true (shifted) local quotient by less than 2:
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
		// If we check whether the remainder is at least denominator afterward,
		// we can reduce this difference to be less than 1.

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
		assert(carry == 0);

		// If integer is still greater than or equal to denominator,
		// subtract denominator from integer and add 1 to quotient.
		// This allows for robust handling of the case where integer is
		// an exact multiple of denominator, but not of simpleDenominatorRoundUp,
		// and round half to even behaviour rounding up comes into play.
		bool isIntegerLessThanDenominator = false;
		for (size_t i = denominator.size(); i > 0; ) {
			--i;
			isIntegerLessThanDenominator = integer[i] < denominator[i];
			if (integer[i] != denominator[i]) {
				break;
			}
		}
		if (!isIntegerLessThanDenominator) {
			// There's room to subtract another denominator from integer,
			// so do that and add 1 to quotient.
			carry = 0;
			for (size_t i = 0, n = denominator.size(); i < n; ++i) {
				uint64 sum = uint64(denominator[i]) + uint64(carry);
				uint32 origInteger = integer[i];
				uint32 newInteger = origInteger - uint32(sum);
				integer[i] = newInteger;
				carry = uint32(sum>>32) + (newInteger > origInteger);
			}
			assert(carry == 0);
			carry = 1;
			for (size_t i = 0, n = quotient.size(); carry && i < n; ++i) {
				++quotient[i];
				carry = (quotient[i] == 0);
			}
			if (carry) {
				quotient.append(1);
			}
		}

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
		// The +1 is so that there's a valid rounding bit.
		else if (numBits >= bits + 1) {
			// As described above, localQuotient is below the true (shifted)
			// local quotient by less than 2, and if it was below by 1 or more,
			// we added an extra 1, so it's now below by less than 1.
			// This gives us an upper bound with which to check for convergence.
			// This means that the true quotient is strictly above quotient and
			// strictly below quotient+1.

			// If the rounding bit is one, it's definitely rounding up,
			// since there's still a remainder, so we're done.
			// If the rounding bit is zero, the true quotient isn't 1 more than
			// quotient, so it can never end up being flipped to 1,
			// even if the current bits below are all 1.
			certainRoundUp = (quotient[roundingBitBlock] & (uint32(1)<<roundingBitIndex));
			certainRoundDown = !certainRoundUp;
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

static void powerOfTwoBase1Billion(Array<uint32>& integer, uint32 exponent) {
	// Compute 2^exponent
	// 2^29 < 1 billion < 2^30
	size_t power = exponent;
	uint32 startPower = exponent % 29;
	integer.append(uint32(1)<<startPower);
	power -= startPower;
	for (; power > 0; power -= 29) {
		// Multiply by 2^29 (536870912) until power is fully applied.
		size_t n = integer.size();
		uint64 v = uint64(integer[0])*(uint32(1)<<29);
		integer[0] = uint32(v % oneBillion);
		uint32 carry = uint32(v / oneBillion);
		for (size_t i = 1; i < n; ++i) {
			v = uint64(integer[i])*(uint32(1)<<29) + carry;
			integer[i] = uint32(v % oneBillion);
			carry = uint32(v / oneBillion);
		}
		if (carry) {
			integer.append(carry);
		}
	}
}

static void multiplyBase1Billion(Array<uint32>& integer, uint32 factor) {
	uint64 v = uint64(integer[0])*factor;
	integer[0] = uint32(v % oneBillion);
	uint32 carry = uint32(v / oneBillion);
	for (size_t i = 1, n = integer.size(); i < n; ++i) {
		v = uint64(integer[i])*factor + carry;
		integer[i] = uint32(v % oneBillion);
		carry = uint32(v / oneBillion);
	}
	if (carry != 0) {
		integer.append(carry);
	}
}

static void multiplyBase1Billion(Array<uint32>& integer, uint32 factorLow, uint32 factorHigh) {
	uint64 vLow = uint64(integer[0])*factorLow;
	uint64 vHigh = uint64(integer[0])*factorHigh;
	integer[0] = uint32(vLow % oneBillion);
	uint32 carryLow = uint32(vLow / oneBillion);
	uint32 carryLow2 = uint32(vHigh % oneBillion);
	uint32 carryHigh = uint32(vHigh / oneBillion);
	carryLow += carryLow2;
	if (carryLow >= oneBillion) {
		carryLow -= oneBillion;
		++carryHigh;
	}
	for (size_t i = 1, n = integer.size(); i < n; ++i) {
		vLow = uint64(integer[i])*factorLow + carryLow;
		vHigh = uint64(integer[i])*factorHigh + carryHigh;
		integer[i] = uint32(vLow % oneBillion);
		carryLow = uint32(vLow / oneBillion);
		carryLow2 = uint32(vHigh % oneBillion);
		carryHigh = uint32(vHigh / oneBillion);
		carryLow += carryLow2;
		if (carryLow >= oneBillion) {
			carryLow -= oneBillion;
			++carryHigh;
		}
	}
	if (carryLow != 0 || carryHigh != 0) {
		integer.append(carryLow);
		if (carryHigh != 0) {
			integer.append(carryHigh);
		}
	}
}

// v must be strictly less than 1 billion, so that digits will be 9 long.
// Returns the index into digits of the first non-zero digit,
// (or the last zero digit if v is zero.)
// Earlier digits will still be filled with zeros.
static size_t base1BillionToDecimal(char* digits, uint32 v) {
	size_t i = 9;
	while (v >= 10) {
		char d = char(v % 10);
		v  = v / 10;
		--i;
		digits[i] = d;
	}
	--i;
	digits[i] = char(v);
	const size_t firstDigit = i;
	while (i > 0) {
		--i;
		digits[i] = 0;
	}
	return firstDigit;
}

static void appendDigitText(bool isNegative, const Array<char>& midDigits, size_t firstMidDigit, size_t i, int32 decimalExponent, Array<char>& text) {
	// Truncate to the current digit, rounding down in mangitude.
	// Remove any trailing zeros.
	char midDigit = midDigits[i];
	while (midDigit == 0) {
		--i;
		midDigit = midDigits[i];
	}
	size_t numDigits = i+1-firstMidDigit;

	bool isNegativeExponent = (decimalExponent < 0);
	uint32 positiveExponent = isNegativeExponent ? uint32(-decimalExponent) : uint32(decimalExponent);

	// Compute the exponent digits.  (There can be at most 3 exponent digits for doubles.)
	char exponentDigits[3];
	size_t numExponentDigits = 1;
	size_t firstExponentDigit = 2;
	while (positiveExponent >= 10) {
		exponentDigits[firstExponentDigit] = positiveExponent % 10;
		positiveExponent = positiveExponent / 10;
		++numExponentDigits;
		--firstExponentDigit;
	}
	exponentDigits[firstExponentDigit] = positiveExponent;

	const size_t initialTextSize = text.size();
	text.setSize(initialTextSize + size_t(isNegative) + numDigits + (numDigits == 1) + 2 + size_t(isNegativeExponent) + numExponentDigits);
	size_t texti = initialTextSize;

	// Add the negative sign if negative.
	if (isNegative) {
		text[texti] = '-';
		++texti;
	}

	// Add [first digit].[other digits]
	text[texti] = (midDigits[firstMidDigit] + '0');
	++texti;
	text[texti] = '.';
	++texti;
	if (numDigits == 1) {
		text[texti] = '0';
		++texti;
	}
	else {
		for (size_t j = firstMidDigit+1; j <= i; ++j) {
			text[texti] = (midDigits[j] + '0');
			++texti;
		}
	}

	// Add the exponent.
	text[texti] = 'e';
	++texti;
	if (isNegativeExponent) {
		text[texti] = '-';
		++texti;
	}
	while (firstExponentDigit < 3) {
		text[texti] = (exponentDigits[firstExponentDigit] + '0');
		++firstExponentDigit;
		++texti;
	}
}

static void doubleToTextWithPrecision(const double value, size_t bits, Array<char>& text) {
	const uint64& valueInt = *reinterpret_cast<const uint64*>(&value);
	const bool negative = (valueInt >> 63) != 0;
	int32 exponent = int32((valueInt >> 52) & 0x7FF) - 0x3FF;
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

	// Basic idea of the algorithm:
	// 0) Determine sign and take abs(value).
	// 1) Convert, to base 1 billion:
	//   a) abs(value), to the precision where the rounding is certain,
	//   b) the number halfway between abs(value) and the next larger value,
	//      to the precision where the digits are certainly different from value,
	//   c) the number halfway between abs(value) and the previous smaller value,
	//      to the precision where the digits are certainly different from value,
	// 2) Convert them to decimal.
	// 3) Round value until just before it would be >= b or <= c.
	// 4) Convert to text.

	if (exponent == -0x3FF) {
		// FIXME: Handle denormal numbers, instead of just treating them all as zero!!!
		// TODO: Consider adding negative sign for negative zero, though most cases probably prefer without the sign.
		text.setSize(initialTextSize + 3);
		text[initialTextSize] = '0';
		text[initialTextSize+1] = '.';
		text[initialTextSize+2] = '0';
		return;
	}

	const bool isPowerOfTwo = (mantissa == 0);

	mantissa |= (uint64(1)<<52);

	uint64 larger;
	uint64 smaller;
	if (!isPowerOfTwo || bits != 53) {
		// Common case
		mantissa += mantissa;
		exponent -= 53;

		larger = mantissa + (uint64(1)<<(53-bits));
		smaller = mantissa - (uint64(1)<<(53-bits-size_t(isPowerOfTwo)));
	}
	else {
		// bits is 53 and mantissa is a power of two, so the smaller bound is
		// smaller by a quarter of the smallest increment of the original mantissa,
		// and the larger bound is larger by half of the smallest increment of
		// the original mantissa.
		mantissa <<= 2;
		exponent -= 54;

		larger = mantissa + 2;
		smaller = mantissa - 1;
	}

	int32 lowBit = int32(bitScanF64(mantissa));
	if (exponent + lowBit >= 0) {
		// mantissa * 2^exponent represents an integer,
		// i.e. exponent + lowBit is >= 0, so it's safe to make smaller
		// at least as small as the next smallest integer, and larger
		// at least as large as the next largest integer.  They may
		// already be at least that small or large, respectively.
		int32 smallerLowBit = int32(bitScanF64(smaller));
		if (exponent + smallerLowBit >= 0) {
			// All 3 are already integers.  Handle this case as normal.
			lowBit = std::min(lowBit, smallerLowBit);
			if (lowBit != 0) {
				mantissa >>= lowBit;
				larger >>= lowBit;
				smaller >>= lowBit;
				exponent += lowBit;
			}
		}
		else {
			// Mantissa is an integer, but smaller is not, so decrease smaller.
			int32 integerBit = -exponent;
			mantissa >>= integerBit;
			smaller = mantissa - 1;
			// Larger may have already been an integer if mantissa was a power of two, but
			// it'd be exactly 1 larger anyway, so it's still safe to make it 1 more than mantissa.
			larger = mantissa + 1;
			exponent = 0;
		}
	}
	else {
		// General case: mantissa is not an integer.
		// This would also work for when mantissa is an integer,
		// but that would result in using the slow, fractional codepath.
		int32 lowBit = std::min(lowBit, int32(bitScanF64(smaller)));
		if (lowBit != 0) {
			mantissa >>= lowBit;
			larger >>= lowBit;
			smaller >>= lowBit;
			exponent += lowBit;
		}
	}

	// The integer or numerator in base 1 billion.
	BufArray<uint32,16> midInteger;
	BufArray<uint32,16> largerInteger;
	BufArray<uint32,16> smallerInteger;

	if (exponent >= 0) {
		// First, compute 2^exponent
		powerOfTwoBase1Billion(midInteger, uint32(exponent));

		// Copy to largerInteger and smallerInteger.
		size_t n = midInteger.size();
		largerInteger.setSize(n);
		for (size_t i = 0; i != n; ++i) {
			largerInteger[i] = midInteger[i];
		}
		smallerInteger.setSize(n);
		for (size_t i = 0; i != n; ++i) {
			smallerInteger[i] = midInteger[i];
		}

		// Multiply midInteger by mantissa.
		// NOTE: 2^55 < (1 billion)^2, so only two parts of mantissa are needed.
		uint32 mantissaLow = uint32(mantissa % oneBillion);
		uint32 mantissaHigh = uint32(mantissa / oneBillion);
		multiplyBase1Billion(midInteger, mantissaLow, mantissaHigh);

		if (midInteger.size() == 1) {
			// If less than 1 billion and an integer, no rounding,
			// since we're not using scientific notation.
			// NOTE: At most 9 digits, minus 1 because high digit is never written to buffer.
			char buffer[8];
			uint32 v = midInteger[0];
			size_t i = 8;
			while (v >= 10) {
				uint8 d = uint8(v % 10);
				v = v / 10;
				--i;
				buffer[i] = char(d + '0');
			}
			// Preallocate text.
			// Negative sign, highest digit, lower digits, decimal point, and trailing zero.
			text.setSize(initialTextSize + size_t(negative) + 1 + (8-i) + 2);
			char* output = text.data() + initialTextSize;
			if (negative) {
				*output = '-';
				++output;
			}
			*output = char(v + '0');
			++output;
			while (i < 8) {
				*output = buffer[i];
				++output;
				++i;
			}
			output[0] = '.';
			output[1] = '0';
			return;
		}

		// At least 1 billion, so will be using scientific notation, and might round.

		// Multiply largerInteger by larger.
		uint32 largerLow = uint32(larger % oneBillion);
		uint32 largerHigh = uint32(larger / oneBillion);
		multiplyBase1Billion(largerInteger, largerLow, largerHigh);

		// Multiply smallerInteger by smaller.
		uint32 smallerLow = uint32(smaller % oneBillion);
		uint32 smallerHigh = uint32(smaller / oneBillion);
		multiplyBase1Billion(smallerInteger, smallerLow, smallerHigh);

		// Convert all three numbers to decimal starting from the highest blocks, until they diverge in rounding.
		// 3 groups of 9 digits ensures at least 19 digits, which is probably always enough.
		// 1 extra digit at the beginning is just so that when rounding midDigits up, there
		// isn't as much of a special case required for rounding up to a new power of ten.
		BufArray<char,28> midDigits;
		BufArray<char,28> largerDigits;
		BufArray<char,28> smallerDigits;

		// If smaller is fewer digits than mid, truncating is always okay.
		bool isSmallerFewerDigits = (smallerInteger.size() != midInteger.size());

		// If larger is more digits than mid, the highest digit of larger is 1,
		// (even if bits is only 1), and then rounding up is only an issue
		// if largerInteger is exactly a power of ten.
		bool isLargerMoreDigits = (midInteger.size() != largerInteger.size());

		// One extra at the beginning in case we need to round up to a new power of 10 at the end.
		midDigits.setSize(10);
		midDigits[0] = 0;
		size_t firstMidDigit = 1 + base1BillionToDecimal(midDigits.data()+1, midInteger.last());

		// The extra digit at the beginning here is just to match midDigits.
		largerDigits.setSize(10);
		largerDigits[0] = 0;
		size_t firstLargerDigit = 1 + base1BillionToDecimal(largerDigits.data()+1, largerInteger.last());

		isLargerMoreDigits |= (firstLargerDigit != firstMidDigit);
		if (isLargerMoreDigits) {
			if (firstLargerDigit == 9) {
				firstLargerDigit = 0;
				largerInteger.setSize(largerInteger.size()-1);
				base1BillionToDecimal(largerDigits.data(), largerInteger.last());
			}
			else {
				++firstLargerDigit;
			}
			// Add 10 to the digit below, (which is always 0 unless bits is very small).
			// This makes handling the larger bound easier, since
			// now, it always lines up with midDigits.
			largerDigits[firstLargerDigit] += 10;
		}

		if (!isSmallerFewerDigits) {
			// The extra digit at the beginning here is just to match midDigits.
			smallerDigits.setSize(10);
			smallerDigits[0] = 0;
			size_t firstSmallerDigit = 1 + base1BillionToDecimal(smallerDigits.data()+1, smallerInteger.last());
			isSmallerFewerDigits = (firstSmallerDigit != firstMidDigit);
		}

		bool isSmallerStrict = isSmallerFewerDigits;
		bool isLargerStrict = false;
		bool isLargerStrictPending = false;
		size_t digiti = firstMidDigit;
		intptr_t blocki = midDigits.size()-1;
		while (true) {
			char midDigit = midDigits[digiti];
			if (digiti+1 == midDigits.size()) {
				midDigits.setSize(midDigits.size()+9);
				--blocki;
				base1BillionToDecimal(midDigits.data() + (digiti+1), (blocki >= 0) ? midInteger[blocki] : 0);
			}
			char nextMidDigit = midDigits[digiti+1];
			bool roundUp = (nextMidDigit >= 5);

			if (!isSmallerStrict) {
				if (digiti == smallerDigits.size()) {
					smallerDigits.setSize(smallerDigits.size()+9);
					// blocki was already decremented above on the previous iteration.
					base1BillionToDecimal(smallerDigits.data() + digiti, (blocki+1 >= 0) ? smallerInteger[blocki+1] : 0);
				}
				isSmallerStrict = smallerDigits[digiti] < midDigit;
			}
			if (!roundUp && isSmallerStrict) {

				int32 decimalExponent = int32((9-firstMidDigit) + 9*(midInteger.size()-1));
				appendDigitText(negative, midDigits, firstMidDigit, digiti, decimalExponent, text);

				return;
			}

			if (!isLargerStrict) {
				if (isLargerStrictPending) {
					// We already had a larger digit in larger, but
					// larger had all zero after it, so we couldn't round up.
					// Now, if the current digit is anything other than 9,
					// it won't round up with a carry, so we're safe.
					isLargerStrict = midDigit != 9;
					isLargerStrictPending = !isLargerStrict;
				}
				else {
					if (digiti == largerDigits.size()) {
						largerDigits.setSize(largerDigits.size()+9);
						// blocki was already decremented above on the previous iteration.
						base1BillionToDecimal(largerDigits.data() + digiti, (blocki+1 >= 0) ? largerInteger[blocki+1] : 0);
					}
					isLargerStrict = largerDigits[digiti] > midDigit + 1;
					if (largerDigits[digiti] == midDigit + 1) {
						// If anything is nonzero in further digits, larger is strictly larger if mid is rounded up.
						bool allZero = true;
						for (size_t digitj = digiti+1; allZero && digitj < largerDigits.size(); ++digitj) {
							allZero = (largerDigits[digitj] == 0);
						}
						for (size_t blockj = 0; allZero && intptr_t(blockj) < blocki+1; ++blockj) {
							allZero = (largerInteger[blockj] == 0);
						}
						if (!allZero) {
							isLargerStrict = true;
						}
						else {
							// Can't round up yet, until we get a new digit that isn't 9,
							// so that rounding up doesn't make it equal to larger.
							isLargerStrictPending = true;
						}
					}
				}
			}
			if (roundUp && isLargerStrict) {
				// Round current digit up, propagating any carry as necessary,
				// possibly adding an additional digit, and possibly removing
				// trailing zeros if the current digit is 9.
				++midDigit;
				while (midDigit == 10) {
					midDigits[digiti] = 0;
					--digiti;
					midDigit = midDigits[digiti];
					++midDigit;
				}
				midDigits[digiti] = midDigit;
				if (digiti < firstMidDigit) {
					--firstMidDigit;
				}
				int32 decimalExponent = int32((9-firstMidDigit) + 9*(midInteger.size()-1));
				appendDigitText(negative, midDigits, firstMidDigit, digiti, decimalExponent, text);

				return;
			}

			++digiti;
		}
	}

	// Not an integer, so compute denominator, 2^(-exponent).
	BufArray<uint32,16> denominator;
	powerOfTwoBase1Billion(denominator, uint32(-exponent));

	// Multiply denominator by a power of ten, such that its top block
	// is at least 1 and less than 10, if it's not already, and
	// if is fewer blocks than midInteger, prepend blocks of zero
	// to make them equal.  Do the multiplication first, since it
	// will add a block, unless it's already in [1,10).
	uint32 topBlock = denominator.last();
	size_t topBlockPower = 0;
	while (topBlockPower <= 7 && topBlock < powersOfTen[topBlockPower+1]) {
		++topBlockPower;
	}
	int32 denominatorPower = 0;
	if (topBlockPower != 0) {
		multiplyBase1Billion(denominator, powersOfTen[9-topBlockPower]);
		denominatorPower += int32(9-topBlockPower);
	}
	if (denominator.size() < midInteger.size()) {
		size_t numBlocksToAdd = denominator.size() - midInteger.size();
		prependZeros(denominator, numBlocksToAdd);
		denominatorPower += int32(9*numBlocksToAdd);
	}
	assert(denominator.last() <= 9);
	// denominator array = true denominator * 10^denominatorPower

	bool roundUpDenominator = false;
	for (size_t i = 0, n = denominator.size(); i < n-2; ++i) {
		roundUpDenominator |= (denominator[i] != 0);
	}

	// This reduced denominator is the same for every iteration.
	// It is between 10^9 and 10^10, both inclusive, and is guaranteed
	// to be an upper bound on the top 10 digits of denominator.
	const uint64 simpleDenominatorRoundDown = (uint64(denominator.last())*oneBillion) + uint64(denominator[denominator.size()-2]);
	const uint64 simpleDenominatorRoundUp = simpleDenominatorRoundDown + uint64(roundUpDenominator);

	struct DeferredDecimalFraction {
		Array<uint32>& numerator;
		const Array<uint32>& denominator;
		const uint64 simpleDenominatorRoundUp;
		BufArray<char,28> quotientDigits;
		int32 quotientExponent;

		// denominatorPower is temporarily stored in quotientExponent
		DeferredDecimalFraction(
			Array<uint32>& numerator_,
			const Array<uint32>& denominator_,
			const uint64 simpleDenominatorRoundUp_,
			const int32 denominatorPower
		) : numerator(numerator_),
			denominator(denominator_),
			simpleDenominatorRoundUp(simpleDenominatorRoundUp_),
			quotientExponent((numerator_.size() != 0) ? denominatorPower : 0)
		{
			// Compute some initial digits, to solidify the correct quotientExponent.
			computeMoreDigits();
		}

		void shiftUpNumerator() {
			// Multiply numerator by a power of ten, such that its top block
			// is at least 100 million and less than 1 billion, and such that it is
			// the same number of blocks as numerator.
			size_t digitsAdded = 0;
			if (numerator.size() < denominator.size()) {
				size_t numBlocksToAdd = denominator.size() - numerator.size();
				prependZeros(numerator, numBlocksToAdd);
				digitsAdded += 9*numBlocksToAdd;
			}
			uint32 topBlock = numerator.last();
			size_t topBlockPower = 0;
			while (topBlockPower <= 7 && topBlock < powersOfTen[topBlockPower+1]) {
				++topBlockPower;
			}
			if (topBlockPower != 8) {
				// NOTE: This should never add a new block, so it should keep the size the same.
				multiplyBase1Billion(numerator, powersOfTen[8-topBlockPower]);
				digitsAdded += (8-topBlockPower);
			}

			if (quotientDigits.size() == 0) {
				quotientDigits.setSize(9);
				// true quotient
				// = true numerator / true denominator
				// = (numerator array * 10^numeratorPower) / (denominator array * 10^denominatorPower)
				// = (numerator array / denominator array) * 10^(numeratorPower - denominatorPower)
				// (numerator array / denominator array) is up to 10^9 - 1, less than 10^9,
				// so the top digit of quotientDigits, if nonzero, will be worth 10^8 * 10^(numeratorPower - denominatorPower),
				// so quotientExponent = 8 + numeratorPower - denominatorPower.
				// If the top digit ends up not being used, quotientExponent will be reduced by 1
				// and quotientDigits will be shifted over by 1.
				// denominstaorPower was previously stored in quotientExponent.
				int32 numeratorPower = int32(digitsAdded);
				int32 denominatorPower = quotientExponent;
				quotientExponent = 8 + numeratorPower - denominatorPower;
				return;
			}

			if (digitsAdded == 0) {
				// This shouldn't happen, but check in case shiftUpNumerator() gets called twice.
				assert(0);
				return;
			}

			// Shift up quotient an equivalent number of digits by just appending zeros.
			size_t i = quotientDigits.size();
			size_t n = i + digitsAdded;
			quotientDigits.setSize(n);
			for (; i != n; ++i) {
				quotientDigits[i] = 0;
			}
		}

		bool computeMoreDigits() {
			if (numerator.size() == 0) {
				// No numerator left, so all future digits are zero.
				return false;
			}

			shiftUpNumerator();

			// Do the division for this iteration.
			// localQuotient is between floor(10^(8+9) / 10^10) and floor((10^(9+9) - 1) / (10^9)),
			// meaning it's in [10^7, 10^9 - 1], so it has either 8 or 9 digits.
			uint64 localNumerator = (uint64(numerator.last())*oneBillion) + uint64(numerator[numerator.size()-2]);
			uint32 localQuotient = uint32(localNumerator / simpleDenominatorRoundUp);

			// Add localQuotient to quotient.
			// Because of the guarantees below, we know that we never have to worry
			// about carries into previously computed digits, so we can just copy the digits.
			char digits[9];
			size_t firstDigit = base1BillionToDecimal(digits, localQuotient);
			for (size_t i = quotientDigits.size()-9+firstDigit, n = quotientDigits.size(); i != n; ++i, ++firstDigit) {
				quotientDigits[i] = digits[firstDigit];
			}

			// At this point, localQuotient is below the true (shifted) local quotient by less than 2:
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
			// If we check whether the remainder is at least denominator afterward,
			// we can reduce this difference to be less than 1.

			// Subtract localQuotient*denominator from numerator.
			uint32 carry = 0;
			for (size_t i = 0, n = denominator.size(); i < n; ++i) {
				uint64 product = uint64(denominator[i])*uint64(localQuotient) + uint64(carry);
				uint32 lowerPart = uint32(product % oneBillion);
				carry = uint32(product / oneBillion);
				uint32 origInteger = numerator[i];
				uint32 newInteger = origInteger - lowerPart;
				if (origInteger < lowerPart) {
					newInteger += oneBillion;
					++carry;
				}
				numerator[i] = newInteger;
			}
			assert(carry == 0);

			// If numerator is still greater than or equal to denominator,
			// subtract denominator from integer and add 1 to quotient.
			// This allows for robust handling of the case where integer is
			// an exact multiple of denominator, but not of simpleDenominatorRoundUp,
			// and round half to even behaviour rounding up comes into play.
			bool isIntegerLessThanDenominator = false;
			for (size_t i = denominator.size(); i > 0; ) {
				--i;
				isIntegerLessThanDenominator = numerator[i] < denominator[i];
				if (numerator[i] != denominator[i]) {
					break;
				}
			}
			if (!isIntegerLessThanDenominator) {
				// There's room to subtract another denominator from integer,
				// so do that and add 1 to quotient.
				carry = 0;
				for (size_t i = 0, n = denominator.size(); i < n; ++i) {
					uint32 sum = denominator[i] + carry;
					carry = 0;
					if (sum == oneBillion) {
						// Zero can't affect this block, so continue to next.
						//sum = 0;
						carry = 1;
						continue;
					}
					uint32 origInteger = numerator[i];
					uint32 newInteger = origInteger - sum;
					if (origInteger < sum) {
						newInteger += oneBillion;
						++carry;
					}
					numerator[i] = newInteger;
				}
				assert(carry == 0);

				// NOTE: We don't need to worry about this going past the
				// current span of digits in quotientDigits, since
				// this will still result in a local quotient at most (10^9 - 1),
				// but it may add a nonzero digit, from 8 digits to 9 digits,
				// since localQuotient could have been exactly (10^8 - 1).
				for (size_t i = quotientDigits.size(); i > 0; ) {
					--i;
					++quotientDigits[i];
					if (quotientDigits[i] != 10) {
						break;
					}
					quotientDigits[i] = 0;
				}
			}

			if (quotientDigits.size() == 9 && quotientDigits[0] == 0) {
				// High digit is zero, so subtract 1 from quotientExponent
				// and shift quotientDigits over by 1.
				for (size_t i = 0; i < 8; ++i) {
					quotientDigits[i] = quotientDigits[i+1];
				}
				quotientDigits.setSize(8);
				--quotientExponent;
			}

			// Remove zeros at the top of numerator.
			while (numerator.size() > 0 && numerator.last() == 0) {
				numerator.setSize(numerator.size()-1);
			}

			return true;
		}

		char operator[](int32 digiti) {
			digiti -= quotientExponent;
			while (digiti >= quotientDigits.size()) {
				bool success = computeMoreDigits();
				if (!success) {
					// No more nonzero digits.
					return 0;
				}
			}
			return quotientDigits[digiti];
		}
	};
	DeferredDecimalFraction midDigits(midInteger, denominator, simpleDenominatorRoundUp, denominatorPower);
	DeferredDecimalFraction smallerDigits(smallerInteger, denominator, simpleDenominatorRoundUp, denominatorPower);
	DeferredDecimalFraction largerDigits(largerInteger, denominator, simpleDenominatorRoundUp, denominatorPower);
	assert(0);
	// FIXME: Implement this!!!
}

void floatToText(float value, Array<char>& text) {
	doubleToTextWithPrecision(double(value), 24, text);
}

void doubleToText(double value, Array<char>& text) {
	doubleToTextWithPrecision(double(value), 53, text);
}

} // namespace text
OUTER_NAMESPACE_END
