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

; * Variables set or used by the PicoMEM 
; !! Don't change without modifying the Pico Code
; Don't try to use it in any DOS Code, as this may change

PP_START_POST   Equ 0x00  ; Start and POST Test
PP_RAM_TEST     Equ 0x01
PP_WAIT_PM_INIT Equ 0x02  ; Wait for the PicoMEM init End
PP_WAIT_COM     Equ 0x03  ; Serial via USB enabled, wait for Serial to start
PP_TEST_RAMCMD  Equ 0x04  ; Test RAM via command


PP_ERR_PORT     Equ 0x80  ; PicoMEM base port error
PP_ERR_RAM      Equ 0x81  ; PicoMEM share RAM Error
PP_ERR_RAMCMD   Equ 0x82  ; PicoMEM RAM Command error

BV_SIZE   Equ 32        ; Size of the section above
DPT_SIZE  Equ 32        ; (2*16)
REGS_SIZE Equ 18        ; 9 registers saved
CONF_SIZE Equ 768       ; Configuration variables size
COMM_SIZE Equ 36        ; Commands variables size
COMB_SIZE Equ 2048+32

VARS_OFFS Equ 16*1024   ; BIOS is 16KB, Variables are just after (8KB in Total)
DPT_OFFS  Equ VARS_OFFS+BV_SIZE
REGS_OFFS Equ DPT_OFFS+DPT_SIZE
CONF_OFFS Equ REGS_OFFS+REGS_SIZE ; Configuration variables
COMM_OFFS Equ CONF_OFFS+CONF_SIZE ; Command variables
COMB_OFFS Equ COMM_OFFS+COMM_SIZE ;
; Next are BIOS Variables, no Offset


; BIOS RAM "Header" - Variables common with the BIOS and the Pico
TIMES (VARS_OFFS)-($-$$) DB 0xFF
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
BV_HWIRQ      DB 0x00    ; List the detected Hardware IRQ
BV_DMANoUpdate DB 0x00   ; Tell to the DMA code to ignore the DMA registers updates
BV_Int13hCLI  DB 0x00    ; 1 if we need to add STI in the Int13h IRQ (Disk code)
BV_SPIPSRAM   DB 0x00	 ; Argument for the IRQ (Like the SW IRQ number to call)
BV_Fill2      DB 0x00    ; Status of the emulated DMA code (In the Interrupt)
; ** Legacy PC infos, detected by the PM BIOS during the First Boot
PC_DiskNB     DB 0x00    ; Number of Disk found at the origin
PC_FloppyNB   DB 0x00    ; Number of Physical Floppy drive
PC_MEM        DW 0x00	 ; PC RAM Size in Kb   ! Must be Word aligned
New_DiskNB    DB 0x00    ; Number of Disk in total after HDD Mount
New_FloppyNB  DB 0x00    ; Number of Floppy in total after FDD Mount
BV_PC_Model      DB 0x01    ; 1 if Tandy mode
BV_TdyRAM     DB 0x00    ; Number of 16KB RAM Block to add to the conventionnal memory
BV_CmdRunning DB 0x00    ; Contains the command number running
BV_PCCMD_SOURCE DB 0x00  ; Contains the ID/number of the PicoMEM code starting/Using the PC Command
BV_BoardID    DB 0x00    ; PicoMEM Board ID (Type)
BV_PicoID     DB 0x00    ; Pico Board ID (Pico, Pico W, Pimoroni...)
BV_USBKbdScan DB 0x00   ; USB keyboard scan code (PS/2 set 1) -- Pico writes,
                        ; PMBIOS_Int1C reads + clears. 0 = no pending event.
                        ; Make = bare scan, break = scan | 0x80. (2026-06-09)
BV_FW_Rev     DW 0x00    ; PicoMEM Firmware revision

