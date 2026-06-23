/*
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
/* ISA PicoMEM By Freddy VETELE
 * pm_defines.h
 */
#pragma once

// Devices 0 to 3 generate no IOCHRDY (Write only)
#define DEV_NULL   0   // No devide use this port
#define DEV_POST   1   // POST Code port
#define DEV_IRQ    2   // ! NoWS IRQ Controller IOW > Out 20h counter and Port 21h register
#define DEV_DMA    3   // DMA Controller IOW
#define DEV_PM     4   // WS PicoMEM Port
#define DEV_LTEMS  5   // WS Lotech EMS Emulation (2Mb)
#define DEV_NE2000 6
#define DEV_JOY    7
#define DEV_ADLIB  8
#define DEV_CMS    9
#define DEV_TANDY  10
#define DEV_SBDSP  11
#define DEV_RTC    12
#define DEV_MMB    13  // Mindscape
#define DEV_CDR    14  // CD ROM
#define DEV_CVX    15
#define DEV_MPU    16
#define DEV_GUS    17

#ifdef BOARD_WM10
#define DEV_MCA_POS 18
#define DEV_TOTAL  19  // Total number of devices
#else
#define DEV_TOTAL  18  // Total number of devices
#endif


// * Status and Commands definition
#define STAT_READY         0x00  // Ready to receive a command
#define STAT_CMDINPROGRESS 0x01
#define STAT_CMDERROR      0x02
#define STAT_CMDNOTFOUND   0x03  // Error : command not found
#define STAT_INIT          0x04  // Init in Progress
#define STAT_WAITCOM       0x05  // Wait for the USB Serial to be connected
#define STAT_DATA_READ     0x10
#define STAT_DATA_WRITE    0x11
#define STAT_DATA_READ_W   0x12
#define STAT_DATA_WRITE_W  0x13

#define CMDERR_NOERROR    0x00
#define CMDERR_WRITE      0x01  // Disk Write Error (File write)
#define CMDERR_IMGEXIST   0x02  // Can't create the image : Already exist
#define CMDERR_NOHDDDIR   0x03  // Directory HDD Missing
#define CMDERR_NOFDDDIR   0x04  // Directory FDD Missing
#define CMDERR_NOROMDIR   0x05  // Directory ROM Missing
#define CMDERR_DISKFULL   0x06
#define CMDERR_DISK       0x07  // Any other Disk Error
#define CMDERR_FILEREAD   0x08  // Can't open/Read file (image)
#define CMDERR_MOUNTFAIL  0x09  // Disk image mount fail
#define CMDERR_PORTUSED   0x10  // Port already used
#define CMDERR_MEM_ERR    0x11  // Not enaugh memory
#define CMDERR_AMIX_FAIL  0x12  // Audio mixer init fail
#define CMDERR_NOIRQ	  0x13  // No IRQ detected/Available
#define CMDERR_NOTSUPP    0xFF  // Command not supported

// 0x 1x General / Configuration Commands
#define CMD_Reset         0x00   // Put back the status to 0, after an error
#define CMD_PMIRQ_ACK     0x01   // PicoMEM multiplexed interrupt acknowledge
#define CMD_DMACPY_ACK    0x02   // DMA Copy Acknowledge
#define CMD_Reserved2     0x03
#define CMD_Reserved3     0x04   //
#define CMD_GetMEMType    0x05   // Read the type of Memory mapped to a memory range
#define CMD_SetMEMType    0x06   // Set the type of Memory mapped to a memory range
#define CMD_MEM_Init      0x07   // Configure the memory based on the Config table
#define CMD_SaveCfg       0x08   // Save the Configuration to the file
#define CMD_DEV_Init      0x09   // Initialize the devices (Run at the BIOS Setup end)
#define CMD_TDY_Init      0x0A   // Initialize the Tandy RAM emulation
#define CMD_Test_RAM      0x0B   // Write the sent value to the BIOS RAM Test Address, Return the value that was present
#define CMD_TESTIO        0x0C   // Test Data transfer via IO > To remove when done
#define CMD_GetBasePort   0x0D
#define CMD_SetBasePort   0x0E

#define CMD_GetPMString   0x10   // Read the PicoMEM Firmware string
#define CMD_ReadConfig    0x12   // Read the Config variables from I/O
#define CMD_WriteConfig   0x13   // Read the Config variables from I/O

