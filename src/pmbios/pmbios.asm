; Copyright (C) 2023 Freddy VETELE

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

;******************************************************************
;* PicoMEM BIOS by Freddy VETELE
;*
;* To Build : 
;* COM : nasm -f bin -o pmbios.com  -l pmbios.lst pmbios.asm -dFTYPE=1
;* ROM : 
;* The final .bin file need to have a null CheckSum
;
; - Test Port        > ToDo Automatic port test and Config
; - Test BIOS Memory ! Fatal error if not Ok > ToDo Inform the PM Board of the failure (Enter "DIAG" Mode ?)
; - Then, reroute Int19h for Configuration code.
; In Int19h:
; - If First Boot :
;   > Read the PC Memory size
;   > Read the Disk and Floppy Drive NB

; 02/09/2026:
; - One sub menu was missing in Disk menu
; - Disable the Disk list check (Reload every time)
; - Add Segment display at init phase
; - Wifi IRQ is set to 3 by default in the BIOS, is it correct ?
; 04/09/2026:
; - PC Junior model detection
; - Copy BIOS Variable 0x415 to 0x413 before boot
; 05/09/2026:
; - Skip memory change check for PC Junior if Config file is loaded
; 10/09/2026:
; - PicoMEM IRQ is no more detected in the BIOS, done at the Pico side, if not defined in config.txt
; - IRQ detection skipped only for the PicoMEM 1.x if configured in config.txt
; 14/09/2026:
; - Add call to CMD_DEV_PreInit during the initialization phase
; 20/09/2026:
; - Added CD-ROM shortcut (Ctrl Shift F4)
; - Added text display in "Draw_rectangle"
; 26/09/2026:
; - 

[BITS 16]
CPU 186

%assign DOS_COM FTYPE  			  ; Come from the compile command line
%assign PM_VERSION PMVER
%assign TINY_BIOS             0

%define FORCE_INT19h          1
%define POST_DisplayPCRAMSize 0

; "Debug" mode options
%define Display_Intermediate  0   ; Display more messages (Debug)
%define DISPLAY_MAXCONV	      0
%define ENABLE_IRQ 	          1
%define DISPLAY_IRQNb		  0
%define IRQ_DEBUGPORT         0
%define ADD_Debug 0
; Debug mode end

%if DOS_COM=1
%define FORCE_INT19h          0
%endif

; Compile as a .COM (For test)
%if DOS_COM=1
ORG 0x0100
%else
ORG 0X0000

; 0xAA55 Header
DB 0x55
DB 0xAA
; Uses 0x20 512-byte pages : 16Kb
;DB 0x18
DB 0x20
%endif ; DOS_COM=1

    JMP PM_START

    DB 'PMBIOS'

%if PM_VERSION=2
STR_PICOMEM   DB 'PicoMEM 2',0
%else
STR_PICOMEM   DB 'PicoMEM',0
%endif
STR_Junior    DB 'PC Junior',0
STR_Init      DB ' BIOS (Date ',__DATE__,') - ',0
STR_SPACECRLF DB ' '
STR_CRLF      DB 0x0D,0x0A,0
STR_Init2     DB ' - Init: ',0
STR_InitEnd   DB ' End',0x0D,0x0A,0

STR_PICOMEMSTART DB ' BIOS by FreddyV ',__DATE__,0
STR_SEGMENT   DB 0x0D,0x0A,' ',254,' ROM @',0
STR_IOPORT    DB ' - Port'  ; Don't move !
STR_DDOT      DB ' :'
STR_SPACE     DB ' ',0
STR_DDOTNS    DB ':',0
STR_BOOTCOUNT DB ' - Boot Count : ',0
STR_PSRAM     DB ' ',254,' Memory : ',0
STR_SD        DB '- MicroSD : ',0
STR_USB       DB '- USB : ',0
STR_CONFIG    DB '- Config : ',0
%if POST_DisplayPCRAMSize=1
STR_MEM       DB ' ',254,' PC Memory : ',0
%endif
STR_Wifi      DB ' ',254,' Wifi : ',0
;STR_MAC      DB ' - MAC : ',0
STR_SSID      DB ' - SSID : ',0
STR_IRQNB	  DB ' - IRQ : ',0
;STR_MAXMEM   DB ' ',254,' Max Conventional Mem : ',0

STR_WAITCOM   DB 0x0D,0x0A,'USB DEBUG Mode: Connect a terminal to the USB Serial Port.',0x0D,0x0A,0

STR_SUP       DB ' > ',0
STR_KB        DB 'kB',0

STR_OK        DB 'Ok ',0
STR_FAILURE   DB 'Fail ',0
STR_DISABLED  DB 'Disabled ',0
STR_SKIP      DB 'Skipped ',0
STR_FATAL     DB 0x0D,0x0A,' Fatal '
STR_ERROR     DB 'Error: ',0
STR_DONE      DB 'Done',0

STR_PMNOTDETECTED DB 'PM BIOS: IO Error',0x0D,0x0A,0
STR_PMROMRAMERR   DB 'PM BIOS: RAM Error',0x0D,0x0A,0
STR_PMRAMCMDERR   DB 'PM BIOS: PC Cache enabled or Command error',0x0D,0x0A,0

STR_PressAny DB 'Press Any Key',0
STR_PressS   DB '> Press S for Setup, Other to continue ',0

STR_HDD  DB ' ',254,' HDD',0
STR_FDD  DB ' ',254,' FDD',0

; BV_DMANoUpdate Values defines (Interrupt Source/Command)

%define IRQ_None        0	; IRQ was fired, but not intentionally or nothing to do

%define IRQ_StartPCCMD  3
%define IRQ_USBMouse    4
%define IRQ_Keyboard    5
%define IRQ_IRQ3     	6
%define IRQ_IRQ4     	7
%define IRQ_IRQ5     	8
%define IRQ_IRQ6     	9
%define IRQ_IRQ7     	10
%define IRQ_IRQ9     	11
%define IRQ_IRQ10     	12

%define IRQ_NE2000      20

%define IRQ_Total       8

HW_IRQ_Table DW IRQCMD_None
		     DW IRQCMD_None
		     DW IRQCMD_None
		     DW IRQCMD_IRQ3
		     DW IRQCMD_IRQ4
		     DW IRQCMD_IRQ5
		     DW IRQCMD_IRQ6
		     DW IRQCMD_IRQ7
;             DW IRQCMD_None
;		     DW IRQCMD_None
;		     DW IRQCMD_IRQ10
;		     DW IRQCMD_None

; Various/Test messages

%if Display_Intermediate=1
STR_InstInt13h DB 'Install Int13h',0x0D,0x0A,0
STR_InstInt19h DB 'Install Int19h',0x0D,0x0A,0
STR_DiskMount  DB 'Mount Disks',0x0D,0x0A,0
%endif

STR_STATUS     DB 'Status : ',0

;STR_Tandy DB 'Tandy',0x0D,0x0A,0
;STR_DISKTEST DB 'HDD Test : ',0x0D,0x0A,0
;STR_HDDLIST  DB 'CMD_HDD_Getlist',0x0D,0x0A,0
;STR_CMDINT13 DB 'CMD_Int13h',0x0D,0x0A,0


;DEBUG :
;STR_CSET DB 'C Set ',0
;STR_CCL  DB 'C Clear ',0

%macro PICOPOST 1
	PUSH AX
	MOV AL,%1
	OUT 81h,AL
	POP AX
%endmacro

%macro printstr_w 1 ; Print String in White
    MOV SI,%1
	CALL BIOS_Printstr
%endmacro

%macro printstr_lw 1 ; Print String in light White
    MOV SI,%1
    MOV BL,15        ; Attribute
	CALL BIOS_Printstrc
%endmacro

%macro printstr_r 1 ; Print String in Red
    MOV SI,%1
    MOV BL,4        ; Attribute
	CALL BIOS_Printstrc
%endmacro

%macro printchar_w 1
	MOV AL,%1
	MOV BL,15
	CALL BIOS_Printchar
%endmacro

%macro push_all 0
	PUSH AX
	PUSH BX
	PUSH CX
	PUSH DX
	PUSH DS
	PUSH SI
	PUSH ES
	PUSH DI
	PUSH BP
%endmacro

%macro pop_all 0
    POP BP
	POP DI
	POP ES
	POP SI
	POP DS
	POP DX
	POP CX
	POP BX
	POP AX
%endmacro

%macro push_all_exceptbx 0
	PUSH AX
	PUSH CX
	PUSH DX
	PUSH DS
	PUSH SI
	PUSH ES
	PUSH DI
	PUSH BP
%endmacro

%macro pop_all_exceptbx 0
    POP BP
	POP DI
	POP ES
	POP SI
	POP DS
	POP DX
	POP CX
	POP AX
%endmacro

;%include "bioscall.asm"
%include "pm_hw.asm"
%include "bios_text.asm"
%include "bios_kb.asm"
%include "bios_disk.asm"
%include "pm_mem.asm"
%include "pm_cmd.asm"
%if TINY_BIOS=0
%include "bios_menu.asm"
%else
EMS_Addr_List DW 2,0xD000,0xE000
EMS_Port_List DW 5,0,0x268,0x288,0x298,0x2A8
BM_ChangeFDDImage:
BM_ChangeCDImage:
Do_BIOS_Menu:
	RET
%endif

; PicoMEM BIOS "Boot Sequence : 
;
; - Simple HW Diagnostic (Port, Shared RAM and Disk Buffer RAM)
; - Wait the end of the Initialisation : Wait for COM Port and Wait for Init End
; - Check if the Status Port is invalid and Reset if not.
;

WaitKey_Loop:
	CALL BIOS_Keypressed
	JZ	WaitKey_Loop		; End pause if key pressed
	CALL BIOS_ReadKey
	RET

; Code starts here.  Save everything before we start.
PM_START:

    PUSHF
    PUSH AX
    PUSH BX
    PUSH CX
	PUSH DX
	PUSH SI
	PUSH DI
	PUSH DS
	PUSH ES

    PUSH CS
	POP DS	

%if DOS_COM=1
; Code test Zone, in COM Version only

    jmp Skip_test


	JMP BIOS_Exit

Skip_test:
%endif  ; DOS_COM=1

; *****  PicoMEM Board initial test sequence *****
	PICOPOST PP_START_POST

	printstr_w STR_PICOMEM

    MOV ax, 0F000h
    MOV es, ax
; BIOS model ID at F000:FFFE
    MOV al, es:[0FFFEh]
    CMP al, 0FDh ; IBM PCjr?
    JNE NotPCjr
	printstr_w STR_Junior
	MOV byte [BV_PC_Model],2  ; 2 > PC Junior
NotPCjr:

	printstr_w STR_Init

	MOV AX,CS
    MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Hex	

; *** PicoMEM Test : Port Test 
; I/O Port test and Display (Currently test one Hardcoded port)
    MOV DX,PM_BasePort	; Test the Base Port
	CALL PM_TestReadPort
%if DOS_COM=1
    MOV AL,1
%endif	 ; DOS_COM=1
	CMP AL,1
	JE .PM_Detected
	PICOPOST PP_ERR_PORT
	MOV AX,STR_PMNOTDETECTED
	CALL PM_FatalError	; Fatal Error
	JMP BIOS_Exit
.PM_Detected:

	PICOPOST PP_RAM_TEST
	
; *** PicoMEM Test : BIOS RAM Test (16x AA/55)

    MOV SI,VARS_OFFS	; Test Offset (Segment DS)
	MOV CX,8			; Test 8KB
	CALL TestRAMSlow
	JNC ROMRAM_Ok

ROMRAM_Error:  ; Shared RAM not working
	PICOPOST PP_ERR_RAM
    MOV AX,STR_PMROMRAMERR
	CALL PM_FatalError	; Fatal Error
	JMP BIOS_Exit	
ROMRAM_Ok:

; Now we can start using the BIOS RAM

	printstr_w STR_Init2
	
; *** As the BIOS RAM is Ok, increment the Init Counter
	INC byte [BV_InitCount]

; *** POST: Initialize the BIOS Vars (Default values)

    CMP byte [BV_InitCount],1
	JNE NotFirstBoot
