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

; IRQ 2B : DOS "Mirror"

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

[BITS 16]
CPU 186

%assign DOS_COM FTYPE  			  ; Come from the compile command line

%define FORCE_INT19h          1
%define POST_DisplayPCRAMSize 0

; "Debug" mode options
%define Display_Intermediate  0   ; Display more messages (Debug)
%define DISPLAY_MAXCONV	      0
%define ENABLE_IRQ 	          1
%define DISPLAY_IRQNb		  0
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

STR_PICOMEM   DB 'PicoMEM',0
STR_Init      DB ' BIOS (Date ',__DATE__,')',0
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

;STR_EmulStart DB 'PicoMEM Emulation : ',0

STR_PMNOTDETECTED DB 'IO Port Error',0x0D,0x0A,0
STR_PMROMRAMERR   DB 'BIOS Memory Error',0x0D,0x0A,0
STR_PMRAMCMDERR   DB 'Memory Error: PC Cache enabled or Command error',0x0D,0x0A,0
;STR_PMDISKBERR    DB 'DISK Buffer Error',0x0D,0x0A,0
;STR_OLDBIOS       DB ' ',254,' Old BIOS > Start the setup later',0x0D,0x0A,0

STR_PressAny DB 'Press Any Key',0
STR_PressS   DB '> Press S for Setup, Other to continue ',0

STR_HDD  DB ' ',254,' HDD',0
STR_FDD  DB ' ',254,' FDD',0

; BV_IRQSource Values defines (Interrupt Source/Command)

%define IRQ_None        0	; IRQ was fired, but not intentionally or nothing to do
%define IRQ_RedirectHW  1	; Directly call another HW interrupt (ne2000, other)
%define IRQ_RedirectSW  2	; Directly call a Software Interrupt
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
%define IRQ_DMA         13

%define IRQ_NE2000      20

%define IRQ_Total       14

IRQ_Table DW IRQCMD_None           ;0
          DW IRQCMD_RedirectHW     ;1
          DW IRQCMD_RedirectSW	   ;2
          DW IRQCMD_StartPCCMD	   ;3
		  DW IRQCMD_USBMouse	   ;4
		  DW IRQCMD_Keyboard	   ;5
		  DW IRQCMD_IRQ3           ;6
		  DW IRQCMD_IRQ4           ;7
		  DW IRQCMD_IRQ5           ;8
		  DW IRQCMD_IRQ6           ;9
		  DW IRQCMD_IRQ7		   ;10
		  DW IRQCMD_IRQ9		   ;11
		  DW IRQCMD_IRQ10		   ;12
		  DW IRQCMD_DMA			   ;13
		  DW IRQCMD_None

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

%macro BIOSPOST 1
	PUSH AX
	MOV AL,%1
	OUT 80h,AL
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
%include "bios_menu.asm"

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

;   MOV BX,0102h
;	PUSH BX
;	PUSH BP
;	MOV BP,SP
;	MOV byte SS:[BP+2],0AAh
;	POP BP
;	POP BX
 
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

	BIOSPOST 0xFF

%if DOS_COM=1
; Code test Zone, in COM Version only

;jmp Skip_test

;jmp WordInputTest
;jmp StrInputTest
;CALL BM_ChangeFDD0Image
;JMP BIOS_Exit

; DMA copy test code

        PUSH CS
		POP DX
		MOV AX,DX
		MOV CL,12
		SHR AX,CL
		MOV CL,4
		SHL DX,CL
		MOV BX,PM_DISKB
		ADD BX,DX
		ADC AX,0
;		AX:BX Page:Offset

		MOV BYTE CS:[BV_IRQParam],1		; DMA Nb
 		MOV BYTE CS:[BV_IRQParam+1],AL	; Save the DMA page (from emulated DMA registers)
		MOV BYTE CS:[BV_IRQParam+2],1   ; 1 if it is the first Address of the transfer (Sent by the Pico)
		MOV BYTE CS:[BV_IRQParam+3],8	; Read the Data copy size (Must be <256)
		

		XOR AX,AX
		OUT 0Ch,AL          ; clear DMA flipflop (To read the values in the correct order)
; Update the DMA offset
		MOV AX,BX
        OUT 02h,AL	; LSB
		XCHG AH,AL
		OUT 02h,AL	; MSB

; Setup the length
		MOV AX,16 ; Test length (DMA controller)
		DEC AX
		OUT 03h,AL
		XCHG AH,AL
		OUT 03h,AL

		CALL IRQCMD_DMA
		CALL IRQCMD_DMA

        jmp Skip_test

    MOV byte [BV_IRQ],3
%if ENABLE_IRQ=1
    CALL PM_InstallIRQ3_5_7
%endif

    MOV AX,8+5
	MOV DX,PM_TestInt05h
	MOV BX,OldInt5h
	CALL BIOS_HookIRQ
	
    MOV AX,8+7
	MOV DX,PM_TestInt07h
	MOV BX,OldInt7h
	CALL BIOS_HookIRQ

    call WaitKey_Loop
	mov byte CS:[BV_IRQSource],IRQ_None
	int 3+8

    call WaitKey_Loop
	mov byte CS:[BV_IRQSource],IRQ_RedirectHW
	mov byte CS:[BV_IRQArg],5	
	int 3+8	
	
    call WaitKey_Loop
	mov byte CS:[BV_IRQSource],IRQ_RedirectSW
	mov byte CS:[BV_IRQArg],7+8	
	int 3+8	

    call WaitKey_Loop
	mov byte CS:[BV_IRQSource],IRQ_StartPCCMD
	
	mov byte CS:[Int_SelfMod+1],3+8
Int_SelfMod:
	int 0

    call WaitKey_Loop
	mov byte CS:[BV_IRQSource],IRQ_USBMouse
	
	MOV BL,3+8
	CALL Call_Int	
	
	JMP BIOS_Exit

Skip_test:
%endif  ; DOS_COM=1

; *****  PicoMEM Board initial test sequence *****
	BIOSPOST 0x00
	
	printstr_w STR_PICOMEM
	printstr_w STR_Init

; *** PicoMEM Test : Port Test 
; I/O Port test and Display (Currently test one Hardcoded port)
    MOV DX,PM_BasePort	; Test the Base Port
	CALL PM_TestReadPort
%if DOS_COM=1
    MOV AL,1
