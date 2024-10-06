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


// * Status and Commands definition
#define STAT_READY         0x00  // Ready to receive a command
#define STAT_CMDINPROGRESS 0x01
#define STAT_CMDERROR      0x02
#define STAT_CMDNOTFOUND   0x03
#define STAT_INIT          0x04  // Init in Progress
#define STAT_WAITCOM       0x05  // Wait for the USB Serial to be connected

#define CMDERR_NOERROR     0x00
#define CMDERR_WRITE       0x01  // Disk Write Error (File write)
#define CMDERR_IMGEXIST    0x02  // Can't create the image : Already exist
#define CMDERR_NOHDDDIR    0x03  // Directory HDD Missing
#define CMDERR_NOFDDDIR    0x04  // Directory FDD Missing
#define CMDERR_NOROMDIR    0x05  // Directory ROM Missing
#define CMDERR_DISKFULL    0x06
#define CMDERR_DISK        0x07  // Any other Disk Error
#define CMDERR_FILEREAD    0x08  // Can't open/Read file (image)
#define CMDERR_MOUTFAIL    0x09  // Disk image mount fail


// 0x 1x General / Configuration Commands
#define CMD_Reset         0x00   // Put back the status to 0, after an error
#define CMD_GetBasePort   0x01
#define CMD_SetBasePort   0x02
#define CMD_GetDEVType    0x03   // Read the device associated with an I/O range     (Not used)
#define CMD_SetDEVType    0x04   // Enable the decoding of a device, on an I/O range (Not used)
#define CMD_GetMEMType    0x05   // Read the type of Memory mapped to a memory range
#define CMD_SetMEMType    0x06   // Set the type of Memory mapped to a memory range
#define CMD_MEM_Init      0x07   // Configure the memory based on the Config table
#define CMD_SaveCfg       0x08   // Save the Configuration to the file
#define CMD_DEV_Init      0x09   // Initialize the devices (Run at the BIOS Setup end)
#define CMD_TDY_Init      0x0A   // Initialize the Tandy RAM emulation

#define CMD_GetPMString   0x10   // Read the PicoMEM Firmware string
#define CMD_Stop_IORW     0x11   // Stop the in progress I/O r/w command
#define CMD_ReadConfig    0x12   // Read the Config variables from I/O
#define CMD_WriteConfig   0x13   // Read the Config variables from I/O

// 2x Debug/Test CMD
#define CMD_SetMEM        0x22  // Set the First 64Kb of RAM to the SetRAMVal
#define CMD_TestMEM       0x23  // Compare the First 64Kb of RAM
#define CMD_StartUSBUART  0x28  // Start the UART Display

//
#define CMD_SendIRQ       0x30  // Enable the IRQ
#define CMD_IRQAck        0x31  // Acknowledge IRQ
#define CMD_LockPort      0x37

// Commands sent to the PicoMEM to send display to the Serial port (Not used / finished)
// https://github.com/nottwo/BasicTerm/blob/master/BasicTerm.cpp
#define CMD_S_Init        0x40  // Serial Output Init (Resolution) and clear Screen
#define CMD_S_PrintStr    0x41  // Print a Null terminated string from xxx
#define CMD_S_PrintChar   0x42  // Print a char (From RegL)
#define CMD_S_PrintHexB   0x43  // Print a Byte in Hexa
#define CMD_S_PrintHexW   0x44  // Print a Word in Hexa
#define CMD_S_PrintDec    0x45  // Print a Word in Decimal
#define CMD_S_GotoXY      0x46  // Change the cursor location (Should be in 80x25)
#define CMD_S_GetXY       0x47  // 
#define CMD_S_SetAtt      0x48
#define CMD_S_SetColor    0x49

#define CMD_USB_Enable    0x50	// To replace by OnOff with parameter
#define CMD_USB_Disable   0x51
#define CMD_Mouse_Enable  0x52
#define CMD_Mouse_Disable 0x53
#define CMD_Joy_OnOff     0x54
#define CMD_Keyb_OnOff    0x55

#define CMD_Wifi_Infos    0x60   // Get the Wifi Status, retry to connect if not connected
#define CMD_USB_Status    0x61   // Get the USB Status

// 7x Audio commands
#define CMD_AudioOnOff    0x70  // Full audio rendering 0: Off 1:On

