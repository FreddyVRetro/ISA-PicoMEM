#ifndef PMDOSBOX_H
#define PMDOSBOX_H

#include <cstdint>  //uint definition

#include <functional>
#include <assert.h>
#include <cstring>
#include <limits>

/*
 * Bit types
 */

typedef uintptr_t	Bitu;
typedef intptr_t	Bits;
typedef uint32_t	Bit32u;
typedef int32_t		Bit32s;
typedef uint16_t	Bit16u;
typedef int16_t		Bit16s;
typedef uint8_t		Bit8u;
typedef int8_t		Bit8s;

typedef Bit32u RealPt;

// in callback.h in DOSBOX
enum {	
	CBRET_NONE=0,CBRET_STOP=1
};

#endif