%endif	 ; DOS_COM=1
	CMP AL,1
	JE .PM_Detected
	BIOSPOST 0x82
	MOV AX,STR_PMNOTDETECTED
	CALL PM_FatalError	; Fatal Error
	JMP BIOS_Exit
.PM_Detected:

	BIOSPOST 0x01
	
; *** PicoMEM Test : BIOS RAM Test (16x AA/55)

    MOV SI,VARS_OFFS	; Test Offset (Segment DS)
	MOV CX,8			; Test 8KB
	CALL TestRAMSlow
	JNC ROMRAM_Ok

;   MOV CX,16
; 	MOV SI,BV_MEMTst
;ROMRAM_TestLoop:
; 	MOV AL,0AAh
;	MOV byte [SI],AL
;	CMP AL,byte [SI]
;	JNE ROMRAM_Error
;	MOV AL,055h
;	MOV byte [SI],AL
;	CMP AL,byte [SI]
;	JNE ROMRAM_Error
;	LOOP ROMRAM_TestLoop
;   JMP ROMRAM_Ok

ROMRAM_Error:
	BIOSPOST 0x80
    MOV AX,STR_PMROMRAMERR
	CALL PM_FatalError	; Fatal Error
	JMP BIOS_Exit	
ROMRAM_Ok:

; Now we can start using the RAM
	BIOSPOST 0x02

	printstr_w STR_Init2
	
; ** As the BIOS RAM is Ok, increment the Init Counter
	INC byte [BV_InitCount]

; *** POST: Initialize the BIOS Vars (Default values)
    MOV byte [PM_NeedReboot],0
	MOV byte [Int19h_SkipSetup],0		; Ask to not start in the Boot Strap Int19h
	MOV byte [ListLoaded],0	
	MOV byte [Int13h_Flag],0
	MOV byte [BOOT_FDDFirst],0
	
	MOV byte [PMCFG_SD_Speed],24
	MOV byte [PMCFG_RAM_Speed],100
	
; *** POST : Wait the end of the PicoMEM Initialisazion ***

; Read Command Status, then :
; 1) Check if in Wait COM Mode (Status Port)

	printchar_w '.'    ; Check if in Debug mode (Wait COM)
	
%if DOS_COM=1
	MOV AH,0
%else
    CALL PM_ReadStatus
%endif ; DOS_COM=1
    CMP AL,STAT_WAITCOM
	JNE POST_NoWaitCOM

	printstr_w STR_WAITCOM
    
POST_WaitCOMLoop:
    CALL PM_ReadStatus 
    CMP AL,STAT_WAITCOM
	JE POST_WaitCOMLoop

POST_NoWaitCOM:

	BIOSPOST 0x03

; 2) If in Init mode (Status Port), wait for the end

    CMP AL,STAT_INIT
	JNE POST_InitCompleted
	
POST_WaitInitLoop:
    CALL PM_ReadStatus 
    CMP AL,STAT_INIT
	JE POST_WaitInitLoop	; If the Init never finish, the PC is crashed !!

POST_InitCompleted:

%if DOS_COM=1
	JMP StatusAtBootOk
%endif ; DOS_COM=1

; 3) If not in Ready Status, Initialize

	BIOSPOST 0x10
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

; Now we can use RAM and commands, verify if the command to write to RAM is Ok
; Write from port, Read from memory and compare : Detect if PC Cache is active

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
	
RAM_CMD_TestError:

	BIOSPOST 0x80
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
	
;	MOV SI,PMCFG_PM_MMAP
;	MOV CX,4
;	MOV AL,MEM_PSRAM
;	REP STOSB
	
    JMP PM_Reboot

POST_TandyRAMTest2:
    CMP byte CS:[BV_InitCount],2
	JA POST_NoTandyRAM 

; 2nd Tandy BOOT : Check if emulation worked and update the RAM MAP

POST_NoTandyRAM:

; 4) Detect IRQ

	BIOSPOST 0x11
	printchar_w '.'    ; Detect IRQ

%if ENABLE_IRQ=1
    CALL PM_DetectIRQ	
%endif	

	BIOSPOST 0x12

	CMP byte CS:[PMCFG_PREBOOT],1
	JNE PM_NoPreBootSetup

	MOV byte CS:[Int19h_SkipSetup],1		; Ask to not start in the Boot Strap Int19h

	printstr_w STR_CRLF

	CALL PM_Do_Setup

PM_NoPreBootSetup:

%if DOS_COM=1
	JMP BIOS_Exit
%endif ; DOS_COM=1

	BIOSPOST 0x13
	
	CMP byte CS:[PMCFG_PREBOOT],1
	JE PM_Skipdot
	printchar_w '.'   		    ; Install IRQ19h
PM_Skipdot:

    CALL PM_InstallIRQ19		; Install the Boot Strap IRQ

	BIOSPOST 0x20

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

; Do the PicoMEM Initialisation (At the end of the BIOS Init Code)
PM_FinalConfig:

; ** Save the Config File **
	
    MOV AH,CMD_SaveCfg
	CALL PM_SendCMD           ; Do CMD_SaveCfg Command
	CALL PM_WaitCMDEnd        ; Wait for the command End
	JNC SaveConfigOk
    printstr_w STR_CFGErr
	CALL PM_Reset			  ; Need to Reset after an error
SaveConfigOk:	

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

    CMP byte CS:[BV_Tandy],1
	JE NotUpdateBIOSRAMSize
	
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

    MOV AH,CMD_DEV_Init
	CALL PM_SendCMD           ; Do CMD_DEV_Init Command
	CALL PM_WaitCMDEnd        ; Wait for the command End

; Add Joystick in the BIOS Variable if added by the PicoMEM
    CMP BYTE CS:[PMCFG_EnableJOY],1
	JNE Not_Add_Joystick
	
    MOV AX,040h
	MOV ES,AX
    OR BYTE ES:[BIOSVAR_EQUIPH],10h		; Needed for some games (Alley Cat)
Not_Add_Joystick:

%if ENABLE_IRQ=1
    CALL PM_InstallIRQ3_5_7
%endif	
	RET ; PM_FinalConfig End

; END of the PicoMEM Init
   

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

;	CMP byte [PC_DiskNB],0    ; If there is already a disk, do nothing (For the moment)
;	JE DiskMount_End
	
;    printstr_w STR_DiskPresent
;    printstr_w STR_CRLF

