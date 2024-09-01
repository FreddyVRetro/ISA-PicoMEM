;This program is free software; you can redistribute it and/or
;modify it under the terms of the GNU General Public License
;as published by the Free Software Foundation; either version 2
;of the License, or (at your option) any later version.

;This program is distributed in the hope that it will be useful,
;but WITHOUT ANY WARRANTY; without even the implied warranty of
;MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;GNU General Public License for more details.

;You should have received a copy of the GNU General Public License
;along with this program; if not, write to the Free Software
;Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

; All these variables defined here are in the PicoMEM BIOS "RAM" (4Kb)
; * Variables set or used by the PicoMEM 
; * !! Don't move them, or change the PicoMEM Firmware accordingly

; BIOS End Fill

VARS_OFFS Equ 16*1024

; !! Don't change without modifying the Pico Code
TIMES (VARS_OFFS)-($-$$) DB 0xFF
; BIOS RAM "Header"
BV_VARID      DB 0x12    ; PicoMEM set the first Byte to 0x12
BV_MEMTst     DB 0x00    ; Byte used to test if the Shared RAM is OK
BV_InitCount  DB 0x00    ; Increment at each BIOS Start
BV_BootCount  DB 0x00    ; Increment at each BootStrap Start
BV_PSRAMInit  DB 0x00    ; 0FFh : Disabled, 0FEh if init in progress, 0FDh Failure, then the Nb of MB         (Updated by the PicoMEM)
BV_SDInit     DB 0xFD    ; 0FFh : Disabled, OFEh if init in progress, 0FDh Failure,0FCh Skipped then 0 if Ok  (Updated by the PicoMEM)
BV_USBInit    DB 0xFD    ; 0FFh : Disabled, OFEh if init in progress, 0FDh Failure,0FCh Skipped then 0 if Ok  (Updated by the PicoMEM)
BV_CFGInit    DB 0x00    ; 0FFh : Disabled, OFEh if init in progress, 0FDh Failure,0FCh Skipped then 0 if Ok  (Updated by the PicoMEM)
BV_WifiInit   DB 0x00    ; 0FFh : Not Available, 0FEh Disabled, Then, init function return code.
BV_PortInit   DB 0x00    ; PicoMEM Base Port initialisation  0FEh not initialized 0 Ok
BV_USBDevice  DB 0x00    ; Mounted USB Device Bit 0: Mouse Bit 1: Keyboard  Bit 3: Joystick
BV_IRQ        DB 3       ; Detected IRQ Number
BV_IRQSource  DB 0x00    ; Source of the Hardware Interrupt, Data stored in IRQ_Param
BV_IRQStatus  DB 0x00	 ; Status of the IRQ
BV_IRQArg     DB 0x00	 ; Argument for the IRQ (Like the SW IRQ number to call)
BV_Reserved   DB 0x00
; ** Legacy PC infos, detected by the PM BIOS during the First Boot
PC_DiskNB     DB 0x00    ; Number of Disk found at the origin
PC_FloppyNB   DB 0x00    ; Number of Physical Floppy drive
PC_MEM        DW 0x00	 ; PC RAM Size in Kb
New_DiskNB    DB 0x00    ; Number of Disk in total after HDD Mount
New_FloppyNB  DB 0x00    ; Number of Floppy in total after FDD Mount
BV_Tandy      DB 0x00    ; 1 if Tandy mode
BV_TdyRAM     DB 0x00    ; 

BV_SIZE Equ 32           ; Size of the section above

; Offset 12Kb + 16
TIMES (VARS_OFFS+BV_SIZE)-($-$$) DB 0x11
; HDD 0 DPT (Updated by the Pi Pico, put a pointer to it in int Vector 41h) (16 Bytes)
PM_DPT_0 DW 1024     ; Cylinders
		 DW 16       ; Heads
		 DW 0        ; Starts reduced write current cylinder
		 DW 0		 ; Write precomp cylinder number
		 DB 0		 ; Max ECC Burst Length
		 DB 40h		 ; Control byte: Disable ECC retries, leave access retries, drive step speed 3ms
		 DB 0		 ; Standard Timeout
		 DB 0		 ; Formatting Timeout
		 DW 0		 ; Landing Zone
		 DB 63       ; Sectors per track
		 DB 0		 ; reserved

