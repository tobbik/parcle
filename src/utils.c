/* vim: ts=4 sw=4 sts=4 sta tw=80 list
 *
 * Some utility functions, special implementation instead of including
 * libraries
 *
 */

#include "utils.h"

/* a fast pow(2,x) that returns only integers */
int pow2(char exponent)
{
	// notice how the range checking is already done on the exponent var 
	static union IeeeFloatUnion u;
	u.f = 2.0;
	// Change the exponent part of the float
	u.brokenOut.exponent += (exponent - 1);
	return (int) (u.f);
}