;    XOR CX,CX
;	MOV CL,byte [PC_DiskNB]
;	PUSH CS 
;	POP ES
;	MOV DI,HDD0_Attribute
;	XOR AL,AL
;	REP STOSB		; Disable "PC_DiskNB" Emulated disk

DiskMount_End:

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

    MOV AX,040h	; ! Useless now
	MOV ES,AX
	MOV byte ES:[BIOSVAR_DISKNB],1	; Set the Nb of disk in the BIOS

DiskInit_NoHDD0:	

    CMP byte [HDD1_Attribute],0x80	; Emulated HDD1 ?
	JB DiskInit_NoHDD1

    MOV AX,46h						; Define the new DPT address
	MOV DX,PM_DPT_1
	MOV BX,OLD_DPT1
	CALL BIOS_HookIRQ

    MOV AX,040h	; ! Useless now
	MOV ES,AX
	INC byte ES:[BIOSVAR_DISKNB]

DiskInit_NoHDD1:

    CMP byte [HDD2_Attribute],0x80	; Emulated HDD2 ?
	JB DiskInit_NoHDD2
	INC byte ES:[BIOSVAR_DISKNB] 	; ! Useless now
DiskInit_NoHDD2:

    CMP byte [HDD3_Attribute],0x80	; Emulated HDD3 ?
	JB DiskInit_NoHDD3
	INC byte ES:[BIOSVAR_DISKNB] 	; ! Useless now
DiskInit_NoHDD3:

    MOV AL,CS:[New_DiskNB]
    MOV ES:[BIOSVAR_DISKNB],AL		; Set the Nb of disk as computer by the PicoMEM

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

PM_DetectIRQ:
; Hook Test Interrupts

    CMP	byte [BV_IRQ],0		; 06/07/24 : Skip IRQ Detection if already set in config	
	JE PM_DoDetectIRQ
	RET
PM_DoDetectIRQ:	

;    printstr_w STR_DetIRQ
    CLI

    MOV AX,8+3
	MOV DX,PM_TestInt03h
	MOV BX,OldInt3h
	CALL BIOS_HookIRQ

    MOV AX,8+5
	MOV DX,PM_TestInt05h
	MOV BX,OldInt5h
	CALL BIOS_HookIRQ
	
    MOV AX,8+7
	MOV DX,PM_TestInt07h
	MOV BX,OldInt7h
	CALL BIOS_HookIRQ

; Enable the Interrupts
	IN AL,21h
	MOV CS:[PM_I],AL	; Save the IRQ Mask register
	AND AL,01010111b    ; IRQ 3+5+7 Mask
	OUT 21h,AL

    STI

; Wait for the IRQ
	MOV byte [BV_IRQ],0

    MOV AH,CMD_SendIRQ
	CALL PM_SendCMD         ; Do CMD_SendIRQ Command
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end	

	MOV CX,0
WaitTestIRQ:
	CMP byte [BV_IRQ],0
	JNE IRQ_Detected
	LOOP WaitTestIRQ
	JMP Detect_IRQ_End

IRQ_Detected:	
;   printstr_w STR_Detected
;	XOR AH,AH
;	MOV AL,byte [BV_IRQ]
;	CALL BIOS_PrintAX_Dec
	
Detect_IRQ_End:

    CLI

    MOV AH,CMD_IRQAck
	CALL PM_SendCMD         ; Do CMD_FDD_Mount Command
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end	
	
; Restore the Interrupt Controller Mask	
   	MOV AL,CS:[PM_I]	; Get the Previous IRQ Mask
	OUT 21h,AL

; Restore IRQ Interrupts

    MOV AX,8+3
	MOV DX,OldInt3h
	CALL BIOS_RestoreIRQ

    MOV AX,8+5
	MOV DX,OldInt5h
	CALL BIOS_RestoreIRQ

    MOV AX,8+7	
	MOV DX,OldInt7h
	CALL BIOS_RestoreIRQ
	
	STI	
	RET

; Interrupt used to detect the HW IRQ number
; Return the Physical IRQ Number
PM_TestInt03h:  ; (serial port 2 interrupt)
    MOV byte CS:[BV_IRQ],3
	JMP IRQ_Test_End
PM_TestInt05h:  ; (XT HDD port interrupt)
    MOV byte CS:[BV_IRQ],5
	JMP IRQ_Test_End	
PM_TestInt07h:  ; (parallel printer interrupt)
    MOV byte CS:[BV_IRQ],7
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

; The Interrupt code that will be copied in RAM for Auto modified code
Call_Int_ROM:
	mov byte CS:[Call_Int+Call_Int_SelfMod-Call_Int_ROM+1],BL
Call_Int_SelfMod:
	int 0
	RET
Call_Int_ROM_End:

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

; Do IRQ 7 Install
	CMP ES:[(8+7)*4+2],AX	; IRQ 7 Redirected to PicoMEM BIOS ?
	JNE Do_IRQ7_Install
	; IRQ 7 Already installed
	RET	
	
Do_IRQ7_Install:
    MOV AX,8+7
	MOV DX,PM_Int
	MOV BX,OldInt7h
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
	MOV BX,OldInt5h
	CALL BIOS_HookIRQ
    CLI	

	IN AL,21h
	AND AL,11011111b    ; IRQ 5 Mask
	
	JMP HWIRQ_InstallEnd
	
PM_InstallIRQ3:

	CMP ES:[(8+3)*4+2],AX	; IRQ 5 Redirected to PicoMEM BIOS ?
	JNE Do_IRQ3_Install
	; IRQ 3 Already installed
	RET
	
Do_IRQ3_Install:
    MOV AX,8+3
	MOV DX,PM_Int
	MOV BX,OldInt3h
	CALL BIOS_HookIRQ
    CLI

	IN AL,21h
	AND AL,11110111b    ; IRQ 3 Mask

HWIRQ_InstallEnd:
	OUT 21h,AL		; Enable the IRQ

; Move the Interrupt call code to the RAM
	MOV SI,Call_Int_ROM
    PUSH CS
	POP ES
	MOV DI,Call_Int
	MOV CX,Call_Int_ROM_End-Call_Int_ROM
	REP MOVSB

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
%define  IRQ_Stat_NONE			0	; Set by the Pico, no IRQ Requested
%define  IRQ_Stat_REQUESTED     1   ; Set by the Pico, IRQ Request in progress
%define  IRQ_Stat_INPROGRESS 	2   ; Set by the BIOS, IRQ In progress

