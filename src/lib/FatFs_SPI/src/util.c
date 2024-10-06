/*
 * util.c
 *
 *  Created on: Apr 12, 2022
 *      Author: carlk
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
//
#include "util.h"

int gcd(int a,int b) {
  int R;
  while ((a % b) > 0)  {
    R = a % b;
    a = b;
    b = R;
  }
  return b;
}

char const* uint8_binary_str(uint8_t number) {
	static char b[sizeof number * 8 + 1];

	memset(b, 0, sizeof b);
	for (size_t i = 0; i < sizeof number * 8; ++i) {
		unsigned int mask = 1 << i;
		if (number & mask)
			b[sizeof number * 8 - 1 - i] = '1';
		else
			b[sizeof number * 8 - 1 - i] = '0';
	}
	return b;
}
char const* uint_binary_str(unsigned int number) {
	static char b[sizeof number * 8 + 1];

	memset(b, 0, sizeof b);
	for (size_t i = 0; i < sizeof number * 8; ++i) {
		unsigned int mask = 1 << i;
		if (number & mask)
			b[sizeof number * 8 - 1 - i] = '1';
		else
			b[sizeof number * 8 - 1 - i] = '0';
	}
	return b;
}