; HDD 1 DPT (Updated by the Pi Pico, put a pointer to it in int Vector 46h)
PM_DPT_1 DW 1024     ; Cylinders
		 DW 16       ; Heads
		 DW 0        ; Starts reduced write current cylinder
		 DW 0		 ; Write precomp cylinder number
		 DB 0		 ; Max ECC Burst Length
		 DB 40h		 ; Control byte: Disable ECC retries, leave access retries, drive step speed 3ms
		 DB 0		 ; Standard Timeout
		 DB 0		 ; Formatting Timeout
		 DW 0		 ; Landing Zone
		 DB 63       ; Sectors per track
		 DB 0		 ; reserved

;Format of diskette parameter table:
;Offset	Size	Description	(Table 01264)
; 00h	BYTE	first specify byte
;		bits 7-4: step rate (Fh=2ms,Eh=4ms,Dh=6ms,etc.)
;		bits 3-0: head unload time (0Fh = 240 ms)
; 01h	BYTE	second specify byte
;		bits 7-1: head load time (01h = 4 ms)
;		bit    0: non-DMA mode (always 0)
;		Note:	The DOS boot sector sets the head load time to 15ms,
;			  however, one should retry the operation on failure
; 02h	BYTE	delay until motor turned off (in clock ticks)
; 03h	BYTE	bytes per sector (00h = 128, 01h = 256, 02h = 512, 03h = 1024)
; 04h	BYTE	sectors per track (maximum if different for different tracks)
; 05h	BYTE	length of gap between sectors (2Ah for 5.25", 1Bh for 3.5")
; 06h	BYTE	data length (ignored if bytes-per-sector field nonzero)
; 07h	BYTE	gap length when formatting (50h for 5.25", 6Ch for 3.5")
; 08h	BYTE	format filler byte (default F6h)
; 09h	BYTE	head settle time in milliseconds
; 0Ah	BYTE	motor start time in 1/8 seconds


; ** Variables used to send the Registers to the Pi Pico, for the IRQ **
; Offset 12Kb + 64
TIMES (VARS_OFFS+BV_SIZE+32)-($-$$) DB 0x22
REG_AX       DW 0x00
REG_BX       DW 0x00
REG_CX       DW 0x00
REG_DX       DW 0x00
REG_DS       DW 0x00
REG_SI       DW 0x00
REG_ES       DW 0x00
REG_DI       DW 0x00
REG_FLAG     DW 0x00

; ** PicoMEM Configuration variables: Loaded at Boot time from the CF, then saved
; !! Don't change without modifying the Pico Code
; !! Don't change the order, or Config file is not compatible
TIMES (VARS_OFFS+BV_SIZE+32+18)-($-$$) DB 0x33 ; 66
PM_ConfigTable:
PMCFG_FDD0Size DW 360 			; FDD Image files
PMCFG_FDD0Name DB 'MyFloppy1    ',0
PMCFG_FDD1Size DW 0
PMCFG_FDD1Name DB 14 DUP (0)
PMCFG_HDD0Size DW 512 			; HDD Image files
PMCFG_HDD0Name DB 'MyDisk1      ',0
PMCFG_HDD1Size DW 0
PMCFG_HDD1Name DB 14 DUP (0)
PMCFG_HDD2Size DW 0
PMCFG_HDD2Name DB 14 DUP (0)
PMCFG_HDD3Size DW 0
PMCFG_HDD3Name DB 14 DUP (0)
; Disk Attribute : 
; Bit 7-6 : 00 Use the Previous BIOS Code, Bit 1-0 tell the Disk number to use
;         : 01 Error : Use the previous BIOS Code
; 		  : 1x Go to the PicoMEM disk code Lower bits will contain disk type
FDD0_Attribute DB 0             ; List of the Disk emulated in the Pi Pico
FDD1_Attribute DB 0
HDD0_Attribute DB 0
HDD1_Attribute DB 0
HDD2_Attribute DB 0
HDD3_Attribute DB 0
PMCFG_ROM0Size DW 16
PMCFG_ROM0Name DB 'MyROM1       ',0
PMCFG_ROM1Size DW 0
PMCFG_ROM1Name DB 14 DUP (0)
PMCFG_ROM0Addr DB 65  			; Address (Index in an Address list)
PMCFG_ROM1Addr DB 0
PMCFG_ROM0BlNb DB 1
PMCFG_ROM1BlNb DB 0
PMCFG_PM_MMAP  DB 64 DUP (0)    ; PicoMEM Memory MAP (What the Pico should do )
PMCFG_EMS_Port DB 1
PMCFG_EMS_Addr DB 1
PMCFG_PMRAM_Ext  DB 0
PMCFG_PSRAM_Ext  DB 0
PMCFG_Max_Conv   DB 0
PMCFG_FastBoot   DB 0
PMCFG_PMBOOT     DB 0 	      ; Use PicoMEM Boot Strap
PMCFG_IgnoreAB   DB 1         ; Ignore segment A and B for Memory
PMCFG_EnableUSB  DB 0
PMCFG_SD_Speed   DB 0         ; 12, 24 or 36MHz      (Default 24MHz)
PMCFG_RAM_Speed  DB 0         ; 80,100,120 or 130MHz (Default 100MHz)
PMCFG_Spare1     DB 0
PMCFG_EnableWifi DB 0
PMCFG_WifiIRQ    DB 0
PMCFG_ne2000Port DW 0
PMCFG_EnableJOY  DB 0		  ; USB Joystick enable/Disable
PMCFG_Reserved1  DB 0
PMCFG_MAX_PMRAM  DB 8 		  ; Maximum allowed PMRAM for the BIOS
PMCFG_MAX_ROM    DB 0	      ; Maximum emulated ROM 16Kb Block
PMCFG_PREBOOT    DB 1		  ; Force Pre Boot Setup for DOS :)
PMCFG_BIOSBOOT   DB 0         ; Hook the BIOS Boot interrupt as well

