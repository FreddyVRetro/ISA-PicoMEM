/* w_bios.cpp : Watcom C runtime BIOS (INT 13h) call shims for the PicoMEM.
 *
 * See w_dos.cpp for the INT 21h (DOS) call shims and the shared-buffer
 * mechanism used to pass strings/data to the PC.
 *
 * These functions have the exact signatures of the Open Watcom BIOS.H
 * runtime calls, but instead of trapping to a local INT handler (there
 * is none -- this code runs on the RP2040/RP2350, not on the PC's x86
 * CPU) they fill in the shared register block (reg_ax, reg_bl, ... see
 * pm_defines.h) and forward the call to the real PC through the
 * PC-command protocol (pm_pccmd.*):
 *   - PC_Int13() -> PCC_IRQ13 -> the PMBIOS ROM on the PC runs "INT 13h"
 * (see pmbios/pm_cmd.asm : PCC_Do_IRQ13). Only INT 13h/21h are wired
 * generically; there is no PC command to run an arbitrary INT, so BIOS
 * calls other than INT 13h (disk) have no way to reach the PC and are
 * left as documented stubs below.
 */

#include <stdio.h>
#include "pico/stdlib.h"

#include "BIOS.H"

#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "../pm_pccmd.h"

/*
AH = Command
AL = Sectors count (0 is illegal) -- cannot cross ES page boundary, or a cylinder boundary, and must be < 128
CH = cylinder & 0xff
CL = Sector | ((cylinder >> 2) & 0xC0);
DH = Head -- may include two more cylinder bits
ES:BX -> buffer
DL = Drive number
*/

// ===========================================================================
// BIOS.H
// ===========================================================================

// __diskinfo->buffer is a 32bit packed far pointer: high 16 bits = segment,
// low 16 bits = offset (there is no native "segment:offset" C type here).
unsigned short _bios_disk(unsigned __cmd, struct diskinfo_t *__diskinfo)
{
  uint32_t far_ptr = (uint32_t)(uintptr_t) __diskinfo->buffer;

  reg_ah = (uint8_t) __cmd;
  reg_dl = (uint8_t) __diskinfo->drive;
  reg_al = (uint8_t) __diskinfo->nsectors;
  reg_ch = (uint8_t) __diskinfo->track;
  reg_cl = (uint8_t)(__diskinfo->sector & 0x3F) | (uint8_t)((__diskinfo->track >> 2) & 0xC0);
  reg_dh = (uint8_t) __diskinfo->head;
  reg_bl = (uint8_t)(far_ptr & 0xFF);
  reg_bh = (uint8_t)((far_ptr >> 8) & 0xFF);
  reg_esl = (uint8_t)((far_ptr >> 16) & 0xFF);
  reg_esh = (uint8_t)((far_ptr >> 24) & 0xFF);

  PC_Int13();

  return reg_ax;   // AH = BIOS disk status (0 = Ok), matches _bios_disk()'s documented return value
}

// Equipment word, read straight from the BIOS Data Area (0040:0010 -> linear 0x410).
// Fixed BDA location, independent of where the PMBIOS ROM itself is mapped.
unsigned short _bios_equiplist(void)
{
  return PC_MR16(0x410);
}

// Memory size in Kb, from the BDA (0040:0013 -> linear 0x413).
unsigned short _bios_memsize(void)
{
  return PC_MR16(0x413);
}

// Only READ/READY are meaningful: PCC_Do_KeyPressed (PC side) discards the
// AX result of "INT 16h AH=01h" and only reports a hit/no-hit flag, so a
// "ready" check here can't return the peeked key without also consuming it.
unsigned short _bios_keybrd(unsigned __cmd)
{
  switch (__cmd)
  {
    case _KEYBRD_READ:
    case _NKEYBRD_READ:
    {
      char ascii;
      uint8_t scan = PC_ReadKey(&ascii);
      return ((uint16_t)scan << 8) | (uint8_t)ascii;
    }
    case _KEYBRD_READY:
    case _NKEYBRD_READY:
      return PC_KeyPressed() ? 1 : 0;   // see limitation above (no code, just "a key is waiting")

    case _KEYBRD_SHIFTSTATUS:
    case _NKEYBRD_SHIFTSTATUS:
      return (PC_MR16(0x416) >> 8) & 0xFF;   // BDA keyboard shift-flags byte @0040:0017

    default:
      PM_WARNING("_bios_keybrd: unsupported command %u\n", __cmd);
      return 0;
  }
}

// INT 17h (printer), INT 14h (serial) and INT 1Ah (time-of-day) have no
// generic PC-command path (only INT 13h/21h are wired, see file header) --
// left as documented stubs rather than silently doing the wrong thing.
unsigned short _bios_printer(unsigned __cmd, unsigned __port, unsigned __data)
{
  (void)__cmd; (void)__port; (void)__data;
  PM_WARNING("_bios_printer: not supported (no INT 17h path in the PC-command protocol)\n");
  return 0;
}

unsigned short _bios_serialcom(unsigned __cmd, unsigned __port, unsigned __data)
{
  (void)__cmd; (void)__port; (void)__data;
  PM_WARNING("_bios_serialcom: not supported (no INT 14h path in the PC-command protocol)\n");
  return 0;
}

unsigned short _bios_timeofday(unsigned __cmd, long *__timeval)
{
  (void)__cmd; (void)__timeval;
  PM_WARNING("_bios_timeofday: not supported (no INT 1Ah path in the PC-command protocol)\n");
  return 0;
}