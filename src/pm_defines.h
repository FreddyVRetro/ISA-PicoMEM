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
#define DEV_MMB    13


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
#define CMDERR_MOUTFAIL   0x09  // Disk image mount fail

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
#define CMD_Test_RAM      0x0B   // Write the sent value to the BIOS RAM Test Address, Return the value that was present
#define CMD_TESTIO        0x0C   // Test Data transfer via IO > To remove when done

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

#define CMD_AdlibOnOff    0x73  // Tandy Audio : 0 : Off 1: On default
#define CMD_TDYOnOff      0x74  // Tandy Audio : 0 : Off 1: On default or port
#define CMD_CMSOnOff      0x75  // CMS Audio   : 0 : Off 1: On default or port
#define CMD_CovoxOnOff    0x76  // Covox Audio : 0 : Off 1: On default or port
#define CMD_SetSBIRQDMA   0x77  // Set Sound Blaster IRQ
#define CMD_SBOnOff       0x78  // Sound Blaster Audio : 0 : Off 1: On default or port
#define CMD_SetGUSIRQDMA     0x79  // Set GUS IRQ
#define CMD_GUSOnOff      0x7A  // GUS Audio : 0 : Off 1: On default or port
#define CMD_MMBOnOff      0x7B  // Mindscape Audio : 0 : Off 1: On default or port

// 8x Disk Commands
#define CMD_HDD_Getlist   0x80  // Write the list of hdd images into the Disk memory buffer
#define CMD_HDD_Getinfos  0x81  // Write the hdd infos in the Disk Memory buffer
#define CMD_HDD_Mount     0x82  // Map a disk image
#define CMD_FDD_Getlist   0x83  // Write the list of file name into the Disk memory buffer
#define CMD_FDD_Getinfos  0x84  // Write the disk infos to the Disk Memory buffer
#define CMD_FDD_Mount     0x85  // Map a disk image
#define CMD_HDD_NewImage  0x86  // Create a new HDD Image

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

#define INIT_SKIPPED       0xFC  // Skipped or Error to initialize  ! Hardcoded in the ASM BIOS
#define INIT_INPROGRESS    0xFE
#define INIT_DISABLED      0xFE

// Shared MEMORY variables defines (Registers and PC Commands)

// BIOSVARS

#define BV_SIZE 32

#define BV_VARID      PM_DP_RAM[0]  // ID
#define BV_Test       PM_DP_RAM[1]  // Test
#define BV_InitCount  PM_DP_RAM[2]  // Nb of time the PicoMEM BIOS Start
#define BV_BootCount  PM_DP_RAM[3]  // Nb of time the PicoMEM BootStrap Start
#define BV_PSRAMInit  PM_DP_RAM[4]  // BIOS RAM Offset of the PSRAM Init Variable
#define BV_SDInit     PM_DP_RAM[5]  // BIOS RAM Offset of the SD Init Variable
#define BV_USBInit    PM_DP_RAM[6]  // BIOS RAM Offset of the USB Host Init Variable
#define BV_CFGInit    PM_DP_RAM[7]  // BIOS RAM Offset of the Config File Init Variable
#define BV_WifiInit   PM_DP_RAM[8]
#define BV_PortInit   PM_DP_RAM[9]  // PicoMEM Base Port initialisation  0FEh not initialized 0 Ok
#define BV_USBDevice  PM_DP_RAM[10] // Mounted USB Device Bit 0: Mouse Bit 1: Keyboard  Bit 3: Joystick ! Not used for the moment
#define BV_IRQ        PM_DP_RAM[11] // Detected Hardware IRQ Number
#define BV_IRQSource  PM_DP_RAM[12] // Source of the Hardware Interrupt, Data stored in IRQ_Param (Cleared by the PC when acknowledge)
#define BV_IRQ_Cnt    PM_DP_RAM[13] // Incremented at the Stard, decremented at the end (To detect if an IRQ is in progress)
#define BV_IRQStatus  PM_DP_RAM[14] // Status of the IRQ
#define BV_IRQArg     PM_DP_RAM[15] // Argument for the IRQ (Like the SW IRQ number to call)
#define BV_DMAStatus  PM_DP_RAM[16] // Status of the emulated DMA code (In the Interrupt)
#define PC_DiskNB     PM_DP_RAM[17] // Number of Disk found at the origin
#define PC_FloppyNB   PM_DP_RAM[18] // Number of Physical Floppy drive
#define PC_MEM        PM_DP_RAM[19] // PC RAM Size in Kb
#define PC_MEM_H      PM_DP_RAM[20] // PC RAM Size in Kb
#define New_DiskNB    PM_DP_RAM[21] // Number of Disk in total after HDD Mount
#define New_FloppyNB  PM_DP_RAM[22] // Number of Floppy in total after FDD Mount
#define BV_Tandy      PM_DP_RAM[23] // Tandy Mode
#define BV_TdyRAM     PM_DP_RAM[24] // Number of 16Kb Block of RAM to add for Tandy
#define BV_CmdRunning PM_DP_RAM[25] // Number of Floppy in total after FDD Mount
#define BV_BoardID    PM_DP_RAM[26] // PicoMEM Board ID (Type)
#define BV_PicoID     PM_DP_RAM[27] // Pico ID (Pico, Pico W, Pimoroni...)
#define BV_FW_Rev     PM_DP_RAM[28] // PicoMEM Firmware revision
#define BV_FW_Rev_H   PM_DP_RAM[29] // PicoMEM Firmware revision

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
#define reg_esh PM_DP_RAM[OFFS_ESH]
#define reg_esl PM_DP_RAM[OFFS_ESL]
#define reg_es (uint16_t) ((uint16_t)(PM_DP_RAM[OFFS_ESH]<<8)+(uint16_t)PM_DP_RAM[OFFS_ESL])
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

