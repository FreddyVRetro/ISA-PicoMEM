#pragma once

#define EMS_PORT_NB 5
#define EMS_ADDR_NB 2
const uint8_t EMS_Port_List[EMS_PORT_NB]={0,0x268>>3,0x288>>3,0x298>>3,0x2A8>>3}; // 278 used by LPT
const uint8_t EMS_Addr_List[EMS_ADDR_NB]={0xD0000>>14,0xE0000>>14};

#define MT_Shift 13
#define MEM_BlockSize 8*1024
#define MEM_S_16k     2			// Nb of MEM Block for 16k

// **  Memory Types, used as index number as well (Except for MEM_NULL)
// Don't change the MEM Type/Index values
// 1 to 15 : No Wait State (Need to be in internal SRAM)
// 16 +    : Wait State    (For RAM/ROM in PSRAM)

#define MEM_NULL   0   // No PicoMEM emulation in this address

#define MEM_RAM    1   // RAM in the Pico SRAM    RW / ZWS  !! Change Disk code if modified !!
#define MEM_DISK   2   // BIOS RAM/DISK           RW / ZWS

#define MEM_BIOS   8   // PicoMEM BIOS ROM        R  / ZWS
#define MEM_ROM0   9   // ROM in Segment C	      R  / ZSW > not used for the moment
#define MEM_ROM1   10  // ROM in Segment D	      R  / ZWS > not used for the moment

#define MEM_BIOS_EXT 12

// Add 15, as NULL, to have 1 Cycle less in Memory Read

#define MEM_PSRAM  16   // RAM in the PSRAM       RW / WS (SPI and QSPI need WS)
#define MEM_EMS    17   // EMS in the PSRAM			  RW / WS

// Following Type used only in the BIOS and Config Table
#define SMEM_RAM   32  // System RAM
#define SMEM_VID   33  // Video RAM
#define SMEM_ROM   34  // System ROM
/* 16  : System RAM
   17  : Video RAM
   18  : ROM        */

#define MEM_I_NULL   0
#define MEM_I_PSRAM  16   // RAM in the PSRAM       WS (SPI and QSPI need WS)

#ifdef PIMORONI_PICO_PLUS2_RP2350
#define MEM_I_EMS0 17   // EMS in the PSRAM			  WS
#define MEM_I_EMS1 18   // EMS in the PSRAM			  WS
#define MEM_I_EMS2 19   // EMS in the PSRAM			  WS
#define MEM_I_EMS3 20   // EMS in the PSRAM			  WS
#endif

// Memory index Types  :
// MIT_NONE     : 
// MIT_RW_NOWS  : Last index Read/Write, No Wait State
// MIT_R_NOWS   : ROM with no Wait State
// MIT_R_WS     : ROM with Wait State
#define MIT_NONE    0   // Nothing
#define MIT_RW_NOWS 1   // RAM with no Wait State (In Pico RAM)
#define MIT_R_NOWS  2   // ROM with no Wait State (In Pico RAM)
#define MIT_W_NOWS  3   // Memory capture         (In Pico RAM)
#define MIT_R_WS    4   // ROM with Wait State    (In Pico Flash)
#define MIT_RW_WS   5   // RAM with Wait State    (In PSRAM)
 
// Memory index Begin/End  :
// MI_NONE        : Nothing
// MI_W_NOWS_END  : Last index Write with no Wait State
// MI_R_NOWS_END  : ROM with no Wait State
// MI_R_WS_END    : ROM with Wait State

#define MI_W_NOWS_END  4
#define MI_R_NOWS_END  7
#define MI_R_WS_END    15

extern volatile uint8_t *RAM_Offset_Table[21];
extern volatile uint16_t PM_BIOSADDRESS;

extern void display_memmap();
extern void display_memindex();

extern void dev_memory_init(uint32_t biosaddr);
extern uint8_t dev_memory_install(uint8_t MEM_Type, uint32_t Pico_Offset);
extern bool dev_ems_install(uint16_t port, uint32_t base_addr);

extern void dev_memory_remove_ram();
extern void dev_memorytype_remove(uint8_t type);

bool dev_ems_ior(uint32_t CTRL_AL8,uint8_t *Data );
extern void dev_ems_iow(uint32_t CTRL_AL8,uint8_t Data);

__force_inline uint8_t GetMEMType(uint32_t Address)
#if MUX_V2  
{ return MEM_Type_T[(Address^(0x00F00))>>MT_Shift]; }
#else
{ return MEM_Type_T[(Address^(0xF0000))>>MT_Shift]; }
#endif

// ! Not used by the ISA RAM Code, as it need the Mask added !
__force_inline uint8_t GetMEMIndex(uint32_t Address)
#if MUX_V2  
{ return MEM_Index_T[(Address^(0x00F00))>>MT_Shift]; }
#else
{ return MEM_Index_T[(Address^(0xF0000))>>MT_Shift]; }
#endif

// Set "Size" consecutive block to "Type" in the Memory type table
__force_inline void SetMEMType(uint32_t Address,uint8_t Type, uint8_t Size)
{ 	
#if MUX_V2
  for (int i=0;i<Size;i++) MEM_Type_T[(((Address+i*MEM_BlockSize)^0x00F00)>>MT_Shift)] = Type;
#else
  for (int i=0;i<Size;i++) MEM_Type_T[(((Address+i*MEM_BlockSize)^0xF0000)>>MT_Shift)] = Type;
#endif
}

__force_inline void SetMEMIndex(uint32_t Address,uint8_t Type, uint8_t Size)
{ 	
#if MUX_V2  
  for (int i=0;i<Size;i++) MEM_Index_T[(((Address+i*MEM_BlockSize)^0x00F00)>>MT_Shift)] = Type;
#else
  for (int i=0;i<Size;i++) MEM_Index_T[(((Address+i*MEM_BlockSize)^0xF0000)>>MT_Shift)] = Type;
#endif
}