//
#define CMD_SendIRQ       0x30  // Enable the IRQ
#define CMD_IRQAck        0x31  // Acknowledge IRQ
#define CMD_LockPort      0x37

// 4x Debug/Test CMD
#define CMD_SetMEM        0x42  // Set the First 64Kb of RAM to the SetRAMVal
#define CMD_TestMEM       0x43  // Compare the First 64Kb of RAM
#define CMD_PCINFO        0x44  // Get PC informations from PCCMD (And test PCCMD)
#define CMD_BIOS_MENU	  0x45  // A BIOS Menu/submenu is selected

// 5x USB devices commands
#define CMD_USB_Enable    0x50	// To replace by OnOff with parameter
#define CMD_USB_Disable   0x51
#define CMD_Mouse_Enable  0x52
#define CMD_Mouse_Disable 0x53
#define CMD_Joy_OnOff     0x54
#define CMD_Keyb_OnOff    0x55

// 6x Commands asking for various status the PicoMEM (Returns a string)
#define CMD_Wifi_Infos    0x60   // Get the Wifi Status, retry to connect if not connected
#define CMD_USB_Status    0x61   // Get the USB Status Strings
#define CMD_DISK_Status   0x62   // Get the Disk emulation status screen.

// 7x Audio commands
#define CMD_AudioOnOff    0x70  // Full audio rendering 0: Off 1:On

#define CMD_PS1OnOff      0x72  // PS1 Audio : 0 : Off 1: On default
#define CMD_AdlibOnOff    0x73  // Tandy Audio : 0 : Off 1: On default
#define CMD_TDYOnOff      0x74  // Tandy Audio : 0 : Off 1: On default or port
#define CMD_CMSOnOff      0x75  // CMS Audio   : 0 : Off 1: On default or port
#define CMD_CVXOnOff      0x76  // Covox Audio : 0 : Off 1: On default or port
#define CMD_SetSBIRQDMA   0x77  // Set Sound Blaster IRQ
#define CMD_SBOnOff       0x78  // Sound Blaster Audio : 0 : Off 1: On default or port
#define CMD_SetGUSIRQDMA  0x79  // Set GUS IRQ
#define CMD_GUSOnOff      0x7A  // GUS Audio : 0 : Off 1: On default or port
#define CMD_MMBOnOff      0x7B  // Mindscape Audio : 0 : Off 1: On default or port
#define CMD_DSSOnOff      0x7C  // Disney Audio
#define CMD_MPUOnOff      0x7D  // MPU/General MIDI : 0 : Off 1: On default or port


// 8x Disk Commands
#define CMD_HDD_Getlist   0x80  // Write the list of hdd images into the Disk memory buffer
#define CMD_HDD_Getinfos  0x81  // Write the hdd infos in the Disk Memory buffer
#define CMD_HDD_Mount     0x82  // Map disk images
#define CMD_FDD_Getlist   0x83  // Write the list of file name into the Disk memory buffer
#define CMD_FDD_Getinfos  0x84  // Write the disk infos to the Disk Memory buffer
#define CMD_FDD_Mount     0x85  // Map a floppy image
#define CMD_HDD_NewImage  0x86  // Create a new HDD Image
#define CMD_Image_Select  0x87  // Copy the selected image to the given drive config entry (HDD or FDD)

#define CMD_Int13h        0x88  // Emulate the BIOS Int13h
#define CMD_EthDFS_Send   0x89  // Send a packet to EthDFS server emulator and wait answer

#define CMD_Int21h        0x8A  //
#define CMD_Int21End      0x8B  //
#define CMD_Int2Fh        0x8C  //
#define CMD_Int2FEnd      0x8D  //

// Commands sent to the PicoMEM to send display to the Serial port (Not used / finished)
// https://github.com/nottwo/BasicTerm/blob/master/BasicTerm.cpp
#define CMD_S_Init         0x90  // Serial Output Init (Resolution) and clear Screen
#define CMD_S_PrintStr     0x91  // Print a Null terminated string from xxx
#define CMD_S_PrintChar    0x92  // Print a char (From RegL)
#define CMD_S_PrintHexB    0x93  // Print a Byte in Hexa
#define CMD_S_PrintHexW    0x94  // Print a Word in Hexa
#define CMD_S_PrintDec     0x95  // Print a Word in Decimal
#define CMD_S_GotoXY       0x96  // Change the cursor location (Should be in 80x25)
#define CMD_S_GetXY        0x97  // 
#define CMD_S_SetAtt       0x98
#define CMD_S_SetColor     0x99
#define CMD_StartUSBUART   0x9F  // Start the UART Display