; Place here the init needed ONLY at the first Boot
; Reset the PC command State only at first Boot
    MOV byte [PCCR_PCSTATE],PCC_PCS_NOTREADY
    MOV byte [PCCR_PMSTATE],0

    JMP WasFirstBoot
NotFirstBoot:
; Place here the init needed ONLY if not the first Boot

WasFirstBoot:
; Place here the init to be done every time

    MOV byte [PM_NeedReboot],0
	MOV byte [Int19h_SkipSetup],0		; Ask to not start in the Boot Strap Int19h
	MOV byte [ListLoaded],0	
	MOV byte [Int13h_Flag],0
	MOV byte [BOOT_FDDFirst],0
	
	MOV byte [PMCFG_SD_Speed],24
	MOV byte [PMCFG_RAM_Speed],100


; *** POST : Wait the end of the PicoMEM Initialisazion ***
	
	PICOPOST PP_WAIT_PM_INIT
	
; Read Command Status, then :
; 1) Check if in Wait COM Mode (Status Port)

	printchar_w '.'    ; Check if in Debug mode (Wait COM)
	
%if DOS_COM=1
	JMP RAM_CMD_TestOk
%endif ; DOS_COM=1

    CALL PM_ReadStatus
    CMP AL,STAT_WAITCOM
	JNE POST_WaitInitLoop

	PICOPOST PP_WAIT_COM	
	
	printstr_w STR_WAITCOM
    
POST_WaitCOMLoop:
    CALL PM_ReadStatus 
    CMP AL,STAT_WAITCOM
	JE POST_WaitCOMLoop

; 2) If in Init mode (Status Port), wait for the end

POST_WaitInitLoop:
    CALL PM_ReadStatus
    CMP AL,STAT_INIT
	JE POST_WaitInitLoop	; If the Init never finish, the PC is crashed !!

POST_InitCompleted:   ; Init completed or Stuck (Need RESET)

; 3) If not in Ready Status, Initialize

	printchar_w '.'    ; Check Command Port Status (Must be 0)

    CALL PM_ReadStatus
    CMP AL,STAT_READY
    JE StatusAtBootOk
	
	PUSH AX
    printstr_w STR_CRLF
	printstr_w STR_STATUS
	POP AX
	CALL BIOS_PrintAL_Hex ; display the Status number
    printstr_w STR_CRLF 	

    CALL PM_Reset
StatusAtBootOk:

    PICOPOST PP_TEST_RAMCMD

; Now we can use RAM and commands, verify if the command to write to RAM is Ok
; Write from port, Read from memory and compare : Detect if PC Cache is active and test commands
    MOV CX,4

	XOR BX,BX
RAM_CMD_TestLoop:
    ADD BL,11h
	PUSH BX
	PUSH CX

    MOV AH,CMD_Test_RAM	     	; Write BL to [BV_MEMTst]
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd

	POP CX
	POP BX

    CMP BL,byte [BV_MEMTst]		; If BV_MEMTst
	JNE RAM_CMD_TestError

	LOOP RAM_CMD_TestLoop
	JMP RAM_CMD_TestOk
	
RAM_CMD_TestError:       ; Display the Shared RAM error

	PICOPOST PP_ERR_RAMCMD
    MOV AX,STR_PMRAMCMDERR
	CALL PM_FatalError	; Fatal Error
	JMP BIOS_Exit

RAM_CMD_TestOk:
	
; *** Tandy RAM Mode : Perform the RAM Scan, then initialise Conventionnal RAM and Reboot ***

    CMP byte CS:[BV_TdyRAM],0
	JE POST_NoTandyRAM

    CMP byte CS:[BV_InitCount],1
	JNE POST_TandyRAMTest2
	
; Detect the RAM config before the PMRAM is used
	CALL PM_Memory_FirstBoot

    MOV byte [PMCFG_PMRAM_Ext],0  ; Disable the PMRAM Extention just in case

; Ask the PicoMEM to add the RAM

    CLI 	; Stop IRQ to avoid problem

	MOV AH,CMD_TDY_Init
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd

; Copy what the PicoMEM wrote in the PMCFG_PM_MMAP to the PMCFG_PC_MMAP
	PUSH CS
	POP ES
	MOV SI,PMCFG_PM_MMAP
	MOV DI,PMCFG_PC_MMAP
	MOV CX,40
	REP MOVSB
	
    JMP PM_Reboot

POST_TandyRAMTest2:
    CMP byte CS:[BV_InitCount],2
	JA POST_NoTandyRAM 

; 2nd Tandy BOOT : Check if emulation worked and update the RAM MAP

POST_NoTandyRAM:

; 4) Detect IRQ

	PICOPOST 0x11
	printchar_w '.'    ; Detect IRQ

%if ENABLE_IRQ=1
    CALL PM_DetectIRQ	
%endif	

	PICOPOST 0x12

;Device init during the Initialization

    MOV AH,CMD_DEV_PreInit
	CALL PM_SendCMD           ; Do CMD_DEV_PreInit Command
    CALL BIOS_DoPCCMD	      ; Command will use the PC Commands
	CALL PM_WaitCMDEnd        ; Wait for the command End

	CMP byte CS:[PMCFG_PREBOOT],1
	JNE PM_NoPreBootSetup

	MOV byte CS:[Int19h_SkipSetup],1		; Ask to not start in the Boot Strap Int19h

	printstr_w STR_CRLF

	CALL PM_Do_Setup

PM_NoPreBootSetup:

%if DOS_COM=1
	JMP BIOS_Exit
%endif ; DOS_COM=1

	PICOPOST 0x13
	
	CMP byte CS:[PMCFG_PREBOOT],1
	JE PM_Skipdot
	printchar_w '.'   		    ; Install IRQ19h
PM_Skipdot:

    CALL PM_InstallIRQ19		; Install the Boot Strap IRQ

	PICOPOST 0x20

	CMP byte CS:[PMCFG_PREBOOT],1
	JE BIOS_Exit
	printstr_w STR_InitEnd

BIOS_Exit:						; The BIOS Jump here after a Fatal Error (Board not working)
    POP ES
    POP DS
	POP DI
	POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    POPF
	
%if DOS_COM=1
    mov  ax,4c00h
    int  21h                            ;exit to DOS
%endif	 ; DOS_COM=1
	RETF			;RETURN to the Computer BIOS

STR_CFGErr DB '! Config File Write Error: Check/Replace the MicroSD',0x0D,0x0A,0
%if DISPLAY_MAXCONV=1
STR_MAXMEM DB ' RAM: ',0
%endif


; Do the PicoMEM Initialisation (At the end of the BIOS Init Code)
PM_FinalConfig:

    PICOPOST 0x40
; ** Save the Config File **
	
    MOV AH,CMD_SaveCfg
	CALL PM_SendCMD           ; Do CMD_SaveCfg Command
	CALL PM_WaitCMDEnd        ; Wait for the command End
	JNC SaveConfigOk
    printstr_w STR_CFGErr
	CALL PM_Reset			  ; Need to Reset after an error
SaveConfigOk:	

    PICOPOST 0x41

; *** Configure the Memory interface
    MOV AH,CMD_MEM_Init
	CALL PM_SendCMD           ; Do CMD_MEM_Init Command
	CALL PM_WaitCMDEnd        ; Wait for the command End

	CALL MEM_GetMaxConv

%if DISPLAY_MAXCONV=1
	PUSH AX
	printstr_w STR_MAXMEM
	POP AX
	
	PUSH AX
    MOV BL,7    ; Light Grey	
	CALL BIOS_PrintAX_Dec
	printstr_w STR_CRLF	
	POP AX
%endif

	CMP AX,16		; Error !! Don't touch the BIOS RAM Size (PC Minimum RAM Size is 16Kb)
	JBE NotUpdateBIOSRAMSize

    CMP byte CS:[BV_PC_Model],1  ; Tandy 1000 Mode > Use the Tandy detected RAM
	JE NotUpdateBIOSRAMSize

    CMP byte CS:[BV_PC_Model],2  ; PC Junior Mode > Apply the RAM Size Patch
	JNE NotPCJunior
	MOV BX,040h
	MOV ES,BX
	MOV AX,word ES:[BIOSVAR_MEMORY_JR]	; Get the PC Junior detected memory size (To move to the PC Memory Size)
    JMP Not640kRAM
NotPCJunior:

; Test if the PC ROM Size is aligned to 16KB > Don't update it if not the case (a BIOS reserved some RAM)
	MOV BX,CS:[PC_MEM]

%if DISPLAY_MAXCONV=1
	PUSH BX
	printstr_w STR_MAXMEM
	POP BX
	
	PUSH BX
	MOV AX,BX
    MOV BL,7    ; Light Grey	
	CALL BIOS_PrintAX_Dec
	printstr_w STR_CRLF	
	POP BX
%endif

	MOV CL,4
	SHR BX,CL
	MOV CL,4
	SHL BX,CL
	CMP BX,word CS:[PC_MEM]
	JNE NotUpdateBIOSRAMSize
	
; *** Update the BIOS with the new memory size
	CMP AX,640
	JB  Not640kRAM
	CMP byte CS:[PMCFG_Max_Conv],1
	JE Not640kRAM		; Maximize the RAM Size
	MOV AX,640
Not640kRAM:
	MOV BX,040h
	MOV ES,BX
	MOV word ES:[BIOSVAR_MEMORY],AX	; 640Kb if 640 or more, or Memory size
NotUpdateBIOSRAMSize:

; Keyboard IRQ no more installed in the BIOS
;	CALL PM_InstallIRQ9         ; Install the KB HotKey interrupt

;Initialise all the other PicoMEM Devices

    PICOPOST 0x42

    MOV AH,CMD_DEV_Init
	CALL PM_SendCMD           ; Do CMD_DEV_Init Command
    CALL BIOS_DoPCCMD	      ; Command will use the PC Commands
	CALL PM_WaitCMDEnd        ; Wait for the command End

; Add Joystick in the BIOS Variable if added by the PicoMEM
    CMP BYTE CS:[PMCFG_EnableJOY],1
	JNE Not_Add_Joystick
	
    MOV AX,040h
	MOV ES,AX
    OR BYTE ES:[BIOSVAR_EQUIPH],10h		; Needed for some games (Alley Cat)
Not_Add_Joystick:
	
    PICOPOST 0x43
	
%if ENABLE_IRQ=1
    CALL PM_InstallIRQ3_5_7
%endif	
	RET ; PM_FinalConfig End

; END of the PicoMEM Init


; *******************  Install the IRQ 21h  ************************
; MS DOS Interrrupts call Hook (For DOS commands Trace/Debug)

PM_InstallIRQ21h:

    MOV AX,21h
	MOV DX,PM_Int21h
	MOV BX,OldInt21h
	CALL BIOS_HookIRQ
	
	RET
	
; *******************  Install the IRQ 2Fh  ************************
   
PM_InstallIRQ2Fh:

    MOV AX,2Fh
	MOV DX,PM_Int2Fh
	MOV BX,OldInt2Fh
	CALL BIOS_HookIRQ
	
	RET	

; *******************  Install the IRQ 13h  ************************

;STR_DiskPresent DB 'Warning: HDD already present : Disable the conflicting PicoMEM Disks',0
;STR_Skip DB 'Skip IRQ13h 3',0x0D,0x0A,0

PM_InstallIRQ13:

    PUSH CS
	POP DS

; Print the current number of HDD

%if Display_Intermediate=1
    printstr_w STR_InstInt13h	
%endif

; !!! If there is already disks, this is not supported !!!

; 1) Send the Mount Disks Command (Based on the PM Config variables)

%if Display_Intermediate=1
    printstr_w STR_DiskMount
%endif

	CALL BIOS_MountFDD	
	
    MOV AH,CMD_HDD_Mount
	CALL PM_SendCMD           ; Do CMD_HDD_Mount Command
	CALL PM_WaitCMDEnd        ; Wait for the command End

jmp PM_DoHook13

; 2) Hook the DPT0 and DPT1 if the disk are Enabled
;    And increment the Disk number count (Initialized by the BIOS at each reboot)
;    http://www.techhelpmanual.com/259-int_41h_and_int_46h__hard_disk_parameter_pointers.html

    CMP byte [HDD0_Attribute],0x80	; Emulated HDD0 ?
	JB DiskInit_NoHDD0
