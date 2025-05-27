/* Copyright (C) 2024 Freddy VETELE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. 
If not, see <https://www.gnu.org/licenses/>.
*/

/* dev_cdrom.cpp : PicoMEM CD ROM device emulation
 Emulate a Panasonic, with 3MKE3 interface, and Sound Blaster ports : 
 https://en.wikipedia.org/wiki/Panasonic_CD_interface

*/

#if USE_CDROM
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType

extern "C" void MKE_WRITE(uint16_t address, uint8_t value);
extern "C" uint8_t MKE_READ(uint16_t address);
extern "C" void mke_init();

#define CDROM_NUM 1
#include "cdrom/cdrom.h"
cdrom_t *cdrom;

bool dev_cdr_active =false;         // True if configured
#define dev_cdr_baseport 0x0

uint8_t dev_cdr_install()
{
 if (!dev_cdr_active)   // Don't re enable if active
  {  
   PM_INFO("Install CD ROM (%x)\n",dev_cdr_baseport);

  cdrom = (cdrom_t *) calloc (1, sizeof (cdrom_t));
  if (cdrom == NULL)
     {
      PM_ERROR("CDROM: Can't allocate RAM (%d)\n",sizeof (cdrom_t));
      dev_cdr_active=false;
      return 1;
     }

   if (GetPortType(dev_cdr_baseport)!=DEV_NULL)
     {
      PM_ERROR("Port already used (%d)\n",GetPortType(dev_cdr_baseport));
      dev_cdr_active=false;
      return 1;
     }

   SetPortType(dev_cdr_baseport,DEV_CDR,1);
   dev_cdr_active=true;
 }
  return 0;
}

void dev_cdr_remove()
{
 if (dev_cdr_active)   // Don't stop if not active
  {  
   dev_cdr_active=false;  
   PM_INFO("Remove CD ROM (%X)\n",dev_cdr_baseport);

   if (cdrom) free(cdrom);  
   SetPortType(dev_cdr_baseport,DEV_NULL,2);
  }  
}

// Return true if CMS is installed
bool dev_cdr_installed()
{
  return (GetPortType(dev_cdr_baseport)==DEV_CDR);
}

// Started in the Main Command Wait Loop
void dev_cdr_update()
{ // Perform file open, read ...
  cdrom_tasks(cdrom);
}

bool dev_dma_ior(uint32_t CTRL_AL8,uint8_t *Data )
{
  if (dev_dma_emulate)
  {

    return false;
  } else return false;  // Nothing to read
}

void dev_cdr_iow(uint32_t CTRL_AL8,uint8_t Data)
{
  uint8_t addr=CTRL_AL8&0xFF;

  switch(addr)
      {
        case 0x0:

      }
 }
}

#endif