; HDD 0 DPT (Updated by the Pi Pico, put a pointer to it in int Vector 41h) (16 Bytes)
TIMES (DPT_OFFS)-($-$$) DB 0x11
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
; !! Don't change without modifying the Pico Code
; Offset 12Kb + 64
TIMES (REGS_OFFS)-($-$$) DB 0x22

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
; !! Don't change the order, or config file is no more compatible
TIMES (CONF_OFFS)-($-$$) DB 0x33 ; 66

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
PMCFG_HDD3Name DB 14 DUP (0)    ;[96]
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
PMCFG_ID       DB 0xAA         ;Should be 0xAA
PMCFG_VERSION  DB 1            ;Version of the PicoMEM configuration (Should be the same as the current firmware)
PMCFG_MKE_Port DW 0            ;
PMCFG_Spare1   DB 32 DUP (0)   ;[112]
PMCFG_PM_MMAP  DB 64 DUP (0)   ;[176]  PicoMEM Memory MAP (What the Pico should do )
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
PMCFG_Boot_HDD   DB 0         ; define the Boot HDD Number
PMCFG_EnableWifi DB 0         ; Network type: NET_NONE/NET_ETH/NET_WIFI/NET_USB (see above).
PMCFG_WifiIRQ    DB 0
PMCFG_ne2000Port DW 0
PMCFG_EnableJOY  DB 0		  ; USB Joystick enable/Disable
PMCFG_EnableUSBKbd DB 0		  ; USB Keyboard emulation on/off (default 0 = off)
				              ; Repurposed from PMCFG_Reserved1 on 2026-06-09.
				              ; Slot is reserved for the planned INT 9h hook --
				              ; toggling has no runtime effect yet.
PMCFG_MAX_PMRAM  DB 8 		  ; Maximum allowed PMRAM for the BIOS
PMCFG_MAX_ROM    DB 0	      ; [100] Maximum emulated ROM 16Kb Block
PMCFG_PREBOOT    DB 1		  ; Force Pre Boot Setup for DOS :)
PMCFG_BIOSBOOT   DB 0         ; Hook the BIOS Boot interrupt as well

PMCFG_AudioOut   DB 0
PMCFG_AudioBuff  DB 0         ; [200]
PMCFG_Audio1     DB 0
PMCFG_Audio2     DB 0
PMCFG_EnableUSBMouse DB 0     ; USB Mouse (COM2 serial mouse + equipment-bit-2
                              ; clear for Windows) on/off. Repurposed from the
                              ; unused PMCFG_Audio3 slot on 2026-07-16 -- same
                              ; offset, config-file layout unchanged.

PMCFG_Adlib	     DB 0
PMCFG_TDYPort	 DW 0
PMCFG_CMSPort    DW 0
PMCFG_SBPort     DW 0
PMCFG_SBIrq      DB 0
PMCFG_Audio4     DB 0
PMCFG_MMBPort    DW 0
PMCFG_ColorPr    DB 0 	      ; Menu Color Profile
PMCFG_RTC        DB 0         ; Real time Clock config
PMCFG_Spare2     DB 0
PMCFG_Spare3     DB 0
PMCFG_CDRSize    DW 0 		  ; [220] CDR Image files  Not size, but file format infos
PMCFG_CDRName    DB '             ',0 ; [234]
PMCFG_FDD0_PATH  DB 64 DUP (0)
PMCFG_FDD1_PATH  DB 64 DUP (0)
PMCFG_HDD0_PATH  DB 64 DUP (0)
PMCFG_HDD1_PATH  DB 64 DUP (0)
PMCFG_HDD2_PATH  DB 64 DUP (0)
PMCFG_HDD3_PATH  DB 64 DUP (0)
PMCFG_CDR_PATH   DB 64 DUP (0)   ; [692]



; reserve 256 Bytes for future Config Param

%define STR_WifiSSID PCCR_Param+6

; ** Memory buffer for the commands sent by the Pico to the PC and the Pico Commands (Commands plus IRQ)
TIMES (COMM_OFFS)-($-$$) DB 0x44 ; Offset : 8Kb + 16+32+18+256

PCCR_PCSTATE   DB 0     ; PC State        > Updated by the PC Only (Wait CMD, CMD Running, Reset)
PCCR_CMD       DB 0     ; Command number  > Updated by the Pico Only (Command to execute)
PCCR_PMSTATE   DB 0     ; PM State        > Updated by the Pico Only (Send Command, Acknowledge command end)
PCCR_CMD_RES   DB 0     ; The command result (Cleared by the Pico, SET by the PC)

IRQ_R_DMACOPY  DB 0     ;// Ask the IRQ mux to send a DMA Copy
IRQ_R_AUDIO    DB 0     ;// Will contain the requested HW IRQ NB
IRQ_R_NE2000   DB 0     ;// Will contain the requested HW IRQ NB
IRQ_R_MOUSE    DB 0     ;// Ask the IRQ mux to move the Mouse cursor
IRQ_R_KEYBOARD DB 0     ;// Ask the IRQ mux to send a Keyboard event
IRQ_R_PCCMD    DB 0     ;// Ask the IRQ mux to send a PCCMD

