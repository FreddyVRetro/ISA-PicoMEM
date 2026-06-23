#pragma once


//https://www.cs.yale.edu/flint/feng/cos/resources/BIOS/Resources/biosdata.htm
// BIOS Data Area
typedef struct BIOS_VAR {
  uint16_t COM1;
  uint16_t COM2;
  uint16_t COM3;
  uint16_t COM4;  
  uint16_t LPT1;
  uint16_t LPT2;
  uint16_t LPT3;
  uint16_t LPT4;
  uint16_t Equip;
  uint16_t memsize;
} bios_var_t;
/*
;----------------------------------------------------------------------------;
; BDA Equipment Flags (40:10H)
;----------------------------------------------------------------------------;
; 00      |			- LPT : # of LPT ports
;   x     |			- X1  :  unused, PS/2 internal modem
;    0    |			- GAM : Game port present
;     000 |			- COM : # of COM ports present
;        0| 		- DMA : DMA (should always be 0)
;         |00	 	- FLP : Floppy drives present (+1) (0=1 drive,1=2,etc)
;         |  00		- VIDM: Video mode (00=EGA/VGA, 01=CGA 40x25, 
; 				-	10=CGA 80x25, 11=MDA 80x25)
;         |    11 	- MBRAM: MB RAM (00=64K, 01=128K, 10=192K, 11=256K+)
;         |      0	- FPU : FPU installed
;         |       1	- IPL : Floppy drive(s) installed (always 1 on 5160)
;----------------------------------------------------------------------------;
*/
extern bios_var_t far * BIOSVAR;
extern bool PM_BIOS_IRQ_Present;  // BIOS Interrupt detected
extern bool PM_BIOS_Present;      // BIOS Code detected and Checksum OK
extern bool PM_Port_present;      // IO Port detected

extern bool pm_detectport(uint16_t LoopNb);
extern uint16_t pm_io_cmd(uint8_t cmd,uint16_t arg);
extern uint16_t pm_irq_cmd(uint8_t cmd);
extern void pm_diag();
extern bool pm_detect();
extern void pm_dmadiag();


#define CVX_TYPE_NONE       0
#define CVX_TYPE_DAC        1  // Covox DAC
#define CVX_TYPE_DAC_STEREO 2  // Covox DAC Stereo in one
#define CVX_TYPE_DAC_DUAL   3  // Covox DAC Dual (LPT1+LPT2)
#define CVX_TYPE_DISNEY     4

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
#define CMDERR_NOIRQ	    0x13  // No IRQ detected/Available
#define CMDERR_NOTSUPP    0xFF  // Command not supported

// 0x 1x General / Configuration Commands
#define CMD_Reset         0x00   // Put back the status to 0, after an error
#define CMD_PMIRQ_ACK     0x01   // PicoMEM multiplexed interrupt acknowledge
#define CMD_Reserved      0x02
#define CMD_Reserved2     0x03
#define CMD_Reserved3     0x04   // Enable the decoding of a device, on an I/O range (Not used)
#define CMD_GetMEMType    0x05   // Read the type of Memory mapped to a memory range
#define CMD_SetMEMType    0x06   // Set the type of Memory mapped to a memory range
#define CMD_MEM_Init      0x07   // Configure the memory based on the Config table
#define CMD_SaveCfg       0x08   // Save the Configuration to the file
#define CMD_DEV_Init      0x09   // Initialize the devices (Run at the BIOS Setup end)
#define CMD_TDY_Init      0x0A   // Initialize the Tandy RAM emulation
#define CMD_Test_RAM      0x0B   // Write the sent value to the BIOS RAM Test Address, Return the value that was present
#define CMD_TESTIO        0x0C   // Test Data transfer via IO > To remove when done

#define CMD_GetPMString   0x10   // Read the PicoMEM Firmware string
#define CMD_ReadConfig    0x12   // Read the Config variables from I/O  ! Not implemented
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
#define CMD_S_Init         0xB0  // Serial Output Init (Resolution) and clear Screen
#define CMD_S_PrintStr     0xB1  // Print a Null terminated string from xxx
#define CMD_S_PrintChar    0xB2  // Print a char (From RegL)
#define CMD_S_PrintHexB    0xB3  // Print a Byte in Hexa
#define CMD_S_PrintHexW    0xB4  // Print a Word in Hexa
#define CMD_S_PrintDec     0xB5  // Print a Word in Decimal
#define CMD_S_GotoXY       0xB6  // Change the cursor location (Should be in 80x25)
#define CMD_S_GetXY        0xB7  // 
#define CMD_S_SetAtt       0xB8
#define CMD_S_SetColor     0xB9
#define CMD_StartUSBUART   0xBF  // Start the UART Display

#define CMD_DMA_TEST       0xAA