;HDD0 Present
    MOV AX,41h						; Define the new DPT address
	MOV DX,PM_DPT_0
	MOV BX,OLD_DPT0
	CALL BIOS_HookIRQ

	MOV byte ES:[BIOSVAR_DISKNB],1	; Set the Nb of disk in the BIOS

DiskInit_NoHDD0:

    CMP byte [HDD1_Attribute],0x80	; Emulated HDD1 ?
	JB DiskInit_NoHDD1
;HDD1 Present
    MOV AX,46h						; Define the new DPT address
	MOV DX,PM_DPT_1
	MOV BX,OLD_DPT1
	CALL BIOS_HookIRQ

DiskInit_NoHDD1:

    CMP byte [HDD2_Attribute],0x80	; Emulated HDD2 ?
	JB DiskInit_NoHDD2
DiskInit_NoHDD2:

    CMP byte [HDD3_Attribute],0x80	; Emulated HDD3 ?
	JB DiskInit_NoHDD3
DiskInit_NoHDD3:

PM_DoHook13:
    
	PUSH ES
    MOV AX,040h	; ! Useless now
	MOV ES,AX
    MOV AL,CS:[New_DiskNB]
    MOV ES:[BIOSVAR_DISKNB],AL		; Set the Nb of disk as computed by the PicoMEM
	POP ES

; 3) Modify the Floppy number

    CALL FDD_UpdateBIOSVar

; 4) Save previous IRQ13h and Install the new IRQ Vector

    MOV AX,13h
	MOV DX,PM_Int13h
	MOV BX,OldInt13h
	CALL BIOS_HookIRQ

;printstr_w STR_Skip

    RET  ; PM_InstallIRQ13 End

; ******  Display the emulated Disk list (FDD and HDD) ******
Display_DisksList:

; Get the Disk Status (String List)
    MOV AH,CMD_DISK_Status
	CALL PM_SendCMD         ; Get the Wifi Status Infos
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end
	
%if DOS_COM=1
    MOV SI,TEST_DL
%else
	MOV SI,PCCR_Param
%endif	
	CALL Display_String_List
	RET

%if 0
; ** Display the FDD Images list **
    XOR BX,BX
DisplayMountedFDDList_Loop:

    PUSH BX
    CMP byte [FDD0_Attribute+BX],0x80
    JB No_Display_FDDImageName

    PUSH BX					; Display "HDDx :"
    printstr_w STR_FDD
    POP BX
    MOV AX,BX
    PUSH BX
    MOV BL,7    			; Light Grey
    CALL BIOS_PrintAX_Dec
    printstr_w STR_DDOT
    POP BX

    MOV SI,PMCFG_FDD0Size

    MOV CL,4
    SHL BX,CL
    ADD SI,BX
    PUSH SI
   
    LODSW  			 	 	; Load the Disk Size
    MOV BL,7    		 	; Light Grey
    CALL BIOS_PrintAX_Dec 	; Display the Disk Size
    MOV SI,STR_KB
    CALL BIOS_Printstrc   	; Display 'Kb'
    printstr_w STR_SPACE   
   
    POP SI
    ADD SI,2
    CALL BIOS_Printstrc 	; Display the Disk name   
    printstr_w STR_CRLF   

No_Display_FDDImageName:
    POP BX
    INC BX
    CMP BX,2	; 2 FDD Images max
    JNE DisplayMountedFDDList_Loop

; ** Display the HDD Images list **
    XOR BX,BX
DisplayMountedHDDList_Loop:

    PUSH BX
    CMP byte [HDD0_Attribute+BX],0x80
    JB No_Display_ImageName

    PUSH BX					; Display HDDx : 
    printstr_w STR_HDD
    POP BX
    MOV AX,BX
    PUSH BX
    MOV BL,7    			; Light Grey
    CALL BIOS_PrintAX_Dec
    printstr_w STR_DDOT
    POP BX

    MOV SI,PMCFG_HDD0Size

    MOV CL,4
    SHL BX,CL
    ADD SI,BX
    PUSH SI
   
    LODSW  			 	  ; Load the Disk Size
    MOV BL,7    		  ; Light Grey
    CALL BIOS_PrintAX_Dec ; Display the Disk Size
    MOV SI,STR_MB
    CALL BIOS_Printstrc   ; Display 'Mb'
    printstr_w STR_SPACE   
   
    POP SI
    ADD SI,2
    CALL BIOS_Printstrc ; Display the Disk name   
    printstr_w STR_CRLF   

No_Display_ImageName:
    POP BX
    INC BX
    CMP BX,4 ; 4 HDD Images max
    JNE DisplayMountedHDDList_Loop
	RET ; Display_DisksList End
%endif

;STR_DetIRQ DB 'IRQ ',0
;STR_Detected DB 'Detected:',0

 ;Detect the PicoMEM jumpers position
PM_DetectIRQ:
; Hook Test Interrupts
%if PM_VERSION = 1
    CMP	byte [BV_IRQ],0		; 06/07/24 : Skip IRQ Detection if already set in config.txt
	JE PM_DoDetectIRQ
	RET
%endif
PM_DoDetectIRQ:	

;    printstr_w STR_DetIRQ
    CLI

    MOV AX,8+3
	MOV DX,PM_TestInt03h
	MOV BX,OldIrq3h
	CALL BIOS_HookIRQ

    MOV AX,8+5
	MOV DX,PM_TestInt05h
	MOV BX,OldIrq5h
	CALL BIOS_HookIRQ

    MOV AX,8+6
	MOV DX,PM_TestInt06h
	MOV BX,OldIrq6h
	CALL BIOS_HookIRQ

    MOV AX,8+7
	MOV DX,PM_TestInt07h
	MOV BX,OldIrq7h
	CALL BIOS_HookIRQ

; Enable the Interrupts
	IN AL,21h
	MOV CS:[PM_I],AL	; Save the IRQ Mask register
	AND AL,00010111b    ; IRQ 3+5+6+7 Mask
	OUT 21h,AL

; Ask the Pico to send all the possible IRQ and wait for them
    STI
	
    MOV AH,CMD_SendIRQ
    XOR BX,BX
	CALL PM_SendCMD         ; Do CMD_SendIRQ Command
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end	

	MOV CX,0
WaitTestIRQ:

	CMP byte [BV_IRQ],0
	JNE IRQ_Detected
	LOOP WaitTestIRQ

IRQ_Detected:

    CLI

    MOV AH,CMD_IRQAck
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd
	
; Restore the Interrupt Controller Mask	
   	MOV AL,CS:[PM_I]	; Get the Previous IRQ Mask
	OUT 21h,AL

; Restore IRQ Interrupts

    MOV AX,8+3
	MOV BX,OldIrq3h
	CALL BIOS_RestoreIRQ

    MOV AX,8+5
	MOV BX,OldIrq5h
	CALL BIOS_RestoreIRQ

    MOV AX,8+6
	MOV BX,OldIrq6h
	CALL BIOS_RestoreIRQ

    MOV AX,8+7
	MOV BX,OldIrq7h
	CALL BIOS_RestoreIRQ

	STI	
	RET

; Interrupt used to detect the HW IRQ number
; Return the Physical IRQ Number
PM_TestInt03h:  ; (serial port 2 interrupt)
;    MOV byte CS:[BV_IRQ],3
	OR byte CS:[BV_HWIRQ],00001000b	; IRQ 3 Detected
	JMP IRQ_Test_End
PM_TestInt05h:  ; (XT HDD port interrupt)
;    MOV byte CS:[BV_IRQ],5
	OR byte CS:[BV_HWIRQ],00100000b	; IRQ 5 Detected		
	JMP IRQ_Test_End
PM_TestInt06h:  ; (serial port 1 interrupt)
;    MOV byte CS:[BV_IRQ],6
	OR byte CS:[BV_HWIRQ],01000000b ; IRQ 6 Detected
	JMP IRQ_Test_End		
PM_TestInt07h:  ; (parallel printer interrupt)
;    MOV byte CS:[BV_IRQ],7
	OR byte CS:[BV_HWIRQ],10000000b  ; IRQ 7 Detected
IRQ_Test_End:
	PUSH AX
	MOV AL,20h
	OUT 20h,AL
	POP AX
	IRET

; Install the Boot Strap IRQ and move the Old Boot Strap to Int 18h
PM_InstallIRQ19:
;	CMP byte CS:[PMCFG_PMBOOT],1  ; Now it is installth all the time and Call the old one if needed
;	JNE PM_NotInstallBootStrap

%if Display_Intermediate=1
	printstr_w STR_InstInt19h
%endif

	MOV byte CS:[Int19h_Counter],0
    MOV AX,19h
	MOV DX,PM_Int19h
	MOV BX,OldInt19h
	CALL BIOS_HookIRQ
	
%if Display_Intermediate=1	
	MOV BL,07
	MOV AX,[OldInt19h+2]
	CALL BIOS_PrintAX_Hex
	printstr_w STR_DDOTNS		
	MOV AX,[OldInt19h]
	CALL BIOS_PrintAX_Hex
%endif

PM_NotInstallBootStrap:
	RET ; PM_InstallIRQ19 End

; ** Install the IRQ 9 Code (Keyboard redirection)
PM_InstallIRQ9: ; Int9h is the Hardware IRQ 1

; Check it IRQ 1 is redirected to the PicoMEM
	PUSH ES
    XOR AX,AX
	MOV ES,AX			 ; IRQ Vectors are at Segment 0

;	MOV AX,ES:[9*4+2]
;	MOV BL,07
;	call BIOS_PrintAX_Hex
	
	PUSH CS
	POP AX
	CMP ES:[9*4+2],AX	 ; IRQ 7 Redirected to PicoMEM BIOS ?
	JE PM_NotInstallIRQ9 ; Yes, Not install again

    MOV AX,09h
	MOV DX,PM_Int09h
	MOV BX,OldInt9h
	CALL BIOS_HookIRQ

PM_NotInstallIRQ9:
    POP ES
	RET ; PM_InstallIRQ9 End

; ** Disable the Mouse (Must be enabled by the Mouse Driver) **
PM_Disable_Mouse:

	MOV AH,CMD_Mouse_Disable	; Ask to disable the Mouse
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd          ; !! WARNING, Crash if the Pico command does not end

	RET	;PM_Disable_Mouse End

; ** Install the PicoMEM Hardware Interrupt **
PM_InstallIRQ3_5_7:

    PUSH CS
	POP DS

	CMP byte [BV_IRQ],0
	JNE HWIRQ_DoInstall
	RET
	
HWIRQ_DoInstall:	
    XOR AX,AX
	MOV ES,AX
	PUSH CS
	POP AX
	
    CMP byte [BV_IRQ],3
	JE PM_InstallIRQ3

    CMP byte [BV_IRQ],5
	JE PM_InstallIRQ5

    CMP byte [BV_IRQ],6
	JE PM_InstallIRQ6	

; Do IRQ 7 Install
	CMP ES:[(8+7)*4+2],AX	; IRQ 7 Redirected to PicoMEM BIOS ?
	JNE Do_IRQ7_Install
	; IRQ 7 Already installed
	RET	
	
Do_IRQ7_Install:
    MOV AX,8+7
	MOV DX,PM_Int
	MOV BX,OldIrq7h
	CALL BIOS_HookIRQ
    CLI

	IN AL,21h
	AND AL,01111111b    ; IRQ 7 Mask

	JMP HWIRQ_InstallEnd

PM_InstallIRQ5:

	CMP ES:[(8+5)*4+2],AX	; IRQ 5 Redirected to PicoMEM BIOS ?
	JNE Do_IRQ5_Install
	; IRQ 5 Already installed
	RET

Do_IRQ5_Install:
    MOV AX,8+5
	MOV DX,PM_Int
	MOV BX,OldIrq5h
	CALL BIOS_HookIRQ
    CLI	

	IN AL,21h
	AND AL,11011111b    ; IRQ 5 Mask
	
	JMP HWIRQ_InstallEnd

PM_InstallIRQ6:

	CMP ES:[(8+5)*4+2],AX	; IRQ 5 Redirected to PicoMEM BIOS ?
	JNE Do_IRQ6_Install
	; IRQ 5 Already installed
	RET