%define  IRQ_Stat_COMPLETED		0x10 ; 16 Set by the BIOS, IRQ Completed Ok
%define  IRQ_Stat_WRONGSOURCE   0x11 ; 17 Set by the BIOS, IRQ Completed, Wrong Source
%define  IRQ_Stat_DISABLED      0x12 ; 18 Ths interrupt is disabled in the PIC
%define  IRQ_Stat_INVALIDARG    0x13 ; 19 Invalid Argument Error
%define  IRQ_Stat_SameIRQ       0x14 ; 20 Asked to call the same IRQ
%define  IRQ_Stat_ALREADY_INP   0x15 ; 21 IRQ was already in progress

IRQ_Mouse_InProgress DB 0

PM_Int:
;        CMP byte CS:[BV_IRQStatus],IRQ_Stat_INPROGRESS
;		JE 
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_INPROGRESS ; Change State to Command in progress
		PUSH BX

%if DISPLAY_IRQNb=1
		push_all_exceptbx
		MOV AL,'i'
		MOV BL,R_BK
		CALL BIOS_Printchar
		MOV BL,7
		MOV AL,CS:[BV_IRQSource]
		CALL BIOS_PrintAL_Hex		
		pop_all_exceptbx	
%endif

		XOR BX,BX
		MOV BL,CS:[BV_IRQSource]    ; Read the next command
		CMP BL,IRQ_Total
		JAE PM_Int_WrongSource

		SHL BX,1
		JMP [CS:IRQ_Table+BX]     	; Jump to the command 		

PM_Int_WrongSource:
		POP BX
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_WRONGSOURCE
		
		PUSH AX
		MOV AL,20h
		OUT 20h,AL
		POP AX
		IRET
		
PM_Int_End_Ok:
       MOV BL,IRQ_Stat_COMPLETED
	   
PM_Int_End_Err:  		; Error must be placed in BL
		MOV AL,20h
		OUT 20h,AL

PM_Int_End_NoAck:    		
		pop_all_exceptbx
		MOV byte CS:[BV_IRQStatus],BL ; Change State to BL
		POP BX 		
		IRET

; This IRQ Does nothing
IRQCMD_None:
%if DISPLAY_IRQNb=1
        push_all_exceptbx
		MOV AL,'x'
		MOV BL,7
		CALL BIOS_Printchar
		pop_all_exceptbx
%endif
		
		PUSH AX
		MOV AL,20h
		OUT 20h,AL
		POP AX
		POP BX
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_COMPLETED ; Change State to Command Completed
		IRET

IRQCMD_RedirectHW:
        push_all_exceptbx  ; To remove for final

        MOV BL,CS:[BV_IRQArg]
		CMP BL,7
		JBE IRQCMD_RedirectHW_Ok
		CMP BL,15
		JA IRQCMD_RedirectHW_Err
	    ADD BL,70h-8	        ; 2nd Interrupt controller vector
		JMP IRQCMD_RedirectHWHigh_Ok
IRQCMD_RedirectHW_Err:		
	    MOV BL,IRQ_Stat_INVALIDARG
		JMP PM_Int_End_Err		; Return with Error		
IRQCMD_RedirectHW_Ok:		

; Read and Verify the PIC Mask register (Only the XT One for the moment)
	    XOR CX,CX
		MOV CL,BL
		MOV AH,1
		SHL AH,CL
		IN AL,21h
		AND AL,AH
		JNZ IRQCMD_RedirectHW_Ignore

; Call the Hardware IRQ (Value + 8)
IRQCMD_RedirectHWHigh_Ok:
		ADD BL,8				; HW IRQ is SW IRQ+8
		CALL Call_Int			; Call the BL SW interrupt

IRQCMD_RedirectHW_Ignore:
		
		JMP PM_Int_End_Ok

IRQCMD_RedirectSW:
        push_all_exceptbx
%if DISPLAY_IRQNb=1		
		MOV AL,'s'
		MOV BL,7
		CALL BIOS_Printchar
		MOV AL,CS:[BV_IRQArg]
		CALL BIOS_PrintAL_Hex
%endif

		MOV BL,CS:[BV_IRQArg]
		CALL Call_Int			; Call the BL SW interrupt !!! Not Working
		
		JMP PM_Int_End_Ok

IRQCMD_StartPCCMD:
        push_all_exceptbx
		
		MOV AL,'c'
		MOV BL,7
		CALL BIOS_Printchar			

		JMP PM_Int_End_Ok		

; Mouse IRQ Event > Send the Data to the Mouse Interrupt Driver
IRQCMD_USBMouse:
        push_all_exceptbx

%if DISPLAY_IRQNb=1		
		MOV AL,'m'
		MOV BL,7
		CALL BIOS_Printchar		
%endif		
        CLI
	
		MOV AL,CS:[BV_IRQParam]
		XOR DH,DH
		MOV DL,AL		; Buttons in DL
		MOV AL,CS:[BV_IRQParam+1]
		CBW
		MOV BX,AX		; Delta x in BX
		MOV AL,CS:[BV_IRQParam+2]
		CBW
		MOV CX,AX		; Delta Y in CX

		MOV AX,0060h	; PicoMEM custom IRQ : Update Mouse
		int 33h			; call Mouse IRQ		> Crash during DOS Boot

		JMP PM_Int_End_Ok	; End and answer Ok

IRQCMD_Keyboard:
        push_all_exceptbx
		MOV AL,'k'
		MOV BL,7
		CALL BIOS_Printchar			

		JMP PM_Int_End_Ok

IRQCMD_IRQ3:
		PUSH AX
		IN AL,21h
		AND AL,00001000b
		JNZ SkipInt
		CMP byte CS:[BV_IRQ],3	; Compare with the Hardware IRQ number
		JE SameInt

		Int 3+8
		
		MOV AL,20h
		OUT 20h,AL
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_COMPLETED ; Change State to Command Completed		
		POP AX
		POP BX
		IRET

IRQCMD_IRQ4:
		PUSH AX
		IN AL,21h
		AND AL,00010000b
		JNZ SkipInt

		Int 4+8

		MOV AL,20h
		OUT 20h,AL
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_COMPLETED ; Change State to Command Completed		
		POP AX
		POP BX
		IRET
		
