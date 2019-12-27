#pragma once

#include "../Types.h"

#include <math.h>

OUTER_NAMESPACE_BEGIN
namespace math {

// Finds solutions to ax^2 + bx + c = 0, returning the number of solutions,
// filling in x0 if one solution is found, and both x0 and x1 if two solutions are found.
// It will err on the side of trying to avoid NaNs or infinities, so may not output
// solutions around 2^62 (~4.6e+18) or larger.
constexpr inline size_t solveQuadratic(double a, double b, double c, double& x0, double& x1) {
	const double ac = a*c;
	const double absac = (ac < 0) ? -ac : ac;
	const double b2 = b*b;
	const double absb2 = (b2 < 0) ? -b2 : b2;
	if (0x1.0p30*absac < absb2) {
		// The sqrt can be well approximated by a truncated Taylor series.
		// The two candidate solutions are:
		// x0 = -(c/b)*(1 + ac/b^2 + 2(ac/b^2)^2 + ...)
		// x1 = x0 - b/a
		// NOTE: If c is very small (x0 ~ 0) but a isn't, x1 may still matter!
		const double c_b = c/b;
		const double ac_b2 = ac/b2;
		x0 = -c_b*(1 + ac_b2 + 2*ac_b2*ac_b2);

		// NOTE: This check is not relative, (a and b have different units),
		// but it is necessary, due to the potential for a to be very small.
		const double absa = (a < 0) ? -a : a;
		const double absb = (b < 0) ? -b : b;
		if (0x1.0p62*absa <= absb) {
			return 1;
		}

		x1 = x0 - b/a;
		return 2;
	}

	const double discriminant = b2 - 4*ac;
	// Within this threshold, the sqrt will yield a scale change of about 1 millionth,
	// (or will yield no solution), so to account for roundoff, we'll consider
	// it to be one solution.
	const double singleSolutionThreshold = 0x1.0p-40*b2;
	// Condition is written so that any NaNs will yield no solutions.
	if (!(discriminant >= -singleSolutionThreshold)) {
		return 0;
	}

	// a could still be problematically small if b is also quite small,
	// since comparison above squared b, but didn't square a.
	// Both could also be zero, since comparison was for strictly less than.
	const double absa = (a < 0) ? -a : a;
	const double absb = (b < 0) ? -b : b;
	if (0x1.0p63*absa <= absb) {
		// Fall back to bx + c = 0 --> x = -c/b,
		// unless b is also problematically small.
		const double absc = (c < 0) ? -c : c;
		if (0x1.0p62*absb <= absc) {
			return 0;
		}
		x0 = -c/b;
		return 1;
	}

	if (discriminant <= singleSolutionThreshold) {
		x0 = -b/(2*a);
		return 1;
	}

	const double half = sqrt(discriminant);
	const double recip2a = 1.0/(2*a);
	x0 = (-b - half)*recip2a;
	x1 = (-b + half)*recip2a;
	return 2;
}

} // namespace math
OUTER_NAMESPACE_END
