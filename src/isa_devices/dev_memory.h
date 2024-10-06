#pragma once

#define MT_Shift 13
#define MEM_BlockSize 8*1024
#define MEM_S_16k     2			// Nb of MEM Block for 16k

//  *  Memory Types
// Don't change the MEM Type values, MEM_PSRAM and MEM_EMS must be > MEM_RAM
//1 to 7  : No Wait State Internal RAM
//8 to 15 : Wait State, Internal RAM or ROM
//16 + PSRAM

#define MEM_NULL   0   // No PicoMEM emulation in this address
#define MEM_RAM    1   // in the Pico RAM          WS or not (if >>1)  !! Change Disk code if modified !!
#define MEM_DISK   2   // Disk Buffer /BIOS RAM    WS or not (if >>2)
// Add MEM_DEV Type (RAM for emulated Devices)

#define MEM_BIOS   8   // PicoMEM BIOS ROM and DP RAM
#define MEM_ROM0   9   // ROM in Segment C	      WS
#define MEM_ROM1   10  // ROM in Segment D	      WS

#define MEM_PSRAM  16   // in the PSRAM				    WS
#define MEM_EMS    17   // EMS in the PSRAM			  WS

// Following Type used only in the BIOS and Config Table
#define SMEM_RAM   32  // System RAM
#define SMEM_VID   33  // Video RAM
#define SMEM_ROM   34  // System ROM
/* 16  : System RAM
   17  : Video RAM
   18 : ROM        */

// Memory index Types  :
// MIT_NONE     : 
// MIT_RW_NOWS  : Last index Read/Write, No Wait State
// MIT_R_NOWS   : ROM with no Wait State
// MIT_R_WS     : ROM with Wait State
#define MIT_NONE    0   // Nothing
#define MIT_RW_NOWS 1   // RAM with no Wait State (In Pico RAM)
#define MIT_R_NOWS  2   // ROM with no Wait State (In Pico RAM)
#define MIT_R_WS    3   // ROM with Wait State    (In Pico Flash)
#define MIT_RW_WS   4   // RAM with Wait State    (In External PSRAM)
 
// Memory index Begin/End  :
// MI_NONE        : Nothing
// MI_RW_NOWS_END : Last index Read/Write, No Wait State
// MI_R_NOWS_END  : ROM with no Wait State
// MI_R_WS_END    : ROM with Wait State

#define MI_RW_NOWS_END 4
#define MI_R_NOWS_END  7
#define MI_R_WS_END    15

// Temp (HardCoded)
#define MEM_RAM_I    1   // in the Pico RAM          
#define MEM_DISK_I   2   // Disk Buffer /BIOS RAM    
#define MEM_BIOS_I   8   // PicoMEM BIOS ROM and DP RAM

extern volatile uint8_t *RAM_Offset_Table[16];
extern volatile uint16_t PM_BIOSADDRESS;

extern void display_memmap();
extern void display_memindex();
extern uint8_t dev_memory_install(uint8_t MEM_Type, uint32_t Pico_Offset);
extern void dev_memory_remove_ram();
extern void dev_memorytype_remove(uint8_t type);

__force_inline uint8_t GetMEMType(uint32_t Address)
{ return MEM_Type_T[(Address^(0xF0000))>>MT_Shift]; }

// ! Not used by the ISA RAM Code, as it need the Mask added !
__force_inline uint8_t GetMEMIndex(uint32_t Address)
{ return MEM_Index_T[(Address^(0xF0000))>>MT_Shift]; }

// Set "Size" consecutive block to "Type" in the Memory type table
__force_inline void SetMEMType(uint32_t Address,uint8_t Type, uint8_t Size)
{ 	
  for (int i=0;i<Size;i++) MEM_Type_T[(((Address+i*MEM_BlockSize)^0xF0000)>>MT_Shift)] = Type;
}

__force_inline void SetMEMIndex(uint32_t Address,uint8_t Type, uint8_t Size)
{ 	
  for (int i=0;i<Size;i++) MEM_Index_T[(((Address+i*MEM_BlockSize)^0xF0000)>>MT_Shift)] = Type;
}