#define CMD_AdlibOnOff    0x73  // Tandy Audio : 0 : Off 1: On default
#define CMD_TDYOnOff      0x74  // Tandy Audio : 0 : Off 1: On default or port
#define CMD_CMSOnOff      0x75  // CMS Audio   : 0 : Off 1: On default or port
#define CMD_CovoxOnOff    0x76  // Covox Audio : 0 : Off 1: On default or port
#define CMD_SBOnOff       0x77  // Sound Blaster Audio : 0 : Off 1: On default or port
#define CMD_GUSOnOff      0x78  // GUS Audio : 0 : Off 1: On default or port

// 8x Disk Commands
#define CMD_HDD_Getlist    0x80  // Write the list of hdd images into the Disk memory buffer
#define CMD_HDD_Getinfos   0x81  // Write the hdd infos in the Disk Memory buffer
#define CMD_HDD_Mount      0x82  // Map a disk image
#define CMD_FDD_Getlist    0x83  // Write the list of file name into the Disk memory buffer
#define CMD_FDD_Getinfos   0x84  // Write the disk infos to the Disk Memory buffer
#define CMD_FDD_Mount      0x85  // Map a disk image
#define CMD_HDD_NewImage   0x86  // Create a new HDD Image

#define CMD_Int13h         0x88  // Emulate the BIOS Int13h
#define CMD_EthDFS_Send    0x89  // Send a packet to EthDFS server emulator and wait answer

#define INIT_SKIPPED       0xFC  // Skipped or Error to initialize  ! Hardcoded in the ASM BIOS
#define INIT_INPROGRESS    0xFE
#define INIT_DISABLED      0xFE

#define  IRQ_Stat_NONE			0	// Set by the Pico, no IRQ Requested
#define  IRQ_Stat_REQUESTED     1   // Set by the Pico, IRQ Request in progress
#define  IRQ_Stat_INPROGRESS 	2   // Set by the BIOS, IRQ In progress

#define  IRQ_Stat_COMPLETED		0x10 // Set by the BIOS, IRQ Completed Ok
#define  IRQ_Stat_WRONGSOURCE   0x11 // Set by the BIOS, IRQ Completed, Wrong Source
#define  IRQ_Stat_INVALIDARG    0x11 // Invalid Argument Error

#define IRQ_None        0		// IRQ was fired, but not intentionally or nothing to do
#define IRQ_RedirectHW  1		// Directly call another HW interrupt (ne2000, other) To test more...
#define IRQ_RedirectSW  2
#define IRQ_StartPCCMD  3
#define IRQ_USBMouse    4
#define IRQ_Keyboard    5
#define IRQ_IRQ3        6		// Directly call HW interrupt 3
#define IRQ_IRQ4        7		// Directly call HW interrupt 4
#define IRQ_IRQ5        8		// Directly call HW interrupt 5
#define IRQ_IRQ6        9		// Directly call HW interrupt 6
#define IRQ_IRQ7        10		// Directly call HW interrupt 7
#define IRQ_IRQ9        11		// Directly call HW interrupt 9
#define IRQ_IRQ10       12		// Directly call HW interrupt 10
#define IRQ_NE2000      20

// Shared MEMORY variables defines (Registers and PC Commands)

// BIOSVARS

#define BV_SIZE 32

#define BV_VARID     PM_DP_RAM[0]  // Nb of time the PicoMEM Boot
#define BV_InitCount PM_DP_RAM[2]  // Nb of time the PicoMEM BIOS Start
#define BV_BootCount PM_DP_RAM[3]  // Nb of time the PicoMEM BootStrap Start
#define BV_PSRAMInit PM_DP_RAM[4]  // BIOS RAM Offset of the PSRAM Init Variable
#define BV_SDInit    PM_DP_RAM[5]  // BIOS RAM Offset of the SD Init Variable
#define BV_USBInit   PM_DP_RAM[6]  // BIOS RAM Offset of the USB Host Init Variable
#define BV_CFGInit   PM_DP_RAM[7]  // BIOS RAM Offset of the Config File Init Variable
#define BV_WifiInit  PM_DP_RAM[8]
#define BV_PortInit  PM_DP_RAM[9]   // PicoMEM Base Port initialisation  0FEh not initialized 0 Ok
#define BV_USBDevice PM_DP_RAM[10]  // Mounted USB Device Bit 0: Mouse Bit 1: Keyboard  Bit 3: Joystick ! Not used for the moment
#define BV_IRQ       PM_DP_RAM[11]  // Detected Hardware IRQ Number
#define BV_IRQSource PM_DP_RAM[12]  // Source of the Hardware Interrupt, Data stored in IRQ_Param
#define BV_IRQStatus PM_DP_RAM[13]  // Status of the IRQ
#define BV_IRQArg    PM_DP_RAM[14]
#define BV_Reserved  PM_DP_RAM[15]
#define PC_DiskNB    PM_DP_RAM[16]  // Number of Disk found at the origin
#define PC_FloppyNB  PM_DP_RAM[17]  // Number of Physical Floppy drive
#define PC_MEM       PM_DP_RAM[18]  // PC RAM Size in Kb
#define PC_MEM_H     PM_DP_RAM[19]  // PC RAM Size in Kb
#define New_DiskNB   PM_DP_RAM[20] // Number of Disk in total after HDD Mount
#define New_FloppyNB PM_DP_RAM[21] // Number of Floppy in total after FDD Mount
#define BV_Tandy     PM_DP_RAM[22] // Tandy Mode
#define BV_TdyRAM    PM_DP_RAM[23] // Number of 16Kb Block of RAM to add for Tandy

