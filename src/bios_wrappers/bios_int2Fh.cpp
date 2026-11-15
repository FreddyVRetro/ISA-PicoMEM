#include "bios_int2Fh.h"

#include "pm_debug.h"
#include "pm_defines.h"
#include "pm_gvars.h"

extern volatile uint8_t PM_Status;

// DOS INT 2Fh tracing observes redirector calls through the BIOS register image.
void bios_int2Fh_execute()
{
  PM_INFO(".NR.......... Int2Fh NW Redir ...........NR.\n");
  if (reg_ah == 0x11) {
    switch (reg_al) {
      case 0x00: PM_INFO(". NR - MSCDEX INSTALLATION CHECK\n"); break;
      case 0x11: PM_INFO(". NR - RENAME REMOTE FILE (AH:%X)\n", reg_ah); break;
      case 0x13: PM_INFO(". NR - DELETE REMOTE FILE (AH:%X)\n", reg_ah); break;
      case 0x16: PM_INFO(". NR - OPEN EXISTING REMOTE FILE (AH:%X)\n", reg_ah); break;
      case 0x17: PM_INFO(". NR - CREATE OR TRUNCATE REMOTE FILE (AH:%X)\n", reg_ah); break;
      case 0x18: PM_INFO(". NR - CREATE/TRUNCATE FILE WITHOUT CDS (AH:%X)\n", reg_ah); break;
      case 0x19: PM_INFO(". NR - FIND FIRST FILE WITHOUT CDS (AH:%X)\n", reg_ah); break;
      case 0x1A: PM_INFO(". NR - FIND NEXT FILE WITHOUT CDS (AH:%X)\n", reg_ah); break;
      case 0x1B: PM_INFO(". NR - FINDFIRST (AH:%X)\n", reg_ah); break;
      case 0x22: PM_INFO(". NR - PROCESS TERMINATION HOOK (AH:%x)\n", reg_ah); break;
      case 0x23: PM_INFO(". NR - QUALIFY REMOTE FILENAME (AH:%x)\n", reg_ah); break;
      default:
        PM_INFO(". AH:%x AL:%X BX:%X, CX:%X, DX:%X\n",
                reg_ah, reg_al, (uint16_t)reg_bl, (uint16_t)reg_cl, (uint16_t)reg_dl);
        break;
    }
  }

  PM_Status = STAT_READY;
}