#define CMD_DMA_TEST       0xAA

#define INIT_DONE          0x00
#define INIT_SKIPPED       0xFC  // Skipped or Error to initialize  ! Hardcoded in the ASM BIOS
#define INIT_INPROGRESS    0xFD
#define INIT_DISABLED      0xFF

// Shared MEMORY variables defines (Registers and PC Commands)
// The Shared Memory is used to exchange variables between the PicoMEM and the PC
// It is located just after the BIOS ROM (16KB) at PC Side
// and in the Pico SRAM in the PM_DPRAM table

// It is splitted in multiple part :
// - Shared BIOS Variables
// - Disk Parameter Table
// - Registers save area (For Int calls)
// - PicoMEM Config      (Saved to the SD Card)
// - PC Command variables
// - PicoMEM IRQ variables and parameters
// - PicoMEM IRQ parameters
// - PC Command parameters
// - BIOS internal variables (Not used by the Pico)
// - PicoMEM Disk Buffer (2KB)
// - PicoMEM DMA  Buffer (2KB)

// Size of the different sections
#define BV_SIZE    32
#define DPT_SIZE   32     // (2*16)
#define REGS_SIZE  18     // 9 registers saved
#define CONF_SIZE  768    // Configuration variables size
#define PCCMD_SIZE 4      // PC Command variables size

#define VARS_OFFS 0 // Offset of the Shared Memory variables in the PM_DP_RAM
#define DPT_OFFS       (VARS_OFFS + BV_SIZE)     // Offset of the Disk Parameter
#define DPT0_OFFS       DPT_OFFS                 // Disk Parameter Table 0
#define DPT1_OFFS      (VARS_OFFS + BV_SIZE+16)  // Disk Parameter Table 1
#define REGS_OFFS      (DPT_OFFS + DPT_SIZE)     // Offset of the Registers save area
#define CONF_OFFS      (REGS_OFFS + REGS_SIZE)   // Offset of the PicoMEM Config
#define PCCMD_OFFS     (CONF_OFFS + CONF_SIZE)   // Offset of the PC Commands and IRQ parameters
#define IRQ_VAR_OFFS   (PCCMD_OFFS + PCCMD_SIZE) // PicoMEM IRQ variables structure (14 bytes)
#define OFFS_IRQ_PARAM   IRQ_VAR_OFFS+14      // Offset of the IRQ Parameter structure (12 bytes)
#define OFFS_PCCMDPARAM  PCCMD_OFFS+36