DEV_ISR        DW 0     ;// Devices ISR Register
PM_IRR         DB 0     ;// True if the PicoMEM IRQ is requested
PM_ISR         DB 0     ;// True if the PicoMEM IRQ is in service
IRQ_CNT        DB 0     ;// Counter for the IRQ, to be used by the IRQ code
IRQ_MOUSE_X    DB 0     ;// Mouse X position
IRQ_MOUSE_Y    DB 0     ;// Mouse Y position
IRQ_MOUSE_B    DB 0     ;// Mouse Button pressed
;// IMPORTANT: do NOT add fields here. This struct is shared 1:1 with
;// the Pico-side pm_irq_svar_t (src/pm_libs/isa_irq.h) -- inserting
;// fields breaks the DPRAM offset mapping and Pico writes to the wrong
;// addresses. Put wmbios-private mouse-deferred-call state in the
;// "OldInt*" / private-data section further down instead.
IRQ_VAR        DB (16) DUP (0)	; Src Seg/Offs, Dest Seg/Offs, Size, Channel

TIMES (COMB_OFFS)-($-$$) DB 0x55 ; Offset : 8Kb + 16+32+18+256
;PCCR_Param Memory space used by the PC Commands / To communicate with the PicoMEM (2 Kb)
%if DOS_COM=0
PCCR_Param   DB (2*1024) DUP (0x66)
%else
PCCR_Param   DW 512           ; Parameters (Cont) 512 Bytes : 32 Disk maximum
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
TEST_DL:	 DB 3
			 DB 'Test',0
			 DB 'Display',0
			 DB 'Lines for USB Status or other',0
             DB (2*1024-6*18) DUP (0x33) ; PM_PCCMD RAM is 2Kb,  There is 18x16 Disk value added as example, to remove
%endif
             DB (32) DUP (0x44) 	; Reserve 32Byte for security
; ** PM BIOS variables, used by the BIOS Only
OldInt1h       DD 0	     ;
OldInt1Ch      DD 0      ; Saved INT 1Ch (BIOS user-timer tick) -- mouse deferred-call hook
;// 2026-06-07 mouse deferred-call state. IRQ_DO_MOUSE buffers events
;// here + sets MOUSE_PENDING; PMBIOS_Int1C drains from a non-nested
;// context. Placed in wmbios-private memory (outside the DPRAM-shared
;// IRQ struct above) so they don't conflict with Pico's SVAR_IRQ.
MOUSE_BUF_X    DB 0      ;// accumulated delta X
MOUSE_BUF_Y    DB 0      ;// accumulated delta Y
MOUSE_BUF_B    DB 0      ;// latest button state
MOUSE_PENDING  DB 0      ;// 1 = mouse update queued, 0 = none
INT33_BUSY     DB 0      ;// reentrancy guard for INT 1Ch handler
WIN_LOADED     DB 0      ;// 1 = Windows is up (set by PM_Int2Fh on AX=1605h, cleared on AX=1606h)
WIN_POLL       DB 1      ;// countdown to next INT 2Fh AX=1600h Windows-detect poll (in INT 1Ch ticks)