Do_IRQ6_Install:
    MOV AX,8+6
	MOV DX,PM_Int
	MOV BX,OldIrq6h
	CALL BIOS_HookIRQ
    CLI	

	IN AL,21h
	AND AL,10111111b    ; IRQ 6 Mask
	
	JMP HWIRQ_InstallEnd
	
	
PM_InstallIRQ3:

	CMP ES:[(8+3)*4+2],AX	; IRQ 5 Redirected to PicoMEM BIOS ?
	JNE Do_IRQ3_Install
	; IRQ 3 Already installed
	RET
	
Do_IRQ3_Install:
    MOV AX,8+3
	MOV DX,PM_Int
	MOV BX,OldIrq3h
	CALL BIOS_HookIRQ
    CLI

	IN AL,21h
	AND AL,11110111b    ; IRQ 3 Mask

HWIRQ_InstallEnd:
	OUT 21h,AL		; Enable the IRQ

	STI
	RET

BIOS_GetFDDNb:

; BIOS Equipment Mask :
;   Bits 7-6: Number of floppy drives
;     00b = 1 floppy drive
;     01b = 2 floppy drives
;     10b = 3 floppy drives
;     11b = 4 floppy drives
;   Bit 0: Boot floppy
;     0b = not installed
;     1b = installed

    MOV AX,040h
	MOV ES,AX

; Get the Number of Floppy on the PC
    MOV AL,ES:[BIOSVAR_EQUIP]
	TEST AL,1
	JZ PC_NoFDD
	MOV CL,6
	SHR AL,CL
	INC AL
	XCHG AL,AH
	JMP PC_FDDFound

PC_NoFDD:
    MOV AH,0
PC_FDDFound:
    MOV AL,ES:[BIOSVAR_EQUIP]
	RET

; Update the FDD Count in the BIOS Variable based on the current Nb and FDD0/FDD1 Enable
; !! Must be called only one time

FDD_UpdateBIOSVar:
    PUSH ES
	MOV AX,040h
	MOV ES,AX

;	printchar_w 'n'
;	MOV BL,7
;	MOV AL,CS:[New_FloppyNB]
;	CALL BIOS_PrintAL_Hex
;	printchar_w '-' 

	MOV AL,CS:[New_FloppyNB]
	CMP AL,0
	JNE Floppy_Not0
; Nb of Floppy drive = 0
	AND byte ES:[BIOSVAR_EQUIP],00111110b	; Clear Bit 0 and 6-7 : No Floppy

;	MOV BL,7
;	MOV AL,ES:[BIOSVAR_EQUIP]
;	CALL BIOS_PrintAL_Hex
;	printchar_w ' ' 

	POP ES
	RET
Floppy_Not0:
	CMP AL,1
	JNE Floppy_Is2
; Nb of Floppy drive = 1	
	AND byte ES:[BIOSVAR_EQUIP],00111110b ; Clear Bit 0 and 6-7 : No Floppy
    OR byte ES:[BIOSVAR_EQUIP],00000001b  ; Set to 1 Floppy	

;	MOV BL,7
;	MOV AL,ES:[BIOSVAR_EQUIP]
;	CALL BIOS_PrintAL_Hex
;	printchar_w ' ' 

	POP ES
	RET	
Floppy_Is2:
; Nb of Floppy drive = 2
	AND byte  ES:[BIOSVAR_EQUIP],00111110b ; Clear Bit 0 and 6-7 : No Floppy
    OR byte ES:[BIOSVAR_EQUIP],01000001b  ; Set to 2 Floppy
	
;	MOV BL,7
;	MOV AL,ES:[BIOSVAR_EQUIP]
;	CALL BIOS_PrintAL_Hex
;	printchar_w ' ' 

	POP ES
	RET


; Display the number of Floppy reader from the BIOS Variables
Display_FloppyNB:
    
	PUSH AX

; Get the Number of Floppy on the PC
    MOV AH,ES:[BIOSVAR_EQUIP]
	TEST AH,1
	JZ DF_NoFDD
	MOV CL,6
	SHR AH,CL
	INC AH
	JMP DF_Ok
DF_NoFDD:
    MOV AH,0
DF_Ok:

	MOV BL,7
	MOV AL,AH
	CALL BIOS_PrintAL_Hex
	
	POP AX
	
	RET	;Display_FloppyNB End

;--------------------------------------------------------------------------------------------------
; Wait for a Key during x second and display a counter at the current position
; Warning: does not work on a Old IBM PC as timer/KB not configured during extention ROM start
; Input : CX Nb of seconds
; Output 
; C Flag  : Key was presed, return in AL
; C Clear : Delay expired
;--------------------------------------------------------------------------------------------------
WaitKey_xs_Counter:

	PUSH CX
	MOV BH,PAGE
	MOV AH,03h  ; Read Cursor Position (DL DH)
	INT 10h
	POP CX
	
WaitKey_xs_Counter_Loop:	
	PUSH CX		; Save the time counter

	CALL BIOS_SetCursorPosDX
	PUSH DX		; Save the cursor position
	
	MOV AX,CX

    MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Dec	; Update the counter display

	CALL WaitKey_1s

	POP DX
	POP CX
	JC WaitKey_xs_Counter_End  ; Key was pressed : Stop the counter
	
	LOOP WaitKey_xs_Counter_Loop
	
WaitKey_xs_Counter_End:

; Clean the counter
	PUSH AX
	CALL BIOS_SetCursorPosDX
	printstr_w STR_SPACECRLF
	POP AX

	RET

;***********************************************************************************************
;*                             IRQ 3, 5 or 7 : PicoMEM Hardware interrupt                      *
;***********************************************************************************************

; Status > 
%define  IRQ_Stat_REQUESTED     1        ; Set by the Pico, IRQ Requested
%define  IRQ_Stat_INPROGRESS 	2        ; Set by the BIOS, IRQ In progress (Accepted and Started)
%define  PM_IRQ_Stat_COMPLETED		3    ; Set by the BIOS, IRQ Completed Ok

%define  IRQ_Stat_WRONGSOURCE   0x11 ; 17 Set by the BIOS, IRQ Completed, Wrong Source
%define  IRQ_Stat_DISABLED      0x12 ; 18 Ths interrupt is disabled in the PIC
%define  IRQ_Stat_INVALIDARG    0x13 ; 19 Invalid Argument Error
%define  IRQ_Stat_SameIRQ       0x14 ; 20 Asked to call the same IRQ
%define  IRQ_Stat_ALREADY_INP   0x15 ; 21 IRQ was already in progress

%define ISR_MASK_DMACOPY 1     ; Bit 0 : DMA Copy IRQ
%define ISR_MASK_SB      2     ; Bit 1 : Sound Blaster IRQ
%define ISR_MASK_NE2000  4     ; Bit 2 : NE2000
%define ISR_MASK_MOUSE   8     ; Bit 3 : Mouse IRQ
%define ISR_MASK_KEYBOARD 16   ; Bit 4 : Keyboard IRQ
%define ISR_MASK_PCCMD    32   ; Bit 5 : Pico Command IRQ


; As it is an HW IRQ, I Flag is cleared : 
; It can't be interrupted except if/When annother IRQ code is called and can enable again the IRQ (Like Mod Master XT)
; The IRQ code can acknowledge everything and enable the IRQ (Like Mod Master XT)
PM_Int:
		MOV byte CS:[PM_ISR],1		 ; Declare the PicoMEM IRQ in Service

		PUSH BX
		PUSH AX

        INC byte CS:[IRQ_CNT]		; Increment can be done here as no IRQ can occur here
		MOV AL,20h		; Allow this HW interrupt and lower priority ones to be requested again
		OUT 20h,AL

%if DISPLAY_IRQNb=1
		push_all_exceptbx
		MOV AL,'i'
		MOV BL,R_BK
		CALL BIOS_Printchar
		MOV BL,7
;		CALL BIOS_PrintAX_Hex	; Display KB then PCCMD
		pop_all_exceptbx	
%endif

; Tell the PM IRQ code that other PicoMEM IRQ can be requested
		MOV byte CS:[PM_IRR],0		 ; PM IRQ no more Requested

IRQ_Start_Requested_IRQ:  ; Will check for Requested interrupt/Command and execute them
        CLI		; Clear again the interrupts, in case a previously called interrupt code enabled them.

		MOV AX,word CS:[IRQ_R_DMACOPY]   ; AL: IRQ_R_DMACOPY AH: IRQ_R_AUDIO
	    OR AX,AX
		JZ IRQ_NO_SB_DMA

		OR AL,AL			; Select DMACOPY or SB
		JNZ IRQ_DO_DMACOPY
		XOR BX, BX
		MOV BL,AH			; IRQ_R_AUDIO contains the IRQ number > Set it to BX
		MOV byte CS:[IRQ_R_AUDIO],0  ; Clear the Audio "IRR"
		MOV AL,2			; Mask to set the Audio ISR bit
		JMP IRQ_DO_HW_IRQ
		
IRQ_NO_SB_DMA:
		MOV AX,word CS:[IRQ_R_NE2000]   ; AL: IRQ_R_NE2000 AH: IRQ_R_MOUSE
	    OR AX,AX
		JZ IRQ_NO_NE2000_MOUSE
		OR AL,AL			; Select NE2000 or MOUSE
		JZ IRQ_DO_MOUSE
		XOR BX, BX
		MOV BL,AL			; IRQ_R_NE2000 contains the IRQ number > Set it to BX
		MOV byte CS:[IRQ_R_NE2000],0  ; Clear the NE2000 "IRR"
		MOV AL,4			; Mask to set the NE2000 ISR bit
		JMP IRQ_DO_HW_IRQ

IRQ_NO_NE2000_MOUSE:
		MOV AX,word CS:[IRQ_R_KEYBOARD]   ; AL: IRQ_R_KEYBOARD AH: IRQ_R_PCCMD
	    OR AX,AX
		JZ IRQ_NO_KEYBOARD_PCCMD
		OR AL,AL
		JNZ IRQCMD_Keyboard
        JMP IRQCMD_StartPCCMD

IRQ_NO_KEYBOARD_PCCMD:

PM_Int_End:
        DEC byte CS:[IRQ_CNT]
		JNZ PM_Int_NoAck

		PUSH DX
		MOV DX,2A0h
		MOV AL,1
		OUT DX,AL		; Command 1 : PM IRQ Ack > The PicoMEM will clear the PM_ISR variable
		POP DX

PM_Int_NoAck:
		
		POP AX
		POP BX
		IRET

IRQ_DO_HW_IRQ:
;No need for SHL, done by the Pico (IRQ Nb is x2)
		MOV byte CS:[PM_IRR],0		    ; If another IRR was sent in between, acknowledge again
		CALL [CS:HW_IRQ_Table+BX]     	; Jump to the HW interrupt start code
		XOR byte CS:[DEV_ISR],AL        ; Clear the ISR of the device (If no Ack arrived) SB: 2 NE2000: 4

        JMP IRQ_Start_Requested_IRQ     ; Now move to the other Interrupt request in the list, if another IRQ request arrived in between

IRQ_DO_DMACOPY:

		MOV byte CS:[PM_IRR],0		    ; If another IRR was sent in between, acknowledge again
		OR byte CS:[DEV_ISR],ISR_MASK_DMACOPY  ; Set the DMA Copy command ISR
		MOV byte CS:[IRQ_R_DMACOPY],0 	 	   ; Set IRR to 0	
; Copy the Data, as requested by the Pico virtual DMA code

        PUSH DS
		PUSH SI
		PUSH ES
		PUSH DI
		PUSH CX

        LES DI,CS:[IRQ_VAR+4]  	    ; Read DI then ES
	    MOV CX,word CS:[IRQ_VAR+8]  ; Read the Nb of Word to move
        LDS SI,CS:[IRQ_VAR]    	    ; Read SI then DS
		
		CLD
%if 0		
		push_all_exceptbx
		PUSH ES
		PUSH CX
		MOV AL,'c'
		MOV BL,R_BK
		CALL BIOS_Printchar
		MOV BL,7
		POP AX
		CALL BIOS_PrintAX_Hex   ; Display DMA then SB
		POP AX
		CALL BIOS_PrintAX_Hex   ; Display DMA then SB		
		pop_all_exceptbx