PMCFG_AudioOut   DB 0
PMCFG_AudioBuff  DB 0
PMCFG_Audio1     DB 0
PMCFG_Audio2     DB 0
PMCFG_Audio3     DB 0
PMCFG_Adlib	     DB 0
PMCFG_TDYPort	 DW 0
PMCFG_CMSPort    DW 0
PMCFG_SBPort     DW 0
PMCFG_Audio4     DB 0
PMCFG_ColorPr    DB 0 	      ; Menu Color Profile

; reserve 256 Bytes for future Config Param

%define STR_WifiSSID PCCR_Param+6


TIMES (VARS_OFFS+BV_SIZE+50+256)-($-$$) DB 0x44 ; Offset : 8Kb + 16+32+18+256
; ** Memory buffer for the commands sent by the Pico to the PC and the Pico Commands (Commands plus IRQ)
; !! Don't change without modifying the Pico Code
PCCR_PMSTATE DB 00          ; PicoMEM State   > Updated by the Pico Only
PCCR_PCSTATE DB 00          ; PC State        > Updated by the PC Only
PCCR_CMD     DB 0xFF        ; The Command number sent by the Pi Pico > To be updated by the Pico Only
PCCR_SectNB  DB 0x00        ; Number of sector copied (Total)
BV_IRQParam  DB (8) DUP (0) ; Parameters used by the IRQ, must be <> PCPARAM
;PCCR_Param Memory space used by the PC Commands / To communicate with the PicoMEM
PCCR_Param   DW 512         ; Parameters (Cont) 512 Bytes : 32 Disk maximum
             DB 'MyDisk1      ',0
             DW 0FFFEh
             DB 'Folder',0,'      ',0
             DW 333
             DB 'MyDisk3..    ',0
             DW 444
             DB 'MyDisk4...   ',0
			 DW 555
             DB 'MyDisk5....  ',0
             DW 666
             DB 'MyDisk6..... ',0
             DW 777
             DB 'MyDisk7......',0
             DW 888
             DB 'MyDisk8      ',0
             DW 999
             DB 'MyDisk9      ',0
             DW 10
             DB 'MyDisk10     ',0
             DW 11
             DB 'MyDisk11     ',0			 
             DW 12
             DB 'MyDisk12     ',0
             DW 13
             DB 'MyDisk13     ',0
             DW 14
             DB 'MyDisk14     ',0
             DW 15
             DB 'MyDisk15     ',0
             DW 16
             DB 'MyDisk16     ',0
             DW 17
             DB 'MyDisk17     ',0
			 
			 DW 0FFFFh
			 DB 'DiskIMG Err  ',0
             DB (512-6*16) DUP (0x55)

