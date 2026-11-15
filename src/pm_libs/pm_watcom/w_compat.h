#pragma once
/* w_compat.h : Minimal compatibility shim for the Open Watcom headers
 * (DOS.H / BIOS.H / DOSFUNC.H) reused here to define the PicoMEM-side
 * "Watcom C runtime" call shims (w_bios.cpp).
 *
 * The real Watcom headers rely on <_comdef.h> and <i86.h> (compiler
 * internal headers, not shipped with the Open Watcom include set we
 * copied) for the _WCRTLINK / _WCINTERRUPT / _WCFAR / _WCI86FAR macros
 * and for `union REGS` / `struct SREGS`. Neither exists in this project
 * and we are not compiling with the Watcom compiler (this builds for
 * the RP2040/RP2350 with GCC), so provide harmless stand-ins:
 *  - the linkage/calling-convention macros become empty (GCC/ARM has no
 *    equivalent and doesn't need one),
 *  - `union REGS` / `struct SREGS` are only referenced by intdos()/
 *    intdosx(), which DOS.H itself only declares when _M_IX86 is
 *    defined (real-mode x86 target) -- never true here, so they are
 *    unused, but the types are still provided for completeness.
 */

#define _COMDEF_H_INCLUDED
#define _I86_H_INCLUDED   // prevents DOS.H from trying to #include <i86.h>

#define _WCRTLINK
#define _WCINTERRUPT
#define _WCFAR
#define _WCI86FAR

union REGS {
    struct { unsigned char al, ah, bl, bh, cl, ch, dl, dh; } h;
    struct { unsigned short ax, bx, cx, dx, si, di, cflag, flags; } x;
};

struct SREGS {
    unsigned short es, cs, ss, ds;
};