%endif

		JCXZ COPY_END   ;End if nothing to copy
		SHR CX,1
		JNC COPY_EVEN
        MOVSB		    ;Move the first byte if needed
COPY_EVEN:
        JCXZ COPY_END   ;End if remains 0
	    REP MOVSW
COPY_END:		

; ToDo : Update the DMA controller registers

        MOV byte CS:[BV_DMANoUpdate],1  ; Tell the PicoMEM to ignore this DMA register update
        XOR AX,AX
		OUT 0Ch,AL               ; Clear DMA flipflop (To read the values in the correct order)

; DMA 1
; Don't update the  DMA page
; Update the DMA offset
		MOV AX,CS:[IRQ_VAR+10]
        OUT 02h,AL	; LSB
		XCHG AH,AL
		OUT 02h,AL	; MSB
		XCHG AH,AL
; Update the length
		MOV AX,CS:[IRQ_VAR+12]
		OUT 03h,AL
		XCHG AH,AL
		OUT 03h,AL

%if 0	
		push_all_exceptbx
		MOV AL,'c'
		MOV BL,R_BK
		CALL BIOS_Printchar
		MOV BL,7
        MOV AX,CS:[IRQ_VAR+10]
		CALL BIOS_PrintAX_Hex   ; Display DMA then SB
		MOV AX,CS:[IRQ_VAR+12]
		CALL BIOS_PrintAX_Hex   ; Display DMA then SB		
		pop_all_exceptbx
%endif
        MOV byte CS:[BV_DMANoUpdate],0
	
        PUSH DX
		MOV DX,2A0h
		MOV AL,2
		OUT DX,AL		; Command 2 : PM DMA Copy Ack > Tell the pico that the requested copy is finished.
		POP DX

        POP CX
		POP DI
		POP ES
		POP SI
		POP DS
		XOR byte CS:[DEV_ISR],ISR_MASK_DMACOPY  ; Clear the DMA Copy command ISR
		
		JMP   IRQ_Start_Requested_IRQ     ; Now move to the other Interrupt request in the list, if another IRQ request arrived in between 

IRQCMD_StartPCCMD:				; Ask the Pico to "Take the controle"
		MOV byte CS:[PM_IRR],0		    ; If another IRR was sent in between, acknowledge again
		OR byte CS:[DEV_ISR],ISR_MASK_PCCMD  ; Set the PC command ISR
		MOV byte CS:[IRQ_R_PCCMD],0 	 	 ; Set IRR to 0
        push_all_exceptbx		; !! Save all the registers !

%if DISPLAY_IRQNb=1		
		MOV AL,'c'
		MOV BL,7
		CALL BIOS_Printchar			
%endif

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
		PUSH DX
		MOV DX,2A5h
		MOV AL,IRQ_StartPCCMD
		OUT DX,AL
		POP DX
%endif
 
        MOV byte CS:[BV_PCCMD_SOURCE],2
        CALL BIOS_DoPCCMD
        MOV byte CS:[BV_PCCMD_SOURCE],0

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
		PUSH DX
		MOV DX,2A6h
		MOV AL,IRQ_StartPCCMD
		OUT DX,AL
		POP DX
%endif
		pop_all_exceptbx
		XOR byte CS:[DEV_ISR],ISR_MASK_PCCMD   ; Clear the PC command ISR
		JMP  IRQ_Start_Requested_IRQ     ; Now move to the other Interrupt request in the list, if another IRQ request arrived in between	

; Mouse IRQ Event > Send the Data to the Mouse Interrupt Driver
IRQ_DO_MOUSE:
		MOV byte CS:[PM_IRR],0		    ; If another IRR was sent in between, acknowledge again
		OR byte CS:[DEV_ISR],ISR_MASK_MOUSE  ; Set the Mouse command ISR
		MOV byte CS:[IRQ_R_MOUSE],0 	 	 ; Set IRR to 0		
        push_all_exceptbx

%if DISPLAY_IRQNb=1
		MOV AL,'m'
		MOV BL,7
		CALL BIOS_Printchar
%endif

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
		PUSH DX
		MOV DX,2A5h
		MOV AL,IRQ_USBMouse
		OUT DX,AL
		POP DX		
%endif

		XOR AX,AX
		MOV AL,CS:[IRQ_MOUSE_B]
		MOV DX,AX		; Buttons in DL
		MOV AL,CS:[IRQ_MOUSE_X]
		CBW
		MOV BX,AX		; Delta x in BX
		MOV AL,CS:[IRQ_MOUSE_Y]
		CBW
		MOV CX,AX		; Delta Y in CX

		MOV AX,0060h	; PicoMEM custom IRQ : Update Mouse
		INT 33h			; call Mouse IRQ		> Crash during DOS Boot

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
		PUSH DX
		MOV DX,2A6h
		MOV AL,IRQ_USBMouse
		OUT DX,AL
		POP DX		
%endif

		pop_all_exceptbx
		XOR byte CS:[DEV_ISR],ISR_MASK_MOUSE  ; Clear the Mouse command ISR
		JMP  IRQ_Start_Requested_IRQ          ; Now move to the other Interrupt request in the list, if another IRQ request arrived in between

IRQCMD_Keyboard:		; KB Not coded
		MOV byte CS:[PM_IRR],0		    ; If another IRR was sent in between, acknowledge again
		OR byte CS:[DEV_ISR],ISR_MASK_KEYBOARD  ; Set the Keyboard command ISR		
		MOV byte CS:[IRQ_R_KEYBOARD],0 	 	 ; Set IRR to 0

%if DISPLAY_IRQNb=1	
        push_all_exceptbx
		MOV AL,'k'
		MOV BL,7
		CALL BIOS_Printchar			
        pop_all_exceptbx
%endif

		XOR byte CS:[DEV_ISR],ISR_MASK_KEYBOARD  ; Clear the Keyboard command ISR
		JMP IRQ_Start_Requested_IRQ     ; Now move to the other Interrupt request in the list, if another IRQ request arrived in between

; ********************  Hardware IRQ code **********************

IRQCMD_None:
		RET

IRQCMD_IRQ3:             ; >> Emulate the Physical IRQ 3  (!! STARTED with a CALL)
		PUSH AX
		IN AL,21h
		AND AL,00001000b
		JNZ SkipInt
		POP AX

%if DISPLAY_IRQNb=1
		push_all_exceptbx
		MOV AL,'3'
		MOV BL,7
		CALL BIOS_Printchar
		pop_all_exceptbx
%endif

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
        PUSH AX
		PUSH DX
		MOV DX,2A5h
		MOV AL,IRQ_IRQ3
		OUT DX,AL
		POP DX
		POP AX
%endif

		OR byte CS:[DEV_ISR],AL        ; Set the ISR of the device (If no Ack arrived) SB: 2 NE2000: 4
		Int 3+8

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
        PUSH AX
		PUSH DX
		MOV DX,2A6h
		MOV AL,IRQ_IRQ3
		OUT DX,AL
		POP DX
		POP AX
%endif
		RET

IRQCMD_IRQ4:             ; >> Emulate the Physical IRQ 4    (!! STARTED with a CALL)
		PUSH AX
		IN AL,21h
		AND AL,00010000b
		JNZ SkipInt
		POP AX	

		OR byte CS:[DEV_ISR],AL        ; Set the ISR of the device (If no Ack arrived) SB: 2 NE2000: 4
		Int 4+8

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
        PUSH AX
		PUSH DX
		MOV DX,2A6h
		MOV AL,IRQ_IRQ4
		OUT DX,AL
		POP DX	
		POP AX
%endif		
		RET
		
SkipInt: ; Skip > Return Disabled IRQ code
	    POP AX
		RET
		
IRQCMD_IRQ5:             ; >> Emulate the Physical IRQ 5
		PUSH AX
		IN AL,21h		; Verify if masked > Skip it
		AND AL,00100000b
		JNZ SkipInt
		POP AX

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
		PUSH AX
		PUSH DX
		MOV DX,2A5h
		MOV AL,IRQ_IRQ5
		OUT DX,AL
		POP DX
		POP AX
%endif

		OR byte CS:[DEV_ISR],AL        ; Set the ISR of the device (If no Ack arrived) SB: 2 NE2000: 4
		Int 5+8

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
		PUSH AX
		PUSH DX
		MOV DX,2A6h
		MOV AL,IRQ_IRQ5
		OUT DX,AL
		POP DX
		POP AX
%endif			

		RET

IRQCMD_IRQ6:              ; >> Emulate the Physical IRQ 6
		PUSH AX
		IN AL,21h		  ; Verify if masked > Skip it
		AND AL,01000000b
		JNZ SkipInt
		POP AX	
		
		OR byte CS:[DEV_ISR],AL        ; Set the ISR of the device (If no Ack arrived) SB: 2 NE2000: 4
		Int 6+8

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
		PUSH AX
		PUSH DX
		MOV DX,2A6h
		MOV AL,IRQ_IRQ6
		OUT DX,AL
		POP DX	
		POP AX	
%endif
		RET

IRQCMD_IRQ7:             ; >> Emulate the Physical IRQ 7
		PUSH AX
		IN AL,21h		; Verify if masked > Skip it
		AND AL,10000000b
		JNZ SkipInt		
		POP AX	
		
		OR byte CS:[DEV_ISR],AL        ; Set the ISR of the device (If no Ack arrived) SB: 2 NE2000: 4
		Int 7+8

%if IRQ_DEBUGPORT=1
; DEBUG Out Port
		PUSH AX
		PUSH DX
		MOV DX,2A6h
		MOV AL,IRQ_IRQ7
		OUT DX,AL
		POP DX	
		POP AX	
%endif			
		RET

IRQCMD_IRQ9:
IRQCMD_IRQ10:

		RET

; Include the keyboard interrupt handler for IRQ 9 (hardware IRQ 1)
#include "int9_kbd.asm"

; Include the Int13h PicoMEM BIOS Functions (in IRQ 13h)
#include "int13_pmbios.asm"

; Include the Int13h Disk BIOS Emulation (IRQ 13h)
%include "int13_disk.asm"