#define BV_VARID       PM_DP_RAM[0]  // ID
#define BV_Test        PM_DP_RAM[1]  // Test
#define BV_InitCount   PM_DP_RAM[2]  // Nb of time the PicoMEM BIOS Start
#define BV_BootCount   PM_DP_RAM[3]  // Nb of time the PicoMEM BootStrap Start
#define BV_PSRAMInit   PM_DP_RAM[4]  // PSRAM Init Status Variable
#define BV_SDInit      PM_DP_RAM[5]  // SD Init Status Variable
#define BV_USBInit     PM_DP_RAM[6]  // BIOS RAM Offset of the USB Host Init Variable
#define BV_CFGInit     PM_DP_RAM[7]  // BIOS RAM Offset of the Config File Init Variable
#define BV_WifiInit    PM_DP_RAM[8]
#define BV_PortInit    PM_DP_RAM[9]  // PicoMEM Base Port initialisation  0FEh not initialized 0 Ok
#define BV_USBDevice   PM_DP_RAM[10] // Mounted USB Device Bit 0: Mouse Bit 1: Keyboard  Bit 3: Joystick ! Not used for the moment
#define BV_IRQ         PM_DP_RAM[11] // Detected Hardware IRQ Number
#define BV_HWIRQ       PM_DP_RAM[12] // Detected Hardware IRQ Number (Bit field for multiple IRQ)
#define BV_DMANoUpdate PM_DP_RAM[13] // Set to 1 when the DMA Code update the DMA registers (For the DMA emulation to ignore)
#define BV_Int13hCLI   PM_DP_RAM[14] //
#define BV_SPIPSRAM    PM_DP_RAM[15] // Set to 1 when SPI PSRAM is used (to stop IRQ during disk access)
#define BV_Fill2       PM_DP_RAM[16] // Status of the emulated DMA code (In the Interrupt)
#define PC_DiskNB      PM_DP_RAM[17] // Number of Disk found at the origin
#define PC_FloppyNB    PM_DP_RAM[18] // Number of Physical Floppy drive
#define PC_MEM         PM_DP_RAM[19] // PC RAM Size in Kb
#define PC_MEM_H       PM_DP_RAM[20] // PC RAM Size in Kb
#define New_DiskNB     PM_DP_RAM[21] // Number of Disk in total after HDD Mount
#define New_FloppyNB   PM_DP_RAM[22] // Number of Floppy in total after FDD Mount
#define BV_Tandy       PM_DP_RAM[23] // Tandy Mode
#define BV_TdyRAM      PM_DP_RAM[24] // Number of 16Kb Block of RAM to add for Tandy
#define BV_CmdRunning  PM_DP_RAM[25] // Command currently running in the Pico
#define BV_PCCMD_SOURCE PM_DP_RAM[26] // Contains the ID/number of the PicoMEM code starting/Using the PC Command
#define BV_BoardID    PM_DP_RAM[27] // PicoMEM Board ID (Type)
#define BV_PicoID     PM_DP_RAM[28] // Pico ID (Pico, Pico W, Pimoroni...)
#define BV_Fill3      PM_DP_RAM[29] //
#define BV_FW_Rev     PM_DP_RAM[30] // PicoMEM Firmware revision
#define BV_FW_Rev_H   PM_DP_RAM[31] // PicoMEM Firmware revision


// *** Registers PM_DP_RAM offset definitions ***
#define OFFS_AL REGS_OFFS
#define OFFS_AH REGS_OFFS +1
#define OFFS_BL REGS_OFFS +2
#define OFFS_BH REGS_OFFS +3
#define OFFS_CL REGS_OFFS +4
#define OFFS_CH REGS_OFFS +5
#define OFFS_DL REGS_OFFS +6
#define OFFS_DH REGS_OFFS +7
#define OFFS_DS REGS_OFFS +8
#define OFFS_SI REGS_OFFS +10
#define OFFS_ES REGS_OFFS +12
#define OFFS_ESL REGS_OFFS+12
#define OFFS_ESH REGS_OFFS+13
#define OFFS_DI REGS_OFFS +14
#define OFFS_CF REGS_OFFS +16

#define reg_ax (uint16_t) ((uint16_t)(PM_DP_RAM[OFFS_AH]<<8)+(uint16_t)PM_DP_RAM[OFFS_AL])
#define reg_al PM_DP_RAM[OFFS_AL]
#define reg_ah PM_DP_RAM[OFFS_AH]
#define reg_bx (uint16_t) ((uint16_t)(PM_DP_RAM[OFFS_BH]<<8)+(uint16_t)PM_DP_RAM[OFFS_BL])
#define reg_bl PM_DP_RAM[OFFS_BL]
#define reg_bh PM_DP_RAM[OFFS_BH]
#define reg_cl PM_DP_RAM[OFFS_CL]
#define reg_ch PM_DP_RAM[OFFS_CH]
#define reg_dl PM_DP_RAM[OFFS_DL]
#define reg_dh PM_DP_RAM[OFFS_DH]
#define reg_esl PM_DP_RAM[OFFS_ESL]
#define reg_esh PM_DP_RAM[OFFS_ESH]
#define reg_es (uint16_t) ((uint16_t)(PM_DP_RAM[OFFS_ESH]<<8)+(uint16_t)PM_DP_RAM[OFFS_ESL])
#define reg_flagl PM_DP_RAM[OFFS_CF]

// PC Commands "Registers"
#define PCCR_PCSTATE  PM_DP_RAM[PCCMD_OFFS]
#define PCCR_CMD      PM_DP_RAM[PCCMD_OFFS+1]  
#define PCCR_PMSTATE  PM_DP_RAM[PCCMD_OFFS+2]  // Set to 1 when a command is sent
#define PCCR_CMD_RES  PM_DP_RAM[PCCMD_OFFS+3]  // Result of the command (Not used)
// [3] Reserved