#define PCC_EndCommand    0    // No more Command
#define PCC_IN8           1	   // IO Read  8Bit
#define PCC_IN16          2    // IO Read  16Bit
#define PCC_OUT8          3    // IO Write 8Bit
#define PCC_OUT16         4    // IO Write 16Bit
#define PCC_MemCopyB      5
#define PCC_MemCopyW      6
#define PCC_MemCopyW_Odd  7
#define PCC_MemCopyW_512b 8
#define PCC_MEMR16        9    // Memory Read 16Bit
#define PCC_MEMW8         10   // Memory Write 16Bit
#define PCC_MEMW16        11   // Memory Write 16Bit
#define PCC_DMA_READ      12   //Read requested DMA channel Page:Offset and size
#define PCC_DMA_WRITE     13   //Update the DMA channel Offset and size
#define PCC_IRQ13         14   //Send an IRQ13 and get the result (BIOS)
#define PCC_IRQ21         15   //Send an IRQ21 and get the result (DOS )

#define PCC_Printf        16   // Display a 0 terminiated string with Int10h
#define PCC_GetScreenMode 17   // Get the currently used Screen Mode
#define PCC_GetPos        18   // Get Screen Offset
#define PCC_SetPos        19   // 
#define PCC_KeyPressed    20   //
#define PCC_ReadKey       21   //
#define PCC_SendKey       22   //
#define PCC_ReadString    23   //

#define PCC_Checksum      24   // Perform a Checksum from one Segment Start with a Size
#define PCC_SetRAMBlock16 25   // Write the same value to a memory Block
#define PCC_MEMWR8        26   // Write then read a byte
#define PCC_MEMWR16       27   // Write then read a word

#define PM_BIOS_SIZE 12*1024

#define PM_DB_Offset   4*1024  // Offset of the Disk Buffer start in the BIOS/DP RAM
#define PM_DB_Size     2*1024  // Size of the 'Disk Buffer in the BIOS/DP RAM

#define PM_ETHDFS_BUFF PM_DP_RAM[PM_DB_Offset-60]  // EtherDFS buffer (simulated packet) start at Disk buffer -60

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

// Size 0FFFEh: Folder
//      0FFFFh: Name error
typedef struct FSizeName_t {
	uint16_t size;
	char name[14];
} FSizeName_t;

// PicoMEM Configuration table, defined in the BIOS
// ! Be carefull, Word must be aligned
typedef struct PMCFG_t {
    FSizeName_t FDD[2];
    FSizeName_t HDD[4];
    uint8_t    FDD_Attribute[2];  // Bit 7:1 PicoMEM Image ; Bit 6: USB or SD ; Bit 0-1: PC device / image number
    uint8_t    HDD_Attribute[4];  // Bit 7:1 PicoMEM Image ; Bit 6: USB or SD ; Bit 0-1: PC device / image number
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
	uint8_t  AudioBuff;  // Not used for the moment to define the Nb of audio Buffer
	uint8_t  Audio1;
	uint8_t  Audio2;
	uint8_t  Audio3;
	uint8_t  Adlib;		// Adlin On / Off (Port 0x388)
	uint16_t TDYPort;
	uint16_t CMSPort;
	uint16_t SBPort;
	uint8_t  SBIRQ;
	uint8_t  Audio4;
	uint16_t MMBPort;
	uint8_t  ColorPt;     // Menu Color Profile
	uint8_t  UseRTC;
//161 Bytes
}PMCFG_t; 

extern PMCFG_t *PM_Config;             // Pointer to the PicoMEM Configuration in the BIOS Shared Memory