; ** PM BIOS variables, used by the BIOS Only
OldInt3h       DD 0
OldInt5h       DD 0
OldInt7h       DD 0
OldInt9h       DD 0		 ; Previously saved Int9h Code
OldInt13h      DD 0      ; Previously saved Int13h code, to be called if disk not emulated
OldInt19h      DD 0      ; Previously saved Int19h code, to call if PM BOOT Strap disabled
OLD_DPT0       DD 0
OLD_DPT1       DD 0
Int13h_Flag    DB 0
Int19h_Counter DB 0 	; Counter to check if called by the BIOS (First time) or from a Boot sector
; Variables used to control the different features
PMCFG_PC_MMAP  DB 64 DUP (0)  ; Memory map of the PC (Detected at the first Boot)
Seg_D_NotUsed  DB 0      ; 1 if there is nothing (Physically) in the segment D
Seg_E_NotUsed  DB 0      ; 1 if there is nothing (Physically) in the segment E
EMS_Possible   DB 0      ; 1 if EMS can be enabled (PSRAM Enabled and Segments Free)
PM_NeedReboot  DB 0      ; Set to 1 if a Reboot is mandatory
BOOT_FDDFirst  DB 0      ; Set to 1 if 'A' is pressed
Int19h_SkipSetup DB 0    ; Set to 1 if the Setup menu is in Int19h

; ** Variables used for the Menu and BIOS function
MEM_Scanned     DB 0  ; Set to 1 when the Memory was scanned
PM_FDDTotal     DB 0  ; Number of Floppy image on the uSD
PM_HDDTotal     DB 0  ; Number of HDD image on the uSD
PM_ROMTotal     DB 0  ; Number of ROM image on the uSD
ListLoaded      DB 0  ; 0 : Not Loaded, 1: Floppy 2:HDD 3:ROM (List loaded in PCCR_Param Table)
Menu_Current    DB 0  ; Selected menu Nb
SubMenu_Current DB 0  ; Current Sub menu Index (BIOS Parameter)
SubMenu_Nb      DB 0  ; Total Nb of Sub Menu in the current Menu
SubMenu_Coord   DW 0  ; Data Display Coordinate
SubMenu_Select  DW 0  ; Selection Code
SubMenu_Data    DW 0  ; Data offset of the sub menus
Conf_Changed    DB 0  ; 1 if Config is changed
RW_XY			DW 0  ; Rectangle Window : For display / Save Restore of rectangle Zone
RW_DX			DB 0
RW_DY			DB 0
; Variables to manage the images list selection
L_RectXY        DW 0  ; Position of the list Rectangle
L_TotalPage     DB 0
L_CurrentPage   DB 0
L_TotalEntry    DB 0  ;
L_CurrPageEntry DB 0  ; Number of entry in the active Page
ColorOffset     DW 0

PM_I         	DW 0x00   ; Generic var
PM_J         	DW 0x00   ; Generic var
PM_K         	DW 0x00   ; Generic var
PM_L         	DW 0x00   ; Generic Var
;PM_Table     DB 32 DUP (0x66)   ; Not used for the moment

Call_Int     	DB 10 DUP (0)	; Interrupt call, self modified code (Moved here)

PMB_Stack:    ; ! Used to display the Memory table

; Next 4Kb Block is RAM
;TIMES (VARS_OFFS+4*1024+4*1024)-($-$$) DB 0x66   ; Offset : Next 16Kb Block
TIMES (VARS_OFFS+4*1024)-($-$$) DB 0x66   ; Offset : Next 16Kb Block

; Disk Buffer (16Kb)  ; copied other disk def to test from here
PM_DISKB     DW 512
             DB 'MyDisk1      ',0
             DW 0FFFEh
             DB 'Folder',0,'      ',0
             DW 245
             DB 'MyDisk3..    ',0
             DW 245
             DB 'MyDisk4...   ',0
			 DW 245
             DB 'MyDisk5....  ',0
             DW 245
             DB 'MyDisk6..... ',0
             DW 245
             DB 'MyDisk7......',0
             DW 245
             DB 'MyDisk8      ',0
             DW 245
             DB 'MyDisk9      ',0
             DW 245
             DB 'MyDisk10     ',0
             DW 0FFFEh
             DB 'Folder1',0,'     ',0
			 DW 0FFFFh
			 DB 'DiskIMG Err  ',0