#define PCCR_Param   PM_DP_RAM[OFFS_PCCMDPARAM] //PC Commands parameters/memory zone
// 2KB is reserved for the Commands parameters RAM

#define PM_DB_Offset   4*1024  // Offset of the Disk Buffer start in the BIOS/DP RAM
#define PM_DB_Size     2*1024  // Size of the 'Disk Buffer in the BIOS/DP RAM

#define PM_ETHDFS_BUFF PM_DP_RAM[PM_DB_Offset-60]  // EtherDFS buffer (simulated packet) start at Disk buffer -60

// Memory emulation defines
#define MEM_T_Size 128

//const char *PM_Models[] = {"PicoMEM Proto", "PicoMEM 1", "PicoMEM LP", "PicoMEM 1.3", "PicoMEM 1.4", "","","","","PicoMEM 1.5", "PicoMEM 2"};

struct hostio_t {
	volatile bool     Updated;
	volatile uint32_t Port20hCount;   	// Incremented each time a write is done to Port 20h
	volatile uint8_t  Port21h;			// IRQ Mask register
    volatile bool 	  Port21h_Updated;  // IRQ Mask register updates
	volatile uint8_t  Port80h;			// BIOS Post Code (80h)
    volatile bool 	  Port80h_Updated;
	volatile uint8_t  Port81h;			// PicoMEM Post Code (81h)
    volatile bool 	  Port81h_Updated;  // Post Code Previous Value
	volatile uint8_t  PortPost2;		// 2nd Post Code port (Custom)
    volatile bool 	  PortPost2_Updated;
	volatile uint16_t Post2Port;
};

typedef struct pm_reg {  // Not realy used for the moment
	uint8_t Reg0;
	uint8_t Reg1;
	uint8_t PSRAM_Status;   // 4
	uint8_t SD_Status;      // 5
	uint8_t CMD_Number;     // Currently executing command number
	uint8_t CMD_Step;       // Currenlty executins Sub Step number (For Debug)
	uint8_t CMD_LastErr;    // Last Command Error
} pm_reg_t;

// Size 0FFFEh: Folder
//      0FFFFh: Name error
typedef struct FSizeName_t {
	uint16_t size;
	char name[14];
} FSizeName_t;

// PicoMEM BIOS variables table, defined also in the BIOS
// ! Be carefull, Word must be aligned
typedef struct BIOSVAR_t {
	uint8_t VARID;
	uint8_t Test;			// Test
	uint8_t InitCount;		// Nb of time the PicoMEM BIOS Start
	uint8_t BootCount;		// Nb of time the PicoMEM BootStrap Start
	uint8_t PSRAMInit;		// BIOS RAM Offset of the PSRAM Init Variable
	uint8_t SDInit;			// BIOS RAM Offset of the SD Init Variable
	uint8_t USBInit;		// BIOS RAM Offset of the USB Host Init Variable
	uint8_t CFGInit;		// BIOS RAM Offset of the Config File Init Variable
	uint8_t WifiInit;
	uint8_t PortInit;		// PicoMEM Base Port initialisation  0FEh not initialized 0 Ok
	uint8_t USBDevice;		// Mounted USB Device Bit 0: Mouse Bit 1: Keyboard  Bit 3: Joystick ! Not used for the moment
	uint8_t IRQ;			// Detected Hardware IRQ Number
	uint8_t IRQSource;		// Source of the Hardware Interrupt, Data stored in IRQ_Param (Cleared by the PC when acknowledge)
	uint8_t IRQ_Cnt;		// Incremented at the Stard, decremented at the end (To detect if an IRQ is in progress)
	uint8_t IRQStatus;		// Status of the IRQ
	uint8_t IRQArg;			// Argument for the IRQ (Like the SW IRQ number to call)
	uint8_t DMAStatus;		// Status of the emulated DMA code (In the Interrupt)
	uint8_t DiskNB;			// Number of Disk found at the origin
	uint8_t FloppyNB;		// Number of Physical Floppy drive
	uint16_t MEM;			// PC RAM Size in Kb

	uint8_t N_DiskNB;		// Number of Disk in total after HDD Mount
	uint8_t N_FloppyNB;	    // Number of Floppy in total after FDD Mount
	uint8_t Tandy;			// Tandy Mode
	uint8_t TdyRAM;			// Number of 16Kb Block of RAM to add for Tandy
	uint8_t CmdRunning;		// Command currently running in the Pico
	uint8_t PCCMD_SOURCE;	// Contains the ID/number of the PicoMEM code starting/Using the PC Command
	uint8_t BoardID;		// PicoMEM Board ID (Type)
	uint8_t PicoID;			// Pico ID (Pico, Pico W, Pimoroni...)
	uint8_t Fill;			//
	uint16_t FW_Rev;		// PicoMEM Firmware revision
} BIOSVAR_t;