;// USB keyboard emulation state (2026-06-09). Pico writes BV_USBKbdScan
;// (DPRAM[0x1D]) when a USB key event needs to be injected. PMBIOS_Int1C
;// reads that byte each tick; if non-zero AND PMCFG_EnableUSBKbd == 1,
;// it copies the scan to USB_KBD_SCAN, sets USB_KBD_PENDING = 1, clears
;// BV_USBKbdScan, and software-fires INT 9. Our PM_Int9h handler sees
;// the flag, processes the scan code, and IRETs. Hardware IRQ 1 (real
;// PS/2 keyboard) finds the flag cleared and chains to OldInt9h, so the
;// chassis-native keyboard keeps working untouched.
;// OldInt9h is already declared further down with the rest of the
;// saved interrupt vectors (alongside OldInt13h, OldInt19h, etc.).
;// PM_InstallInt9h uses that existing slot to save the chassis BIOS
;// INT 9h keyboard ISR. Below is just the rest of our keyboard state:
USB_KBD_PENDING DB 0     ;// 1 = USB scan code waiting for PM_Int9h
USB_KBD_SCAN   DB 0      ;// PS/2 set-1 scan code captured from BV_USBKbdScan
OldIrq3h       DD 0
OldIrq5h       DD 0
OldIrq6h       DD 0
OldIrq7h       DD 0
OldInt9h       DD 0		 ; Previously saved Int9h Code
OldInt13h      DD 0      ; Previously saved Int13h code, to be called if disk not emulated
OldInt19h      DD 0      ; Previously saved Int19h code, to call if PM BOOT Strap disabled
OldInt21h      DD 0      ; Previously saved Int21h, for DOS command debug
OldInt2Fh      DD 0      ; Previously saved Int2Fh, for DOS command debug
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
PM_CDRTotal     DB 0  ; Number of CD images in 0:/CDROM
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
RW_Text         DW 0  ; Rectangle Window : Text to display in the Rectangle
; Variables to manage the images list selection
L_RectXY        DW 0  ; Position of the list Rectangle
L_TotalPage     DB 0
L_CurrentPage   DB 0
L_TotalEntry    DB 0  ;
L_CurrPageEntry DB 0  ; Number of entry in the active Page
ColorOffset     DW 0  ; Colore profile list Offset
PCC_TextAttr    DB 0x07  ; BIOS text attribute used by PCC_Do_Printf
Def_VideoMode   DW 0  ; Save of the default video mode

PM_I         	DW 0x00   ; Generic var
PM_J         	DW 0x00   ; Generic var
PM_K         	DW 0x00   ; Generic var
PM_L         	DW 0x00   ; Generic Var

PMB_Stack:    ; ! Used to display the Memory table (DISPLAY_MEM_Bytes)

; USB keyboard scancode RING (2026-07-14). Pico pushes PS/2 set-1 scans at
; Head; PMBIOS_Int1C drains the whole ring each tick. Sits in the free 0x55
; padding before the EtherDFS header. The TIMES below is a BUILD GUARD: if
; the private-var block above ever grows past 0xF00 the count goes negative
; and assembly FAILS -- no silent DPRAM corruption. Offset MUST match the
; firmware USBKBD_RING_OFFS (0xF00 = DPRAM byte 3840).
TIMES (VARS_OFFS+0xF00)-($-$$) DB 0x55
BV_USBKbd_Buf  DB (64) DUP (0)   ; 64-byte raw set-1 scancode ring (Pico writes, PMBIOS reads)
BV_USBKbd_Head DB 0              ; write index (Pico)
BV_USBKbd_Tail DB 0              ; read index (PMBIOS_Int1C)
; USB mouse -> PS/2 aux packet MAILBOX (0xF42). Pico coalesces USB deltas into
; one 3-byte PS/2 packet; PMBIOS_Int1C injects it into the real 8042 aux device
; (cmd 0xD3 -> IRQ12) when Windows is loaded. Must match src/pm_defines.h.
BV_USBMouse_MB  DB (3) DUP (0)   ; 0xF42..0xF44 packet byte0 (08h|btn|signs), byte1 X, byte2 Y
BV_USBMouse_Rdy DB 0             ; 0xF45  1=packet ready (Pico -> PMBIOS)
BV_Dbg_Int1C    DB 0             ; 0xF46  DEBUG: PMBIOS_Int1C call counter
BV_Dbg_Win      DB 0             ; 0xF47  DEBUG: WIN_LOADED snapshot
BV_Dbg_MInj     DB 0             ; 0xF48  DEBUG: mouse aux inject counter
BV_Dbg_2FCnt    DB 0             ; 0xF49  DEBUG: INT 2Fh hook hit counter
BV_Dbg_Hooked   DB 0             ; 0xF4A  DEBUG: WD_HOOKED snapshot

TIMES (VARS_OFFS+4*1024-60)-($-$$) DB 0x55 ; 60 Bytes before Disk : EtherDFS Send buffer Header (Must not be changed/Used)

TIMES (VARS_OFFS+4*1024)-($-$$)    DB 0x66 ; RAM Offset 4Kb : Data Transfer buffer

PM_DISKB     DB 0

TIMES (VARS_OFFS+6*1024)-($-$$)    DB 0x77 ; RAM Offset 6Kb : DMA Copy Buffer (256 Bytes)

E_DMA_BUFFER DB 0