SkipInt: ; Skip > Return Disabled IRQ code
		MOV AL,20h
		OUT 20h,AL
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_DISABLED  ; The Interrupt was not executed (PIC Mask)
		POP AX
		POP BX		
		IRET

SameInt:
		MOV AL,20h
		OUT 20h,AL
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_SameIRQ  ; The Interrupt was not executed (PIC Mask)
		POP AX
		POP BX
		IRET
		
IRQCMD_IRQ5:
		PUSH AX
		IN AL,21h
		AND AL,00100000b
		JNZ SkipInt
		CMP byte CS:[BV_IRQ],5	; Compare with the Hardware IRQ number
		JE SameInt

		Int 5+8

		MOV AL,20h
		OUT 20h,AL
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_COMPLETED ; Change State to Command Completed
		POP AX
		POP BX
		IRET

IRQCMD_IRQ6:
		PUSH AX
		IN AL,21h
		AND AL,01000000b
		JNZ SkipInt

		Int 6+8

		MOV AL,20h
		OUT 20h,AL
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_COMPLETED ; Change State to Command Completed		
		POP AX
		POP BX		
		IRET

IRQCMD_IRQ7:
		PUSH AX
		IN AL,21h
		AND AL,10000000b
		JNZ SkipInt
		CMP byte CS:[BV_IRQ],7	; Compare with the Hardware IRQ number
		JE SameInt		

		Int 7+8

		MOV AL,20h
		OUT 20h,AL
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_COMPLETED ; Change State to Command Completed		
		POP AX
		POP BX		
		IRET

IRQCMD_IRQ9:
IRQCMD_IRQ10:

       JMP IRQCMD_RedirectHW_Err

;--------------------------------------------------------------
; - Emulated DMA Code

IRQCMD_DMA:
		MOV Byte CS:[BV_DMAStatus],2
	
;		POP SI
;		POP DS
;		POP CX
;		POP AX

;		POP BX
;		MOV byte CS:[BV_IRQStatus],IRQ_Stat_COMPLETED ; Change State to Command Completed		
;		MOV Byte CS:[BV_DMAStatus],3
;		IRET
		
        PUSH AX
		PUSH CX
		PUSH DS
		PUSH SI

; Get the DMA number
		XOR AX,AX
        CLI
		OUT 0Ch,AL          ; clear DMA flipflop (To read the values in the correct order)
		CMP BYTE CS:[BV_IRQParam],1		 ; Read the DMA number
		JNE IRQ_CMD_READ_DMA3

; Read the DMA Values (Address and Transfer size)

; DMA 1
; Read the DMA page
 		MOV AL,BYTE CS:[BV_IRQParam+1]	; Read the DMA page (from emulated DMA registers)
		MOV CL,12
		SHL AX,CL
;       IN AL,83h
		MOV DS,AX			; DS is the current page number
; Read the DMA offset
        IN AL,02h	; LSB
		XCHG AH,AL
		IN AL,02h	; MSB
		XCHG AH,AL
		MOV SI,AX			; SI is the current DMA offset
; Read the length
		IN AL,03h
		XCHG AH,AL
		IN AL,03h
		XCHG AH,AL
		INC AX
		MOV DX,AX

		JMP IRQ_READ_DMA_END

IRQ_CMD_READ_DMA3:
; DMA 3
; Read the DMA page
 		MOV AL,BYTE CS:[BV_IRQParam+1]	; Read the DMA page (from emulated DMA registers)
		MOV CL,12
		SHL AX,CL
;		IN AL,82h
		MOV DS,AX			; DS is the current page number
		
; Read the DMA offset
        IN AL,06h	; LSB
		XCHG AH,AL
		IN AL,06h	; MSB
		XCHG AH,AL
		MOV SI,AX			; SI is the current DMA offset
; Read the length
		IN AL,07h
		XCHG AH,AL
		IN AL,07h
		XCHG AH,AL
		INC AX
		MOV DX,AX			; DX : DMA transfer size

IRQ_READ_DMA_END:
        STI

;DS:SI : DMA Buffer Address
;DX    : Remaining bytes to copy  (From DMA registers)

		CMP BYTE CS:[BV_IRQParam+2],1   ; 1 if it is the first Address of the transfer (Sent by the Pico)
		JNE IRQCMD_DMA_NOT_FIRST

		MOV AX,DS
		MOV CL,12
		SHR AX,CL		; Page is the last 4 Bit of the segment
		MOV CS:[DMA_PAGE],AL	; Save the Page
		MOV AX,SI
		MOV CS:[DMA_OFFSET],AX  ; Save the PC DMA Buffer Offset
		MOV CS:[DMA_SIZE],DX	; Save the DMA Buffer Size (DMA Controller side)

IRQCMD_DMA_NOT_FIRST:

%if DISPLAY_IRQNb=1
		push_all_exceptbx
		MOV AL,'d'
		MOV BL,R_BK
		CALL BIOS_Printchar
		MOV BL,7
		MOV AL,CS:[DMA_PAGE]	
		CALL BIOS_PrintAL_Hex
		MOV AX,CS:[DMA_OFFSET]
		CALL BIOS_PrintAX_Hex
		pop_all_exceptbx	
%endif
; Get the Nb of bytes to copy

		XOR BX,BX
		MOV BL,CS:[BV_IRQParam+3]	; Read the Data copy size (Must be <256)
; DS:SI to ES:DI, BX bytes to copy, DX bytes in DMA Controller

; Set the target 
		PUSH CS
		POP ES
		MOV AX,E_DMA_BUFFER
		MOV DI,AX

        CMP Byte CS:[BV_IRQArg],0
		JNE E_DMA_Autoinit

;Perform the Data copy

        MOV CX,BX	; Copy the Nb of bytes 
		CMP BX,DX
		JB E_DMA_LastCopy_Single
	    MOV CX,DX	; More to copy than available from the DMA controller > Adjust
E_DMA_LastCopy_Single:
        OR CX,CX
		JZ E_DMA_UPDATE_REGS
		SUB DX,CX
		DEC DX		; Update the remaining byted to copy value (DMA Register)
		
		CLD
		SHR CX,1
		REP MOVSW
		JNC E_DMA_UPDATE_REGS
		MOVSB

        JMP E_DMA_UPDATE_REGS
E_DMA_Autoinit:

E_DMA_DataCopyLoop:

