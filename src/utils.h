/* vim: ts=4 sw=4 sts=4 sta tw=80 list
 *
 * Some utility functions, special implementation instead of including
 * libraries
 *
 */
struct IeeeFloat {
	unsigned int base : 23;
	unsigned int exponent : 8;
	unsigned int signBit : 1;
};


union IeeeFloatUnion {
	struct IeeeFloat brokenOut;
	float f;
};

int pow2(char exponent);

