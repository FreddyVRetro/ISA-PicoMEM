/* Copyright (C) 2023 Freddy VETELE

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

/* dev_irq.cpp : PicoMEM IRQ controller ports trap
 */ 

#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_defines.h"
#include "dev_picomem_io.h"


void dev_irq_install()
{
 PM_INFO("Install IRQ Spy Device\n");
 SetPortType(0x20,DEV_IRQ,1);
}

void dev_irq_remove()
{
 DelPortType(DEV_IRQ);
}

void dev_irq_update()
{
}

void dev_irq_iow(uint32_t Addr,uint8_t Data)
{
 switch (Addr & 0x03)
    {
     case 0: //PM_INFO ("$");
             break;
     case 1: PM_INFO ("p21:%X ",Data);   // Port 21h IRQ 
             if (Data&0x80) PM_INFO("DIQ7 ");
             break;
 /*    case 2:PM_INFO ("p22:%X ",Data);   // Port 22h
            break;
     case 3:PM_INFO ("p23:%X ",Data);   // Port 23h
            break;                       
 */            
    }
}