;Update the DMA controller registers (Only for Auto Init)
;Now SI contains the current location Current remaining size in ??
E_DMA_UPDATE_REGS:

        CLI
		XOR AX,AX
		OUT 0Ch,AL          ; clear DMA flipflop (To read the values in the correct order)
		CMP BYTE CS:[BV_IRQParam],1		 ; Read the DMA number
		JNE IRQ_CMD_WRITE_DMA3

; DMA 1
; Don't update the  DMA page
; Update the DMA offset (SI)
        PUSH SI
		POP AX
        OUT 02h,AL	; LSB
		XCHG AH,AL
		OUT 02h,AL	; MSB
		XCHG AH,AL
; Update the length (DX)
		MOV AX,DX
		OUT 03h,AL
		XCHG AH,AL
		OUT 03h,AL

		JMP IRQ_WRITE_DMA_END

IRQ_CMD_WRITE_DMA3:
; DMA 3
; Don't update the  DMA page
; Update the DMA offset
        PUSH SI
		POP AX
        OUT 06h,AL	; LSB
		XCHG AH,AL
		OUT 06h,AL	; MSB
		XCHG AH,AL
; Update the length
		MOV AX,DX
		OUT 07h,AL
		XCHG AH,AL
		OUT 07h,AL

IRQ_WRITE_DMA_END:

		STI

; FOR TEST ! TO REMOVE
		POP SI
		POP DS
		POP CX
		POP AX
		MOV Byte CS:[BV_DMAStatus],3

		RET
		
		

		MOV AL,20h
		OUT 20h,AL
		MOV byte CS:[BV_IRQStatus],IRQ_Stat_COMPLETED ; Change State to Command Completed		
		
		POP SI
		POP DS
		POP CX
		POP AX

		POP BX
		MOV Byte CS:[BV_DMAStatus],3
		IRET
; Emulated DMA IRQ End


;***********************************************************************************************
;*                           IRQ 9h Redirect the Keyboard IRQ   (Hardware IRQ 1)               *
;***********************************************************************************************
; PicoMEM IRQ9 is installed via a BIOS Command

CtrlI  Equ 1709h
CtrlP  Equ 1910h
CtrlF1 Equ 5E00h
CtrlF2 Equ 5F00h
CtrlF3 Equ 6000h
CtrlF4 Equ 6100h
CtrlF5 Equ 6200h

STR_Infos DB 'PicoMEM Infos > ',0
STR_INT13Act DB 'Int13h Active',0

;	KB_BUF_HD (1Ah) = "head" next character stored in keyboard buffer
;	KB_BUF_TL (1Ch) = "tail" next spot available in keyboard buffer

PM_Int09h:
		
		PUSHF
		CALL FAR [CS:OldInt9h]	; Call the BIOS Int9h (Read the Key)

		PUSH AX
		PUSH BX
		PUSH DX
		PUSH DS

        MOV AX,0040h
        MOV DS,AX               ; DS=BIOS Variables
        MOV DX,Word DS:[001Ch]	; Read Tail
        MOV BX,Word DS:[001Ah]  ; Read Head
        CMP DX,BX
        JZ  Int9_end            ; Empty buffer > End

		MOV AL,DS:[17h]         ; Test if Left Shift pressed
		AND AL,00000010b
		JZ Int9_end
		
        MOV AX,word DS:[BX]             ; Read the Key Code

;		push_all
;		MOV BL,07
;		CALL BIOS_PrintAX_Hex
;        printchar_w '.'		
;		pop_all
		
        CMP AX,CtrlI
		JE  SCI_Pressed ; LeftShift+Control+I Pressed

        CMP AX,CtrlF1
		JE  SCI_Pressed ; LeftShift+Control+F1 Pressed

        CMP AX,CtrlF2
		JE  CSF2_Pressed ; LeftShift+Control+F2 Pressed

        JMP Int9_end

CSF2_Pressed:		; Swap Floppy A (0)
        MOV word DS:[001Ah],DX       ; Clean the Keyboard buffer
        PUSH SI
        PUSH ES
		PUSH DI
		PUSH CX

	    MOV AH,03h  ; GET Cursor position(DH DL)
	    INT 10h
		PUSH DX		; Save Cursor Position
		
        CALL BM_ChangeFDD0Image

		CALL BIOS_MountFDD
		
		POP DX		; Restore Cursor Position
		MOV AH,02	; Set Cursor position
		INT 10h

		JMP PM_Int09h_Skip

SCI_Pressed:
        MOV word DS:[001Ah],DX       ; Clean the Keyboard buffer
        PUSH SI
        PUSH ES
		PUSH DI
		PUSH CX

;		MOV BX,113
;	    CALL BIOS_PrintAX_Hex
        PUSH CS
		POP DS
		
        CMP byte CS:[Int13h_Flag],0
		JE I9_Int13_NotActive 
		printstr_w STR_INT13Act
		JMP PM_Int09h_Skip   ; Don't start if there is a disk access in progress.
I9_Int13_NotActive:
		
;        printstr_w STR_CRLF
;		printstr_w STR_Infos
        CALL PM_Display_FullHeader		
	    CALL Display_DisksList
		CALL DisplayMEMConfig

; Print the Status Register
;        CALL DISPLAY_StatusReg		

; Skip if Int 13h Active		
PM_Int09h_Skip:
		
        POP CX
		POP DI
		POP ES
		POP SI
Int9_end:

        POP DS
		POP DX
		POP BX
		POP AX
		
        IRET  ; Int09h End
		
;***********************************************************************************************
;*                            IRQ 13h PicoMEM BIOS Functions                                   *
;*  																	                       *
;***********************************************************************************************

PM_BIOS:
    PUSH DS   		; Preserve DS
	CMP AL,0
	JNE PMB_Check1
;PM BIOS Function 0 : Detect the PicoMEM BIOS and return config  > To use by PMEMM, PMMOUSE ...
; Input : AH=60h AL=00h
;         DX=1234h
; Return AX : Base Port
;        BX : BIOS Segment
;        CX : Available devices Bit Mask
;        DX : AA55h (Means Ok)
; Redirect the Picomem IRQ if not done
	PUSH DS
	PUSH ES
	PUSH SI
	PUSH DI
	CALL PM_InstallIRQ3_5_7	; Force the reinstall of the HW IRQ if not done
	POP DI
	POP SI
	POP ES
	POP DS

    XOR AX,AX
	CMP byte CS:[BV_PSRAMInit],0
	JNE PMB_NoPSRAM
	OR AL,00000001b
