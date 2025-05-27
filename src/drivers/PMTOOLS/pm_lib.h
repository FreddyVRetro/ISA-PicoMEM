#pragma once

extern bool PM_BIOS_IRQ_Present;  // BIOS Interrupt detected
extern bool PM_BIOS_Present;      // BIOS Code detected and Checksum OK
extern bool PM_Port_present;      // IO Port detected

extern bool pm_detectport(uint16_t LoopNb);
extern uint16_t pm_io_cmd(uint8_t cmd,uint16_t arg);
extern uint16_t pm_irq_cmd(uint8_t cmd);
extern void pm_diag();
extern bool pm_detect();

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
#define CMD_Test_RAM      0x0B   // Write the sent value to the BIOS RAM Test Address, Return the value that was present
#define CMD_TESTIO        0x0C   // Test Data transfer via IO > To remove when done

#define CMD_GetPMString   0x10   // Read the PicoMEM Firmware string
#define CMD_ReadConfig    0x12   // Read the Config variables from I/O
#define CMD_WriteConfig   0x13   // Read the Config variables from I/O

// 2x Debug/Test CMD
#define CMD_SetMEM        0x22  // Set the First 64Kb of RAM to the SetRAMVal
#define CMD_TestMEM       0x23  // Compare the First 64Kb of RAM

//
#define CMD_SendIRQ       0x30  // Enable the IRQ
#define CMD_IRQAck        0x31  // Acknowledge IRQ
#define CMD_LockPort      0x37

#define CMD_USB_Enable    0x50	// To replace by OnOff with parameter
#define CMD_USB_Disable   0x51
#define CMD_Mouse_Enable  0x52
#define CMD_Mouse_Disable 0x53
#define CMD_Mouse_Disable 0x53
#define CMD_Joy_OnOff     0x54
#define CMD_Keyb_OnOff    0x55

//6x Commands asking for various status the PicoMEM (Returns a string)
#define CMD_Wifi_Infos    0x60   // Get the Wifi Status, retry to connect if not connected
#define CMD_USB_Status    0x61   // Get the USB Status
#define CMD_DISK_Status   0x62   // Get the Disk emulation status screen.

// 7x Audio commands
#define CMD_AudioOnOff    0x70  // Full audio rendering 0: Off 1:On

#define CMD_AdlibOnOff    0x73  // Tandy Audio : 0 : Off 1: On default
#define CMD_TDYOnOff      0x74  // Tandy Audio : 0 : Off 1: On default or port
#define CMD_CMSOnOff      0x75  // CMS Audio   : 0 : Off 1: On default or port
#define CMD_CovoxOnOff    0x76  // Covox Audio : 0 : Off 1: On default or port
#define CMD_SetSBIRQDMA   0x77  // Set Sound Blaster IRQ and DMA
#define CMD_SBOnOff       0x78  // Sound Blaster Audio : 0 : Off 1: On default or port
#define CMD_SetGUSIRQDMA  0x79  // Set GUS IRQ and DMA
#define CMD_GUSOnOff      0x7A  // GUS Audio       : 0 : Off 1: On default or port
#define CMD_MMBOnOff      0x7B  // Mindscape Audio : 0 : Off 1: On default or port

// 8x Disk Commands
#define CMD_HDD_Getlist    0x80  // Write the list of hdd images into the Disk memory buffer
#define CMD_HDD_Getinfos   0x81  // Write the hdd infos in the Disk Memory buffer
#define CMD_HDD_Mount      0x82  // Map a disk image
#define CMD_FDD_Getlist    0x83  // Write the list of file name into the Disk memory buffer
#define CMD_FDD_Getinfos   0x84  // Write the disk infos to the Disk Memory buffer
#define CMD_FDD_Mount      0x85  // Map a disk image
#define CMD_HDD_NewImage   0x86  // Create a new HDD Image

#define CMD_Int13h         0x88  // Emulate the BIOS Int13h
#define CMD_Int21h         0x8A  //
#define CMD_IntEnd         0x8B  //

// Commands sent to the PicoMEM to send display to the Serial port (Not used / finished)
// https://github.com/nottwo/BasicTerm/blob/master/BasicTerm.cpp
#define CMD_S_Init        0x90  // Serial Output Init (Resolution) and clear Screen
#define CMD_S_PrintStr    0x91  // Print a Null terminated string from xxx
#define CMD_S_PrintChar   0x92  // Print a char (From RegL)
#define CMD_S_PrintHexB   0x93  // Print a Byte in Hexa
#define CMD_S_PrintHexW   0x94  // Print a Word in Hexa
#define CMD_S_PrintDec    0x95  // Print a Word in Decimal
#define CMD_S_GotoXY      0x96  // Change the cursor location (Should be in 80x25)
#define CMD_S_GetXY       0x97  // 
#define CMD_S_SetAtt      0x98
#define CMD_S_SetColor    0x99
#define CMD_StartUSBUART  0x9F  // Start the UART Display