// PicoMEM Configuration table, defined also in the BIOS
// ! Be carefull, Word must be aligned
typedef struct PMCFG_t {
    FSizeName_t FDD[2];
    FSizeName_t HDD[4];
    uint8_t     FDD_Attribute[2];  // Bit 7:1 PicoMEM Image ; Bit 6: USB or SD ; Bit 0-1: PC device / image number
    uint8_t     HDD_Attribute[4];  // Bit 7:1 PicoMEM Image ; Bit 6: USB or SD ; Bit 0-1: PC device / image number
    FSizeName_t ROM[2];    // Emulated ROM Extention names
//130 Bytes
    uint8_t  ROM_Addr[2]; // Address
	uint8_t  ROM_BlNb[2]; // Nb of 16K Block (Max 2)
    uint8_t  MEM_Map[64]; // Memory configuration MAP
    uint8_t  EMS_Port;    // EMS Port table index (0, No EMS)
    uint8_t  EMS_Addr;    // EMS Memory Address, (Table EMS_Addr_List index)
    uint8_t  PMRAM_Ext;   // Allow Memory extention with Internal RAM
    uint8_t  PSRAM_Ext;   // Allow Memory extention with PSRAM
	uint8_t  Max_Conv;    // Maximize the Conv Memory (Increase the RAM Size in BIOS variable)
	uint8_t  FastBoot;    // Reduce the Key Wait Time during the Boot
	uint8_t  PMBOOT;      // Use PicoMEM Boot Strap
    uint8_t  IgnoreAB;    // Ignore Segment A and B for Memory emulation (Video RAM)
    uint8_t  EnableUSB;   // Tell the PicoMEM to enable or not the USB Host Mode
	uint8_t  SD_Speed;    // 12, 24 or 36MHz      (Default 24MHz)
	uint8_t  RAM_Speed;   // 80,100,120 or 130MHz (Default 100MHz)
	uint8_t  Boot_HDD;    // define the Boot HDD Number
	uint8_t  EnableWifi;  // Tell the PicoMEM to enable or not the Wifi
	uint8_t  ne2000IRQ;	  // ne2000 / Wifi IRQ
	uint16_t ne2000Port;
	uint8_t  EnableJOY;
	uint8_t  Reserved1;
	uint8_t  B_MAX_PMRAM; // Maximum allowed PMRAM for the BIOS
	uint8_t  B_MAX_ROM;   // Maximum allowed ROM for the BIOS	
	uint8_t  PREBOOT;     // Set to 1 to have the Setup started before the Boot Strap
	uint8_t  BIOSBOOT;

	uint8_t  AudioOut;
	uint8_t  AudioBuff;   // Not used for the moment to define the Nb of audio Buffer
	uint8_t  Audio1;
	uint8_t  Audio2;
	uint8_t  Audio3;
	uint8_t  Adlib;		  // Adlin On / Off (Port 0x388)
	uint16_t TDYPort;
	uint16_t CMSPort;
	uint16_t SBPort;
	uint8_t  SBIRQ;
	uint8_t  Audio4;
	uint16_t MMBPort;	
	uint8_t  ColorPr;     // Menu Color Profile
	uint8_t  UseRTC;
	uint8_t  Spare2;
	uint8_t  Spare3;	
	FSizeName_t CDR;      // CD ROM image name
	char FDDPath[2][64];
    char HDDPath[4][64];

}PMCFG_t; 

extern PMCFG_t *PM_Config;             // Pointer to the PicoMEM Configuration in the BIOS Shared Memory