PMB_NoPSRAM:	
	CMP byte CS:[BV_SDInit],0
	JNE PMB_NoSD
	OR AL,00000010b
PMB_NoSD:
	CMP byte CS:[BV_USBInit],0
	JNE PMB_NoUSB
	OR AL,00000100b
PMB_NoUSB:
	CMP byte CS:[BV_WifiInit],0
	JNE PMB_NoWifi
	OR AL,00001000b
PMB_NoWifi:
	MOV AH,CS:[BV_IRQ]	; Return the IRQ number

	MOV CX,AX			; Return the available devices
	
	MOV AX,PM_BasePort	; Return the Base Port
	PUSH CS
	POP  BX				; Return the BIOS Segment

	MOV DX,0AA55h 		; Return the PicoMEM BIOS Signature

	JMP PMB_End
	
PMB_Check1:
	CMP AL,1
	JNE PMB_Check2
;PM BIOS Function 1 : Get RAM Config
; Return AH : PMRAM Enabled
;        AL : PSRAM Enable
;        BX :
;        CX : EMS Base Port (0 if Disabled) 
;        DX : EMS Address Segment

	MOV AH,CS:[PMCFG_PMRAM_Ext]
	MOV AL,CS:[PMCFG_PSRAM_Ext]
	XOR BX,BX
	MOV BL,CS:[PMCFG_EMS_Port]
	SHL BX,1
	MOV CX,CS:[EMS_Port_List+2+BX]
	MOV BL,CS:[PMCFG_EMS_Addr]
	SHL BX,1
	MOV DX,CS:[EMS_Addr_List+2+BX]
	MOV BX,0FFFFh

	JMP PMB_End
PMB_Check2:
	CMP AL,2
	JNE PMB_Check3
;PM BIOS Function 2 : Return the NE2000 / Wifi Connection Status
; Return AL : Connection Status
;        AH : IRQ
;        BX : Base Port (0 if not Present)
;        Connection status string returned in PCCR_Param
	CALL PM_GetWifiInfos

	MOV AH,byte CS:[PMCFG_WifiIRQ]
	MOV AL,byte CS:[BV_WifiInit]
    MOV BX,word CS:[PMCFG_ne2000Port]
	XOR CX,CX
	
	JMP PMB_End

PMB_Check3:
	CMP AL,3
	JNE PMB_Check10
;PM BIOS Function 3 : Return the PicoMEM Board ID and BIOS RAM Offset
; Added in Sept 2024
; Return AH : Pi Pico Board / chip ID
;        AL : PicoMEM Board ID
;        BX : Firmware Revision
;        CX : PCCR_Param (Offset of commands response)
;        DX : Reserved for the future

; Info : To detect if implemented, just check if BX is not changed after calling it.

    MOV AX,CS:[BV_BoardID]
	MOV BX,CS:[BV_FW_Rev]
	MOV CX,PCCR_Param
	MOV DX,0FFFFh
	JMP PMB_End	

PMB_Check10:
	CMP AL,10h
	JNE PMB_Check11

;PM BIOS Function 10h : Enable the Mouse
; Return AL : 0 Success or Error
	MOV AH,CMD_Mouse_Enable		; Ask to enable the Mouse
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end	
    
	PM_Read_DataL	; Read Data L to AL (Macro)
	
	JMP PMB_End

PMB_Check11:
	CMP AL,011h
	JNE PMB_Check12
;PM BIOS Function 11h : Disable the Mouse
; Return AL : 0 Success or Error

	MOV AH,CMD_Mouse_Disable	; Ask to disable the Mouse
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end	
    
	PM_Read_DataL	; Read Data L to AL (Macro)
	
	JMP PMB_End

PMB_Check12:
	CMP AL,012h
	JNE PMB_Check13
;PM BIOS Function 12h : Reinstall the Keyboard interrupt

	CALL PM_InstallIRQ9

	JMP PMB_End

PMB_Check13:

; Fonction is not implemented

PMB_End:
	DEC byte CS:[Int13h_Flag]
	POP DS
	IRET
;	RETF	2 			      ; return from int with current flags

;***********************************************************************************************
;*                            IRQ 13h DISK BIOS Emulation and PicoMEM IRQ                      *
;*  																	                       *
;*  https://stanislavs.org/helppc/int_13.html                                                  *
;***********************************************************************************************

;AH = Command 02h Read 03h Write 05h format ....         plus 60h for PicoMEM
;AL = number of sectors to read/write (must be nonzero)  Command number for PicoMEM
;CH = low eight bits of cylinder number
;CL = sector number 1-63 (bits 0-5)
;high two bits of cylinder (bits 6-7, hard disk only)
;DH = head number
;DL = drive number (bit 7 set for hard disk)
;ES:BX -> data buffer

;STR_IRQ13 DB 'IRQ13h',0
;STR_Mouse DB 'Enable Mouse',0

PM_Int13h:
    PUSHF

	INC byte CS:[Int13h_Flag]

    CMP AH,60h
	JNE Not_PMBIOS_Call
	CMP DX,1234h
	JNE Int13_CallPrevIRQ

    POPF
	JMP PM_BIOS
	
Not_PMBIOS_Call:	

	PUSH BX	      ; Save BX (Only BX is used here) Warning : don't change another reg    

;CALL IRQ_DisplayRegs

	MOV BL,DL     ; Drive Number is in DL
	CMP BL,80h    ; No Floppy for the moment
	JAE Int13_CheckHDDNb
	CMP BL,01h
	JA Int13_Device_Notenabled
	XOR BH,BH
	CMP byte CS:[FDD0_Attribute+BX],80h   ; Check if the disk is enabled.
	JAE PM_Int13_MyDisk	                  ; Ok, Floppy found

; Floppy is not in the PicoMEM
	MOV DL,byte CS:[FDD0_Attribute+BX]    ; Change DL (New Disk number)

;push_all
;    PUSH AX
;	MOV BL,7
;    PUSH DX	
;	v
;	POP DX	
	
;	MOV AL,DL
;	CALL BIOS_PrintAL_Hex
;	printchar_w 'C'
	