PM_Int21h:
    PUSHF
    PUSH DI		; Changed by SaveRegs
	PUSH DS
	PUSH AX
	PUSH BX
	PUSH CX
	PUSH DX

    PUSH CS
	POP DS
	
	CALL PM_SaveRegs		  ; Send the registers values to the PicoMEM

	CALL PM_WaitCMDEnd		  ; Check if previous command ended

    MOV AH,CMD_Int21h
	CALL PM_SendCMD           ; Do CMD_Int21h Command	(Don't wait previous command end)

	CALL PM_WaitCMDEnd		  ; Wait the Command end

    POP DX
	POP CX
	POP BX
	POP AX
	POP DS
	POP DI
	POPF

	PUSHF					 ; Add resisters to the call to simulate an Interrupt
	CALL FAR [CS:OldInt21h]   ; CALL the saved other IRQ13h !
	RETF	2 			     ; return from int with current flags
	
PM_Int2Fh:

    ; --- Windows start/exit broadcast snoop ---
    ; AX=1605h: Windows is initializing (sent BEFORE DOSX hooks any
    ;           vector, so we can set the flag in time for PMBIOS_Int1C
    ;           to bail on the next tick).
    ; AX=1606h: Windows is exiting -- clear the flag.
    ; In both cases we just snoop and fall through to the existing chain
    ; so other TSRs / DOS still get the broadcast.
    CMP AX,1605h
    JNE PM_Int2Fh_NotWinStart
    MOV byte CS:[WIN_LOADED],1
PM_Int2Fh_NotWinStart:
    CMP AX,1606h
    JNE PM_Int2Fh_NotWinExit
    MOV byte CS:[WIN_LOADED],0
PM_Int2Fh_NotWinExit:

    PUSHF
    
    PUSH DI		; Changed by SaveRegs
	PUSH DS
	PUSH AX
	PUSH BX
	PUSH CX
	PUSH DX

    PUSH CS
	POP DS
	
	CALL PM_SaveRegs		  ; Send the registers values to the PicoMEM
	
	CALL PM_WaitCMDEnd		  ; Check if previous command ended

    MOV AH,CMD_Int2Fh
	CALL PM_SendCMD           ; Do CMD_Int21h Command	(Don't wait previous command end)

	CALL PM_WaitCMDEnd		  ; Wait the Command end

    POP DX
	POP CX
	POP BX
	POP AX
	POP DS
	POP DI

	POPF

	PUSHF					 ; Add resisters to the call to simulate an Interrupt
	CALL FAR [CS:OldInt2Fh]   ; CALL the saved other IRQ2Fh !

	RETF	2 			     ; return from int with current flags

;***********************************************************************************************
;*                                   IRQ 19h  (Boot Strap)                                     *
;*  BOOT Strap Loader and PicoMEM Setup   												       *
;*  Track 0, sector 1 is loaded into address 0:7C00 and control is transferred there           *
;*  https://stanislavs.org/helppc/int_19.html                                                  *
;***********************************************************************************************

;STR_FDDFist DB 'Boot FDD',0x0D,0x0A,0
;STR_HDDFist DB 'Boot HDD',0x0D,0x0A,0

STR_PMBoot   DB 'PicoMEM Boot: ',0
STR_Legacy   DB 'Start Legacy Boot Strap',0x0D,0x0A,0
STR_PressA_B DB 'Press A to Boot from Floppy, C for CD-ROM, B for Basic/ROM ',0
STR_CDBoot   DB 0x0D,0x0A,'CD: Boot... ',0x0D,0x0A,0
STR_CDNoBoot DB 0x0D,0x0A,'CD: not bootable (no El Torito catalog / no disc)',0x0D,0x0A,0
STR_CDEmu    DB 0x0D,0x0A,'CD: HDD-emulation boot image not supported',0x0D,0x0A,0
STR_CDFDD    DB 0x0D,0x0A,'CD: floppy-emulation boot (A: = CD boot image)',0x0D,0x0A,0
STR_InsertDisk DB 0x0D,0x0A,'> No Boot Disk found - Insert a Floppy and Press any Key.',0
STR_BAD_MBR  DB ': Incorrect MBR ',0
STR_Int13hError DB ': Int13h Error ',0 
STR_BOOT     DB ': Boot... ',0x0D,0x0A,0
STR_Int19_NotFirst DB 'Boot failed : Select another image or disable the PicoMEM Boot Code.',0x0D,0x0A,0

PM_Int19h:

; No Need to save registers, it is "not" a normal Interrupt	
	PUSH CS
	POP DS

    CMP byte [Int19h_Counter],0		; This counter is initialized at each Init
	JZ Int19_FirstTime
; If the Int19h was called more than one time or Boot Failed
PM_Int19h_BootFailed:
    PICOPOST 0x85
    printstr_w STR_Int19_NotFirst
    JMP Int19_Do_Setup              ; No need to display the Header again but do the Config

Int19_FirstTime:
	INC byte [Int19h_Counter]

    CMP byte [Int19h_SkipSetup],1	; If Not an Old BIOS, Skip the initialisation (Already done)
    JE PM_Int19h_skipconfig

Int19_Do_Setup:

	CALL PM_Do_Setup			; ****PicoMEM Detection, Config and Initialisation ***

PM_Int19h_skipconfig:	

;**********************************************************************
;****** Int19h : Real Begining of the Boot Sequence  (BootStrap) ******
;**********************************************************************
	printstr_w STR_PMBoot
	
; Neutralize stale SOFTWARE-interrupt vectors from the previous session.
; A hot reboot keeps RAM: POST rebuilds only the vectors it owns, so INT
; 2Fh / INT 33h can still point into the OLD OS's (now reused) memory.
; PMBIOS_Int1C calls both -- the 2Fh Windows poll is guarded by "segment
; != 0", which a stale vector PASSES: the call jumps into garbage from
; inside the INT 08h tick (before EOI) and the timer chain dies (frozen
; FreeDOS-CD boot countdown, 2026-07-17). Cleared HERE at the boot entry
; so warm and cold boots start identical: null until DOS / the mouse
; driver installs the real handlers.
	XOR AX,AX
	MOV ES,AX
	MOV word ES:[2Fh*4],AX
	MOV word ES:[2Fh*4+2],AX
	MOV word ES:[33h*4],AX
	MOV word ES:[33h*4+2],AX
	
	CMP byte [PMCFG_PMBOOT],1
	JE	Do_PM_BootStrap

    printstr_w STR_Legacy	; Display "Start Legacy Boto Strap"

    MOV AX,19h
	MOV BX,OldInt19h
	CALL BIOS_RestoreIRQ	; Restore the Legacy Boot Strap
	
%if Display_Intermediate=1	
	MOV AX,0
	MOV ES,AX
	MOV BL,07
	MOV AX,ES:[19h*4+2]
	CALL BIOS_PrintAX_Hex
	printstr_w STR_DDOTNS		
	MOV AX,ES:[19h*4]
	CALL BIOS_PrintAX_Hex
	printstr_w STR_CRLF
%endif	
	
	MOV DL,80h				; Try Boot from HDD0 (GLABIOS or maybe other)
	INT 19h

	XOR DX,DX 				; Try Boot from FDD0 (GLABIOS or maybe other)
	INT 19h

; If it returned : Fail
	
	JMP PM_Int19h_BootFailed

	POPF
	JMP FAR [CS:OldInt19h]  ; CALL the saved other IRQ19h !
	
; If returned from Int19h, try Floppy 2X
	
	
;**** Real Int19h Start ****

Do_PM_BootStrap:
    PICOPOST 0x50

	CMP byte CS:[BOOT_FDDFirst],1	; If A was pressed before, bypass the wait time
	JE Int19h_FDD_First

    printstr_w STR_PressA_B
	
	MOV CX,1
    CMP byte CS:[PMCFG_FastBoot],1
	JE SmallWait2
	MOV CX,3
SmallWait2:
	CALL WaitKey_xs_Counter

	CMP AL,'A'
	JE PM_Int19h_FDD
	CMP AL,'a'
	JE PM_Int19h_FDD
	CMP AL,'Q'
	JE PM_Int19h_FDD
	CMP AL,'q'
	JE PM_Int19h_FDD
	CMP AL,'B'
	JE PM_Int19h_BASIC
	CMP AL,'b'
	JE PM_Int19h_BASIC
	CMP AL,'C'
	JE PM_Int19h_CD
	CMP AL,'c'
	JE PM_Int19h_CD
	CMP AL,'R'
	JE PM_Reboot
	CMP AL,'r'
	JE PM_Reboot

	JMP PM_Int19h_Start
	
PM_Int19h_FDD:
    PICOPOST 0x52
	MOV byte CS:[BOOT_FDDFirst],1	; If A Pressed, force Boot on Floppy first
	JMP PM_Int19h_Start
PM_Int19h_BASIC:
    PICOPOST 0x51
	Int 18h							; Int 18h > Go to the Basic
	PM_Int19h_CD:
	CALL BOOT_CD					; Only returns on failure (message printed)
	JMP PM_Int19h_Start				; Fall back to the normal boot order
PM_Reboot:
    JMP 0F000h:0FFF0h		        ; Test Reboot
	
PM_Int19h_Start:
	
	CMP byte CS:[BOOT_FDDFirst],0
	JE Int19h_HDD_First

	MOV byte CS:[BOOT_FDDFirst],0	; Reset the FDD First variable
Int19h_FDD_First:
	XOR DL,DL
	MOV CX,2
	CALL BOOT_Drives	; Try to boot on 2 FDD

	MOV DL,80h			; Try to boot on 2 HDD (Was 4 Before)
	MOV CX,2	
	CALL BOOT_Drives

	JMP Int19h_DisplayInsertDisk
Int19h_HDD_First:
;    printstr_w STR_FDDFist

	MOV DL,80h			; Try to boot on 2 HDD
	MOV CX,2	
	CALL BOOT_Drives

Int19h_RetryFloppy:
	XOR DL,DL
	MOV CX,2
	CALL BOOT_Drives	; Try to boot on 2 FDD	

Int19h_DisplayInsertDisk:
	PUSH CS
	POP DS
    printstr_w STR_InsertDisk

	CALL WaitKey_Loop
	
	JMP Int19h_RetryFloppy

; Input  DL : First drive number
;        DX : The number of drive to test

;STR_BOOTLOOP DB 'BootLoop: ',0

BOOT_Drives:

Int19_BootLoop:
	PUSH CX				; Save drives counter

;	PUSH DX
;	printstr_w STR_BOOTLOOP
;	MOV AX,DX
;	CALL BIOS_PrintAX_Hex
;	POP DX

	PUSH DX				; Save drive number
	XOR AX,AX
	INT	13h				; Reset drive
	POP DX				; Restore drive number
	JC	Int19_Error

    PUSH DX
	XOR	AX,AX
	MOV	ES,AX				; Segment 07C0h
	MOV	AX,0201h			; AH=2 (read)  AL=1 sector
	MOV BX,7C00h      		; Read sector to ES:BX=0000:7C00
	MOV	CX,0001h			; CH=Cyl 0  CL=Sec 1
	XOR DH,DH				; DH=head 0 DL=Drive number
	INT	13h
	POP DX
	JC Int19_Error     ; CF=0 Error

Boot_ReadSectorOk:			; Jump to the BOOT Code
    PUSH DX

    CALL Display_DriveNumber
	
	CMP word ES:[7C00h+510],0AA55h
	JE Int19_BootSectorOk
	
	printstr_w STR_BAD_MBR

    POP DX					; Restore the Disk number
	OR DL,DL
	JNS Int19_BootSectorOk  ; If this is a Floppy, don't check the AA55h
	JMP Int19_NextDrive
	
Int19_BootSectorOk:
	printstr_w STR_BOOT

    POP DX					; Restore the Disk number
	XOR AX,AX
	MOV ES,AX
	MOV DS,AX
	JMP	0000h:7C00h         ; Jump to the boot/MBR code

Int19_NextDrive:
	POP CX					; Get Drives counter
	INC DL					; Go to the next drive
	LOOP Int19_BootLoop
	RET

Int19_Error:

; Error : Display the disk number and error code
    PUSH CX
	PUSH DX
	PUSH AX
    CALL Display_DriveNumber
	printstr_w STR_Int13hError
	POP AX
	MOV AL,AH
	CALL BIOS_PrintAL_Hex
	printstr_w STR_CRLF
	POP DX
	POP CX

    JMP Int19_NextDrive
;***********************************************************************
; BOOT_CD : El Torito boot from the firmware CD (0E0h)
;
; No-emulation discs (media 0): AH=4B01h fetches the boot spec packet
; (firmware parses the catalog), AH=42h loads the boot image at
; load_seg:0000 (07C0h when the catalog says 0), far-jump with DL=0E0h.
; Floppy-emulation discs (media 1-3, e.g. Win95 OSR2): arm the firmware
; drive-00h overlay (internal AH=0B0h) and boot A: normally -- see
; BOOT_CD_Emu below. Only returns on failure, after printing the reason
; (for floppy emulation "failure" includes the armed case: the caller's
; normal boot order then boots the overlay). Scratch (spec packet @0 +
; DAP @20) lives on the stack: CS is the option ROM, not writable RAM.
;***********************************************************************
BOOT_CD:
	PUSH BP
	MOV BP,SP
	SUB SP,40
	PUSH SS
	POP DS
	MOV SI,SP				; DS:SI -> stack scratch

	MOV AX,4B01h			; El Torito: get boot spec packet
	MOV DL,0E0h
	INT 13h
	JC BOOT_CD_NoBoot

	CMP word [SI+14],0		; boot image size (512-byte units)
	JE BOOT_CD_NoBoot		; 0 = data-only disc / no catalog
	CMP byte [SI+1],0		; media type 0 = no-emulation
	JNE BOOT_CD_Emu

; DAP at [SI+20]: load ceil(count/4) CD sectors (2048b) to load_seg:0000.
; Cap at 16 CD sectors (32KB) -- far above any real loader (isolinux
; asks for 4 x 512 bytes).
	MOV AX,[SI+14]
	ADD AX,3
	SHR AX,1
	SHR AX,1				; 512-byte units -> 2048-byte CD sectors
	CMP AX,16
	JBE BOOT_CD_CountOk
	MOV AX,16
BOOT_CD_CountOk:
	MOV word [SI+20],0010h	; DAP size 16 / reserved 0
	MOV [SI+22],AX			; CD sector count
	MOV word [SI+24],0		; buffer offset 0
	MOV AX,[SI+12]			; load segment from the spec packet
	MOV [SI+26],AX			; buffer segment
	MOV AX,[SI+4]
	MOV [SI+28],AX			; image LBA 15..0
	MOV AX,[SI+6]
	MOV [SI+30],AX			; image LBA 31..16
	XOR AX,AX
	MOV [SI+32],AX
	MOV [SI+34],AX			; LBA 63..32 = 0

	MOV BX,[SI+12]			; BX = load segment (regs preserved across our Int13h)
	ADD SI,20				; DS:SI -> DAP
	MOV AH,42h				; EDD extended read
	MOV DL,0E0h
	INT 13h
	JC BOOT_CD_NoBoot		; read error

	PUSH CS
	POP DS
	printstr_w STR_CDBoot	; (clobbers SI -- load segment already in BX)

	PUSH BX					; far "return address" = load_seg:0000
	XOR AX,AX
	PUSH AX
	MOV DS,AX
	MOV ES,AX
	MOV DL,0E0h				; El Torito: DL = the CD drive number
	RETF					; jump into the boot image
					  
; Media type 1-3 = floppy emulation (Tier 3): ask the firmware to overlay
; drive 00h with the CD's embedded boot-floppy image (internal AH=0B0h --
; it re-parses the catalog and flips FDD0_Attribute to 80h so PM_Int13h
; claims drive 0), then run the normal boot order floppy-first: the very
; next INT 13h reads on drive 0 come from the CD image, DL=00h at 7C00h
; exactly as the booted image expects. Media 4 (HDD emulation) stays
; unsupported. Overlay is dropped at the next CMD_DEV_Init (reboot).
BOOT_CD_Emu:
	CMP byte [SI+1],3		; DS=SS scratch still valid here
	JA BOOT_CD_EmuHDD		; 4 = HDD-emulation image: unsupported

	MOV AX,0B000h			; internal: arm the drive-00 floppy overlay
	MOV DL,0E0h
	INT 13h
	JC BOOT_CD_NoBoot		; catalog vanished / firmware refused

	PUSH CS
	POP DS
	printstr_w STR_CDFDD
	MOV byte CS:[BOOT_FDDFirst],1	; caller's PM_Int19h_Start boots A: first
	JMP BOOT_CD_Fail		; shared epilogue (SP restore + RET)
BOOT_CD_EmuHDD:
	PUSH CS
	POP DS
	printstr_w STR_CDEmu
	JMP BOOT_CD_Fail
BOOT_CD_NoBoot:
	PUSH CS
	POP DS
	printstr_w STR_CDNoBoot
BOOT_CD_Fail:
	MOV SP,BP
	POP BP
	RET
;***********************************************************************************************
;*                       PM_DoSetup : Full HW Detection and Setup code                         *
;*                                                                                             *
;*  Detect the Hardware, start the BIOS Setup code and initialise the Board                    *
;*                                                                                             *
;***********************************************************************************************

PM_Do_Setup:
    PICOPOST 0x60
	INC byte [BV_BootCount]

; *** POST: Read/Detect the PC Configuration informations, only during the First BOOT
    CMP byte [BV_BootCount],1
	JNE POST_Skip_PC_Config_Detect

    PICOPOST 0x61
; - Read the initial BIOS Disk infos
    CALL BIOS_GetPCDriveInfos
	
    PICOPOST 0x62
; - Read the PC Memory configuration
; ! Now do the memory config Detection is in the Boot Strap, to bypass the case when the BIOS or a board change the RAM layout.
; ! Need the PicoMEM commands to work to not crash
    CALL PM_Memory_FirstBoot

POST_Skip_PC_Config_Detect:

; *** POST : Check if the Memory changed (PM config file VS Physical)
;            If Changed, disable all the Memory settings
; Can change between reboot due to CF Swap or PC Shutdown with the PicoMEM still powered on
	CALL MEM_Check_MEM_Changed
    CALL MEM_Init 		; Re initialize the Memory allocation table (If the settings has been changed)

; *** First Display ***

    CALL SetText_80

    PICOPOST 0x63	
    CALL PM_Display_FullHeader

; ****** Code doing the Menu and the configuration  *****

	STI 				; Enable interrupts (for the Keyboard)

    PICOPOST 0x64
    printstr_w STR_PressS

	MOV CX,2
    CMP byte CS:[PMCFG_FastBoot],1
	JE SmallWait1_19h
	MOV CX,5
SmallWait1_19h:	
	CALL WaitKey_xs_Counter

	CMP AL,'A'
	JE A_Pressed_19h
	CMP AL,'a'
	JNE Test_S_19h
A_Pressed_19h:
	MOV byte CS:[BOOT_FDDFirst],1	; If A Pressed, force Boot on A
	
Test_S_19h:
	CMP AL,'S'
	JE S_Pressed_19h
	CMP AL,'s'
	JE S_Pressed_19h

    JMP BIOS_Final_Init

S_Pressed_19h:   ; *****   Setup menu
    PICOPOST 0x65
    CALL Do_BIOS_Menu
; Display again the Header after BIOS Menu Exit
    CALL PM_Display_FullHeader
	
BIOS_Final_Init:

    PICOPOST 0x40

%if DOS_COM=1		; Don't redirect IRQ if DOS (Debug)
	RET
%endif ; DOS_COM=1

; Finalize all the configs after the setup or Wait End

    CALL PM_FinalConfig
    CALL DisplayMEMConfig		; Display the Memory configuration after Final config

    PICOPOST 0x45

    CALL PM_InstallIRQ13		; Mount the Disks and Install the IRQ 13h
	CALL Display_DisksList

    PICOPOST 0x4F
	RET
; PM_Do_Setup End

;*******************  Various Display/Utilities code ************************
	
Display_DriveNumber:

	XOR AH,AH
	MOV AL,DL
	CMP DL,80h
    MOV SI,STR_FDD+3
	JB Display_FDD
	SUB AL,80h
    MOV SI,STR_HDD+3
Display_FDD: 

	PUSH AX
	CALL BIOS_Printstr
	POP AX

    MOV BL,7    			; Light Grey
	CALL BIOS_PrintAX_Dec

	RET	;Display_DriveNumber End

; PM_Int19h End

;Put all the registers to the BIOS Memory (Only the one needed)
;Need only 5 Registers plus FLAG for Int13h (To be changed)
; Save AX,BX,CX,DX DS,SI ES,DI, Flags 
; Change AX and DI
PM_SaveRegs:
	PUSH ES  ; Save ES that is needed for the Memory source/Destination
	PUSH DI
	
	PUSH ES
	PUSH CS
	POP ES
	CLD
	MOV DI,REG_AX  ; Regs table Offset
	STOSW          ; AX
	MOV AX,BX
	STOSW          ; BX
	MOV AX,CX
	STOSW          ; CX
	MOV AX,DX
	STOSW          ; DX
	MOV AX,DS
	STOSW          ; DS
	MOV AX,SI
	STOSW          ; SI
	POP AX         ; GET ES
	STOSW          ; ES
	POP AX         ; GET DI
	STOSW          ; DI
	PUSHF
	POP AX         ; GET Flags
	STOSW          ; Flags
	POP ES
	RET  ;PM_SaveRegs

PM_GetRegs_int13:
	PUSH CS
	POP DS
	MOV BX,[REG_BX]
	MOV CX,[REG_CX]
	MOV DX,[REG_DX]
	MOV AX,[REG_ES]
	MOV ES,AX
	MOV AX,[REG_DI]
	MOV DI,AX
	MOV AX,[REG_SI]
	MOV SI,AX
	MOV AX,[REG_FLAG]
	PUSH AX
	POPF
	MOV AX,[REG_DS]
	MOV DS,AX
	MOV AX,CS:[REG_AX]
	RET  ;PM_GetRegs_int13


;************************************************************************************************
;*                                    DISPLAY FUNCTIONS
;************************************************************************************************


PM_Display_FullHeader:

; *** First Display ***
    printstr_r  STR_PICOMEM
    printstr_lw STR_PICOMEMSTART	

PM_Display_Insof_NoHeader:
; *** POST: Display the PM Infos (Port, Address) and Boot Counter
	CALL PM_Display_Config

; *** Print the Peripherals status (PSRAM, SD, USB...)
    CALL PM_Display_Perif_Status
	
	CALL PM_Display_Wifi
	
	CALL PM_Display_USB

%if POST_DisplayPCRAMSize=1
; *** Print PC Memory Size Informations
	printstr_w STR_MEM
	MOV AX,CS:[PC_MEM]
    MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Dec
    printstr_w STR_KB	
    printstr_w STR_CRLF	
%endif	

	RET

; Display the PSRAM, SD, USB... Status
PM_Display_Perif_Status:
	printstr_w STR_PSRAM
	MOV BX,BV_PSRAMInit
	CALL DisplayInitStatus

; *** PicoMEM Wait then end of SD Init and display the Status
	printstr_w STR_SD
	MOV BX,BV_SDInit
	CALL DisplayInitStatus
	
; *** PicoMEM Wait then end of SD Init and display the Status
	printstr_w STR_USB
	MOV BX,BV_USBInit
	CALL DisplayInitStatus	

; *** PicoMEM Wait then end of Config file Loading and display the Status
    MOV byte CS:[BV_CFGInit],0    ; Debug FFh
	printstr_w STR_CONFIG
	MOV BX,BV_CFGInit
	CALL DisplayInitStatus
    printstr_w STR_CRLF
	RET

; **** Display the @, Port and Boot Count

PM_Display_Config:

	printstr_w STR_SEGMENT
	MOV AX,CS
    MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Hex

	printstr_w STR_IOPORT
	MOV AX,PM_BasePort	; Display the Base Port
    MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Hex3	

    printstr_w STR_IRQNB
    MOV BL,7    ; Light Grey
	XOR AX,AX
	MOV AL,[BV_IRQ]
	CALL BIOS_PrintAX_Dec

	printstr_w STR_BOOTCOUNT
	XOR AH,AH
	MOV AL,[BV_BootCount]
	CALL BIOS_PrintAX_Dec
    printstr_w STR_CRLF	

	RET	;PM_Display_Config
	
PM_Display_Wifi:
	
	CMP byte [BV_WifiInit],0FFh		; Disabled
	JNE No_Wifi_Continue_Display1
	RET
	
No_Wifi_Continue_Display1:

; Read the Wifi Status object, not simple string display
    CALL PM_GetWifiInfos

	printstr_w STR_Wifi

	CMP byte [BV_WifiInit],0FCh		; Skipped
	JE Wifi_Display_OnlyStatus
	
	CMP byte [BV_WifiInit],0FEh		; Disabled ?
	JNE No_Wifi_Continue_Display2

    printstr_w STR_DISABLED
	JMP Wifi_Display_End
	
No_Wifi_Continue_Display2:
    CMP byte [BV_WifiInit],0
    JE Wifi_Display_Ok
	
Wifi_Display_Ok:

    CMP byte [PMCFG_WifiIRQ],0      ; To correct ??
	JNE Wifi_IRQ_IsSet
    MOV byte [PMCFG_WifiIRQ],3
	
Wifi_IRQ_IsSet:

    printstr_w PCCR_Param + 6		; Print SSID

Wifi_Display_OnlyStatus:	
	printstr_w PCCR_Param + 102		; Print Wifi connection status string

    printstr_w STR_CRLF

    JMP Wifi_Display_End

    printstr_w STR_IRQNB
	MOV BL,7
	XOR AX,AX
	MOV AL,[PMCFG_WifiIRQ]
	CALL BIOS_PrintAX_Dec	

    printstr_w STR_IOPORT
	MOV BL,7
	MOV AX,[PMCFG_ne2000Port]
	CALL BIOS_PrintAX_Hex3

    printstr_w STR_SSID

Wifi_Display_End:

	RET	;PM_Display_Wifi

PM_Display_USB:

; Get the USB connection status (String List)
    MOV AH,CMD_USB_Status
	CALL PM_SendCMD         ; Get the Wifi Status Infos
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end
	
%if DOS_COM=1
    MOV SI,TEST_DL
%else
	MOV SI,PCCR_Param
%endif	
	CALL Display_String_List

	RET ;PM_Display_USB

; Display a list of Null terminated string, with the Nb of line in the first byte.
Display_String_List:
    XOR CX,CX
	LODSB     ; Load the Nb of string to display
	CMP AL,0
	JE Display_String_List_End	; Display nothing if no line

	MOV CL,AL
Display_String_List_Loop:
	PUSH CX
    MOV BL,7
	CALL BIOS_Printstrc
	PUSH SI
    printstr_w STR_CRLF    ; Go to the next line
	POP SI
	POP CX
	LOOP Display_String_List_Loop
Display_String_List_End:
	RET


; * PM_FatalError *
PM_FatalError:
    PUSH CS
    POP DS

    PUSH AX ; Save the Error Text
    printstr_r  STR_FATAL
	POP AX

	MOV SI,AX
	CALL BIOS_Printstrc

	printstr_w STR_PressAny
    printstr_w STR_CRLF

	CALL WaitKey_Loop

	RET

; * DISPLAY_StatusReg *
DISPLAY_StatusReg:
	printstr_w STR_STATUS	
DISPLAY_StatusReg2:	
	CALL PM_ReadStatus
    MOV BL,7    ; Light Grey
	CALL BIOS_PrintAL_Hex
    printstr_w STR_CRLF 
    RET

; * DisplayInitStatus *	
; 0FFh : Disabled, 0FEh if init in progress, 0FDh Failure, 0FEh Skipped
DisplayInitStatus:
	MOV AL,[BX]	
	CMP AL,0FFh
	JE Init_Disabled
	CMP AL,0FDh
	JE Init_Failure
	CMP AL,0FCh
	JE Init_Skipped
	CMP AL,0FEh
	JNE Init_Ok
	
    PUSH BX  ; Little Delay
	POP BX
	
    JMP DisplayInitStatus 	; Loop, still in progress
	
Init_Skipped:
    MOV SI,STR_SKIP
    MOV BL,4          ; red -- skipped = not OK
    JMP Init_DisplayRES
Init_Failure:
    MOV SI,STR_FAILURE
    MOV BL,4          ; red
    JMP Init_DisplayRES
Init_Disabled:
    MOV SI,STR_DISABLED
    MOV BL,7          ; light grey -- intentional, neutral
    JMP Init_DisplayRES
Init_Ok:
    MOV SI,STR_OK
    MOV BL,7          ; light grey -- intentional, neutral	

Init_DisplayRES:	
	CALL BIOS_Printstr
    RET

PM_Display_Y:
    ;	(AH)=3	READ CURSOR POSITION					:
;		(BH) = PAGE NUMBER (MUST BE 0 FOR GRAPHICS MODES)	:
;		ON EXIT (DH,DL) = ROW,COLUMN OF CURRENT CURSOR		:
;			(CH,CL) = CURSOR MODE CURRENTLY SET		:
    MOV AH,3
	INT 10h
	MOV AL,DH
	CALL BIOS_PrintAL_Hex

    RET


; * DISPLAY_Status *	
; Display the status that is set in AL

%assign STAT_READY         00h;
%assign STAT_CMDINPROGRESS 01h;
%assign STAT_CMDERROR      02h;
%assign STAT_CMDNOTFOUND   03h;
%assign STAT_INIT          04h;
%assign STAT_LAST          04h;

STR_RDY  DB "Ready",0
STR_BSY  DB "Busy",0
STR_ERR  DB "Error",0
STR_CNF  DB "Unknown CMD",0
STR_INIT DB "Init",0
STR_UNK  DB "Unkn Stat:",0

STR_CSIP DB " CS:IP ",0

DISPLAY_Status:
    PUSH AX  ; Save the Status value
    PUSH CS
    POP DS   ; To be sure that DS is Correct
    MOV SI,STR_UNK
	
    CMP AL,STAT_READY
	JNE Check_2
	MOV SI,STR_RDY
Check_2:
    CMP AL,STAT_CMDINPROGRESS
	JNE Check_3	
	MOV SI,STR_BSY
Check_3:	
    CMP AL,STAT_CMDERROR
	JNE Check_4
	MOV SI,STR_ERR
Check_4:
    CMP AL,STAT_CMDNOTFOUND
	JNE Check_5
	MOV SI,STR_CNF
Check_5:
    CMP AL,STAT_INIT
	JNE Check_6
	MOV SI,STR_INIT
Check_6:

	CMP SI,STR_UNK
	JNE DoDisplayStatus
	PUSH AX
	CALL BIOS_Printstr    ; Display Unknown Status
	POP AX
	MOV BL,4  ; Red
	CALL BIOS_PrintAL_Hex ; display the Status number

    CALL Do_Wait
    CALL DISPLAY_StatusReg2

; DEBUG Wait for a key if Unknown Status
	CALL WaitKey_Loop

%if TINY_BIOS=0	
	JMP DoDisplay_CSIP
%endif	
DoDisplayStatus:
	CALL BIOS_Printstr ; Display the Status Value (Text)

    POP AX   ; DEBUG Don't display CS:IP anymore if not Unknown Status
	RET

%if TINY_BIOS=0
DoDisplay_CSIP:	
; Display CS:IP (To find the error location)
	printstr_w STR_CSIP
	POP DX              ; Unstack the AX Value
	POP BX              ; UnStack "DISPLAY_Status" call IP
	PUSH BX
	PUSH DX
	PUSH BX
    PUSH CS
	POP AX
    MOV BL,7           ; Light Grey
	CALL BIOS_PrintAX_Hex
	printstr_w STR_DDOTNS
	POP AX
	SUB AX,2
	CALL BIOS_PrintAX_Hex

    POP AX
    RET	

Do_Wait:
    PUSH CX
	MOV CX,1000
WaitLoop1000:
	LOOP WaitLoop1000
	POP CX
	RET
%endif

;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! DEBUG Code disabled !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

STR_AXDX DB 'AX-DX :',0
STR_ESDI DB 'ES DI :',0

DisplayRegs:

	PUSH DX
	PUSH CX
	PUSH BX
	PUSH AX
    printstr_w STR_AXDX
	MOV BL,7
	POP AX
	CALL BIOS_PrintAX_Hex
	POP AX
	CALL BIOS_PrintAX_Hex
	POP AX
	CALL BIOS_PrintAX_Hex
	POP AX
	CALL BIOS_PrintAX_Hex

    printstr_w STR_ESDI
    MOV AX,ES
	CALL BIOS_PrintAX_Hex
	MOV AX,DI
	CALL BIOS_PrintAX_Hex
	
    printstr_w STR_CRLF	
	RET


%if ADD_Debug=1

ADVANCE_SPINNER:
	PUSH AX
	MOV AX, 0x0E08
	INT 0x10;
	INC DI
	CMP DI, 4
	JB .SPINNER_ADVANCED
	XOR DI, DI
.SPINNER_ADVANCED:

	MOV AL, [DI + .SPINNER_CHARACTERS]
	INT 0x10
	POP AX
	RET

.SPINNER_CHARACTERS:
	db '-\|/'

STR_AXDX DB 'AX-DX :',0
STR_ESDI DB 'ES DI :',0

DisplayRegs:

	PUSH DX
	PUSH CX
	PUSH BX
	PUSH AX
    printstr_w STR_AXDX
	MOV BL,7
	POP AX
	CALL BIOS_PrintAX_Hex
	POP AX
	CALL BIOS_PrintAX_Hex
	POP AX
	CALL BIOS_PrintAX_Hex
	POP AX
	CALL BIOS_PrintAX_Hex

    printstr_w STR_ESDI
    MOV AX,ES
	CALL BIOS_PrintAX_Hex
	MOV AX,DI
	CALL BIOS_PrintAX_Hex
	
    printstr_w STR_CRLF	
	RET

IRQ_DisplayRegs:
    PUSH AX
	PUSH BX
	PUSH CX
	PUSH DX
	PUSH SI
	PUSH DI
	PUSH DS
	PUSH ES

    PUSH CS
	POP DS
	CALL PM_SaveRegs
    MOV SI,REG_AX
    MOV AX,1
    CALL DISPLAY_MEM_Bytes	

	POP ES
	POP DS
	POP DI
	POP SI
	POP DX
	POP CX
	POP BX
	POP AX
	RET

IRQ_DisplayData :
    PUSH AX
	PUSH BX
	PUSH CX
	PUSH DX
	PUSH SI
	PUSH DI
	PUSH DS
	PUSH ES

    PUSH ES   ;Display ES:BX
	POP DS
; 	 MOV BX,7DE0h
;    ADD BX,180h
	MOV SI,BX
   	MOV AL,2         ; Display the result ! (1000:0000)
    CALL DISPLAY_MEM_Bytes

    MOV SI,REG_AX
    MOV AX,1
    CALL DISPLAY_MEM_Bytes

	POP ES
	POP DS
	POP DI
	POP SI
	POP DX
	POP CX
	POP BX
	POP AX
	RET   ;IRQ_DisplayData	
	
	
STR_DSD DB "S D: ",0

DISPLAY_S_D:
    PUSH AX
	PUSH BX
	PUSH DS
	PUSH SI
	PUSH ES
	PUSH DI
	
	PUSH DI
	PUSH ES
	PUSH SI
	
	PUSH CS
	POP DS
	MOV BL,10
	printstr_w STR_DSD
	MOV AX,DS
	CALL BIOS_PrintAX_Hex
	printstr_w STR_DDOTNS	
	POP AX
	CALL BIOS_PrintAX_Hex
	printstr_w STR_SPACE	
	POP AX
	CALL BIOS_PrintAX_Hex
	printstr_w STR_DDOTNS		
	POP AX
	CALL BIOS_PrintAX_Hex	
	
	POP DI
	POP ES
	POP SI
	POP DS
	POP BX
	POP AX
RET		;DISPLAY_S_D

; * DISPLAY_MEM_Bytes *	
;DS:SI : Data to DISPLAY    !! DS can be different than CS
;AL    : Number of Lines (32 Bytes)
DISPLAY_MEM_Bytes:
;    PUSH CS
;    POP DS   ; To be sure that DS is Correct

    XOR CX,CX
    MOV byte CS:[PM_I],AL
DISPLAY_MEM_NextLine:
	MOV AX,SI
	PUSH SI
    MOV BL,7    ; Light Grey
	PUSH DS
	PUSH CS
	POP DS
	CALL BIOS_PrintAX_Hex
	printstr_w STR_DDOTNS
	POP DS
	POP SI
	MOV byte CS:[PM_J],16
DISPLAY_MEM_NextByte:
	LODSB
	PUSH SI
	PUSH DS
	PUSH CS
	POP DS	
	CALL  BIOS_PrintAL_Hex
	printstr_w STR_SPACE
	POP DS
	POP SI
	DEC byte CS:[PM_J]
	JNE DISPLAY_MEM_NextByte
    
	PUSH SI
	PUSH DS
	PUSH CS
	POP DS
	printstr_w STR_DDOTNS
	POP DS
	POP SI

; Loop to display printable Char
    PUSH CS
    POP ES
    MOV AX,PMB_Stack
	MOV DI,AX     ; ES:DI BIOS RAM Internal Table
	MOV AX,SI
	SUB AX,16
	MOV SI,AX
	MOV byte CS:[PM_J],16
DISPLAY_MEM_NextChar:
	LODSB
	CMP AL,32
	JB Not_printable
    CMP AL,128
	JA Not_printable
	JMP printable
Not_printable:
    MOV AL,'.'
printable:
    STOSB  ; Store in the output buffer
	DEC byte CS:[PM_J]
	JNE DISPLAY_MEM_NextChar
	XOR AL,AL
	STOSB  ; Finish with 0

	PUSH DS
	PUSH CS
	POP DS	
	PUSH SI
    printstr_w PMB_Stack  
	printstr_w STR_CRLF
	POP SI	
	POP DS
	
	DEC byte CS:[PM_I]
	JNE DISPLAY_MEM_NextLine
	
	PUSH CS
	POP DS
    RET	
	
%endif  ;ADD_Debug

%include "pm_vars.asm"