// Disk DPT  Size 16, Offset 16
// Registry  Size 18, Offset 16+32
// PicoMEM Config 16+32+18

/*         Disk Parameters Table            */

#define OFFS_DPT0  BV_SIZE    /* Disk Parameter Table 0 */
#define OFFS_DPT1  BV_SIZE+16 /* Disk Parameter Table 1 */

/* Registers save area */
#define OFFS_REGS BV_SIZE+32  // 16+32  Registers Offset in the Shared RAM

#define OFFS_AL OFFS_REGS
#define OFFS_AH OFFS_REGS+1
#define OFFS_BL OFFS_REGS+2
#define OFFS_BH OFFS_REGS+3
#define OFFS_CL OFFS_REGS+4
#define OFFS_CH OFFS_REGS+5
#define OFFS_DL OFFS_REGS+6
#define OFFS_DH OFFS_REGS+7
#define OFFS_DS OFFS_REGS+8
#define OFFS_SI OFFS_REGS+10
#define OFFS_ES OFFS_REGS+12
#define OFFS_ESL OFFS_REGS+12
#define OFFS_ESH OFFS_REGS+13
#define OFFS_DI OFFS_REGS+14
#define OFFS_CF OFFS_REGS+16

#define reg_ax (uint16_t) ((uint16_t)(PM_DP_RAM[OFFS_AH]<<8)+(uint16_t)PM_DP_RAM[OFFS_AL])
#define reg_ah PM_DP_RAM[OFFS_AH]
#define reg_al PM_DP_RAM[OFFS_AL]
#define reg_bx (uint16_t) ((uint16_t)(PM_DP_RAM[OFFS_BH]<<8)+(uint16_t)PM_DP_RAM[OFFS_BL])
#define reg_bh PM_DP_RAM[OFFS_BH]
#define reg_bl PM_DP_RAM[OFFS_BL]
#define reg_ch PM_DP_RAM[OFFS_CH]
#define reg_cl PM_DP_RAM[OFFS_CL]
#define reg_dh PM_DP_RAM[OFFS_DH]
#define reg_dl PM_DP_RAM[OFFS_DL]
#define reg_es (uint16_t) ((uint16_t)(PM_DP_RAM[OFFS_ESH]<<8)+(uint16_t)PM_DP_RAM[OFFS_ESL])
#define reg_esh PM_DP_RAM[OFFS_ESH]
#define reg_esl PM_DP_RAM[OFFS_ESL]
#define reg_flagl PM_DP_RAM[OFFS_CF]

//** PicoMEM Configuration variables: Loaded at Boot time from the CF, then saved

#define PMRAM_CFG_Offset BV_SIZE+50 // 18 Added

/*         PC Commands defines          */
#define OFFS_PCCMD BV_SIZE+306  // 16+32+18+256=322
#define OFFS_IRQPARAM   OFFS_PCCMD+4
#define OFFS_PCCMDPARAM OFFS_PCCMD+12
#define PCCR_PMSTATE PM_DP_RAM[OFFS_PCCMD]
#define PCCR_PCSTATE PM_DP_RAM[OFFS_PCCMD+1]
#define PCCR_CMD     PM_DP_RAM[OFFS_PCCMD+2]    // 
#define PCCR_SectNB  PM_DP_RAM[OFFS_PCCMD+3]    //
#define IRQ_Param    PM_DP_RAM[OFFS_IRQPARAM]
#define PCCR_Param   PM_DP_RAM[OFFS_PCCMDPARAM] //