;	POP AX
;	MOV AL,AH
;	CALL BIOS_PrintAL_Hex
;pop_all

    JMP Int13_Device_Notenabled			  ; No emulated Floppy, JMP to legacy BIOS
Int13_CheckHDDNb:     
	CMP BL,83h
	JA Int13_Device_Notenabled
	XOR BH,BH
	CMP byte [CS:BX+HDD0_Attribute-80h],80h ; Check if the disk is enabled.
	JAE PM_Int13_MyDisk                   ; Ok, HDD found
; Disk is not in the PicoMEM
	MOV DL,byte CS:[BX+HDD0_Attribute-80h]
	ADD DL,80h								; Change DL : Disk number

Int13_Device_Notenabled:	; Call the Real BIOS Int13h
	POP BX

Int13_CallPrevIRQ:
	DEC byte CS:[Int13h_Flag]
	POPF
	JMP FAR [CS:OldInt13h]  ; CALL the saved other IRQ13h !

; PicoMEM Disk BIOS (int13h	)
PM_Int13_MyDisk:
	POP BX
    POPF    ; Restore the Flags

    CALL PM_SendRegs_int13    ; Send registers to the PicoMEM

    PUSH CS
	POP DS

    STI
    CMP byte [PMCFG_PSRAM_Ext],0
	JE Int13_NoCLI		; If no RAM is emulated in PSRAM, no conflict possible (PSRAM/MicroSD)
    CLI    ; Test to avoid PSRAM Conflict
Int13_NoCLI:

; Change Stack to BIOS RAM  > Not Needed, Disk fontion does not use the Stack
;	printstr_w STR_IRQ13

	CALL PM_WaitCMDEnd		  ; Check if previous command ended

    MOV AH,CMD_Int13h
	CALL PM_SendCMD           ; Do CMD_Int13h Command	(Don't wait previous command end)
	
	CALL BIOS_DoPCCMD         ; The PicoMEM Will send commands to the BIOS (Copy data Block, return)

;    CALL PM_WaitCMDEnd_Timeout ; Wait that the command complete
;	 CALL PM_GetRegs_int13     ; Get the registers returned by the PicoMEM (Even the Flag)
; 	 CALL IRQ_DisplayData

    STI  ; Restore IRQ

	CALL PM_GetRegs_int13     ; Get the registers returned by the PicoMEM (Even the Flag)

	DEC byte CS:[Int13h_Flag]
	
	RETF	2 			      ; return from int with current flags
	
; Int 13h End	

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
STR_PressA_B DB 'Press A to Boot from Floppy , B for Basic/ROM ',0
STR_InsertDisk DB 0x0D,0x0A,'> No Boot Disk found - Insert a Floppy and Press any Key.',0
STR_BAD_MBR DB ': Incorrect MBR ',0
STR_Int13hError DB ': Int13h Error ',0 
STR_BOOT    DB ': Boot... ',0x0D,0x0A,0
STR_Int19_NotFirst DB 'Boot failed : Select another image or disable the PicoMEM Boot Code.',0x0D,0x0A,0

PM_Int19h:

; No Need to save registers, it is "not" a normal Interrupt	
	PUSH CS
	POP DS

    CMP byte [Int19h_Counter],0		; This counter is initialized at each Init
	JZ Int19_FirstTime
; If the Int19h was called more than one time or Boot Failed
PM_Int19h_BootFailed:
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
	CMP AL,'R'
	JE PM_Reboot
	CMP AL,'r'
	JE PM_Reboot

	JMP PM_Int19h_Start
PM_Int19h_FDD:
	MOV byte CS:[BOOT_FDDFirst],1	; If A Pressed, force Boot on Floppy first
	JMP PM_Int19h_Start
PM_Int19h_BASIC:
	Int 18h							; Int 18h > Go to the Basic
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

BOOT_Drives:

Int19_BootLoop:
	PUSH CX				; Save drives counter

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
	XOR AX,AX	; Test
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

;***********************************************************************************************
;*                       PM_DoSetup : Full HW Detection and Setup code                         *
;*                                                                                             *
;*  Detect the Hardware, start the BIOS Setup code and initialise the Board                    *
;*                                                                                             *
;***********************************************************************************************

PM_Do_Setup:

	INC byte [BV_BootCount]

; *** POST: Read/Detect the PC Configuration informations, only during the First BOOT
    CMP byte [BV_BootCount],1
	JNE POST_Skip_PC_Config_Detect

; - Read the initial BIOS Disk infos
    CALL BIOS_GetPCDriveInfos
	
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

    CALL PM_Display_FullHeader

; ****** Code doing the Menu and the configuration  *****

	STI 				; Enable interrupts (for the Keyboard)
	
    printstr_w STR_PressS

	MOV CX,1
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
    CALL Do_BIOS_Menu
; Display again the Header after BIOS Menu Exit
    CALL PM_Display_FullHeader
	
BIOS_Final_Init:

%if DOS_COM=1		; Don't redirect IRQ if DOS (Debug)
	RET
%endif ; DOS_COM=1

; Finalize all the configs after the setup or Wait End

    CALL PM_FinalConfig
    CALL DisplayMEMConfig		; Display the Memory configuration after Final config

    CALL PM_InstallIRQ13		; Mount the Disks and Install the IRQ 13h

	CALL Display_DisksList

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
PM_SendRegs_int13:
	PUSH ES  ; Save ES that is needed for the Memory source/Destination
	PUSH DI
	
	PUSH ES
	PUSH CS
	POP ES
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
	POP AX ; GET ES
	STOSW          ; ES
	POP AX ; GET DI
	STOSW          ; DI
	PUSHF
	POP AX ; GET Flags
	STOSW          ; Flags
	POP ES
	RET  ;PM_SendRegs_int13

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

    CMP byte [PMCFG_WifiIRQ],0
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
    JMP Init_DisplayRES	
Init_Failure:	
    MOV SI,STR_FAILURE
    JMP Init_DisplayRES
Init_Disabled:
    MOV SI,STR_DISABLED
    JMP Init_DisplayRES
Init_Ok:
    MOV SI,STR_OK

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
	
	JMP DoDisplay_CSIP
	
DoDisplayStatus:
	CALL BIOS_Printstr ; Display the Status Value (Text)

    POP AX   ; DEBUG Don't display CS:IP anymore if not Unknown Status
	RET

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

;!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! DEBUG Code disabled !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
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
	CALL PM_SendRegs_int13
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