#define PCC_PCS_NOTREADY   0x00    // PC Not Waiting for a Command
#define PCC_PCS_WAITCMD    0x01    // PC Waiting for an Command
#define PCC_PCS_INPROGRESS 0X02    // PC Command in Progress
#define PCC_PCS_COMPLETED  0X03    // PC Command completed
#define PCC_PCS_ERROR      0x10    // PC Command Error
#define PCC_PCS_RESET      0x20    // When the Pi Pico is blocked waiting for the PC, use this to reset
 
#define PCC_PMS_DOWAITCMD    0x00  // Ask the PC to Wait for a command
#define PCC_PMS_COMMAND_SENT 0x01  // Tell the PC a command is available (At the end of the command, the PC Will wait)

#define PCC_EndCommand    0   // No more Command
#define PCC_Printf        2   // Display a 0 terminiated string with Int10h from PCCMD_Param Address
#define PCC_GetScreenMode 3   // Get the currently used Screen Mode
#define PCC_GetPos        4   // Get Screen   (x,y)
#define PCC_SetPos        5   // Set Position (x,y)
//#define PCC_DS_SI_toDisk  10  
//#define PCC_Disk_toES_DI  11
//#define PCC_Set_DS_SI     12
//#define PCC_Set_ES_DI     13
#define PCC_IN8           10
#define PCC_IN16          11
#define PCC_OUT8          12
#define PCC_OUT16         13
#define PCC_MemCopyB      14
#define PCC_MemCopyW      15
#define PCC_MemCopyW_512b 16
#define PCC_IRQ           17

#define PM_BIOS_SIZE 12*1024

#define PM_DB_Offset 4*1024  // Offset of the Disk Buffer start in the BIOS/DP RAM
#define PM_DB_Size   4*1024  // Size of the 'Disk Buffer in the BIOS/DP RAM
//#define PC_DB_Start  ROM_DISK+PM_DB_Offset  // Start of the Shared disk buffer in the Host (PC) RAM

// Memory emulation defines
#define MEM_T_Size 128

struct hostio_t {
	volatile bool Updated;
	volatile uint32_t Port20hCount;   	// Incremented each time a write is done to Port 20h
	volatile uint8_t  Port21h;			// IRQ Mask register
    volatile bool 	  Port21h_Updated;  // IRQ Mask register updates
	volatile uint8_t  Port80h;			// Post Code
    volatile bool 	  Port80h_Updated;  // Post Code Previous Value
	volatile uint8_t  Port81h;			// Post Code
    volatile bool 	  Port81h_Updated;  // Post Code Previous Value	
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

typedef struct SizeName_t {
	uint16_t size;
	char name[14];
} SizeName_t;

// PicoMEM Configuration table, defined in the BIOS
// ! Be carefull, Word must be aligned
struct PMCFG_t {
    SizeName_t FDD[2];
    SizeName_t HDD[4];
    uint8_t FDD_Attribute[2];
    uint8_t HDD_Attribute[4];
    SizeName_t ROM[2];    // Emulated ROM Extention names
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
	uint8_t  WifiIRQ;	  // ne2000 / Wifi IRQ
	uint16_t ne2000Port;
	uint8_t  EnableJOY;
	uint8_t  Reserved1;
	uint8_t  B_MAX_PMRAM; // Maximum allowed PMRAM for the BIOS
	uint8_t  B_MAX_ROM;   // Maximum allowed ROM for the BIOS	
	uint8_t  PREBOOT;     // Set to 1 to have the Setup started before the Boot Strap
	uint8_t  BIOSBOOT;

	uint8_t  AudioOut;
	uint8_t  AudioBuff;  // Not used for the moment to define the Nb of audio Buffer
	uint8_t  Audio1;
	uint8_t  Audio2;
	uint8_t  Audio3;
	uint8_t  Adlib;		// Adlin On / Off (Port 0x388)
	uint16_t TDYPort;
	uint16_t CMSPort;
	uint16_t SBPort;
	uint16_t Audio4;
	uint8_t  ColorPt;     // Menu Color Profile
//161 Bytes
};

extern PMCFG_t *PM_Config;             // Pointer to the PicoMEM Configuration in the BIOS Shared Memory