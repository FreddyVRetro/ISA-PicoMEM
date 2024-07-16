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
%ifndef PM_HW
%assign PM_HW 1

%define PM_Diag 0

%assign PM_BasePort 0x2A0     ; To change to be a variable....

%define PCC_PCS_NOTREADY   0x00  ; PC Not Waiting for a Command (Set when the Picomem ask to stop)
%define PCC_PCS_RESET      0x20  ; When the Pi Pico is blocked waiting for the PC, use this to reset

%assign PORT_PM_CMD_STATUS 0 ;Write Command, Read Status
%assign PORT_PM_CMD_DATAL  1 ;Write/Read The Command Parameter L
%assign PORT_PM_CMD_DATAH  2 ;Write/Read The Command Parameter H
%assign PORT_TEST          3 ;Test port
;%assign PORT_PM_REG        3 ;Write/Read the register number
;%assign PORT_PM_REG_DATA   4 ;Write/Read the selected Register Data

;// 0x Basic Config Commands
%assign CMD_Reset         0x00
%assign CMD_GetBasePort   0x01
%assign CMD_SetBasePort   0x02
%assign CMD_GetDEVType    0x03   ; Not used
%assign CMD_SetDEVType    0x04   ; Not used
%assign CMD_GetMEMType    0x05
%assign CMD_SetMEMType    0x06
%assign CMD_MEM_Init      0x07   ;// Configure the memory based on the Config table
%assign CMD_SaveCfg       0x08   ;// Save the Configuration to the file
%assign CMD_DEV_Init      0x09

;// 2x Debug/Test CMD
%assign CMD_SetMEM        0x22   ;// Set the First 64Kb of RAM to the SetRAMVal
%assign CMD_TestMEM       0x23 
%assign CMD_displayCMD    0x24
%assign CMD_Test_PCCMD    0x25
%assign CMD_StartUSBUART  0x28   ; Avoid using it in the BIOS

%assign CMD_SendIRQ       0x30   ;// Enable the IRQ
%assign CMD_IRQAck        0x31   ;// Acknowledge IRQ

%assign CMD_USB_Enable    0x50
%assign CMD_USB_Disable   0x51
%assign CMD_Mouse_Enable  0x52
%assign CMD_Mouse_Disable 0x53
%assign CMD_Keyb_Enable   0x54
%assign CMD_Keyb_Disable  0x55
%assign CMD_Joy_Enable    0x56
%assign CMD_Joy_Disable   0x57

%assign CMD_Wifi_Infos    0x60

;// 8x Disk Commands
%assign CMD_HDD_Getlist   0x80  ;// Write the list of hdd images into the Disk memory buffer
%assign CMD_HDD_Getinfos  0x81  ;// Write the hdd infos in the Disk Memory buffer
%assign CMD_HDD_Mount     0x82  ;// Map a disk image
%assign CMD_FDD_Getlist   0x83  ;// Write the list of file name into the Disk memory buffer
%assign CMD_FDD_Getinfos  0x84  ;// Write the disk infos in the Disk Memory buffer
%assign CMD_FDD_Mount     0x85  ;// Map a disk image
%assign CMD_HDD_NewImage  0x86  ;// Create a new HDD Image
%assign CMD_Int13h        0x88  ;// Perform the Int13h command, registers need to be copied to the BIOS RAM Before

%assign STAT_READY         00h;
%assign STAT_CMDINPROGRESS 01h;
%assign STAT_CMDERROR      02h;
%assign STAT_CMDNOTFOUND   03h;
%assign STAT_INIT          04h;
%assign STAT_WAITCOM       05h; 
%assign STAT_LAST          05h;


ERR_WRITE    	DB 'Write to file failed'
ERR_NOERROR     DB 0
ERR_IMGEXIST 	DB 'Image already exist',0
ERR_NOHDDDIR	DB 'HDD directory missing',0
ERR_NOFDDDIR    DB 'FLOPPY directory missing',0
ERR_DISKFULL	DB 'uSD Full',0
ERR_DISK        DB 'Disk Operation failed',0
ERR_FILEREAD    DB 'No Image found',0
ERR_MOUTFAIL    DB 'Disk Mount failed',0
ERR_UNKN        DB 'Unknown Error',0

CMDERR_Table DW ERR_NOERROR,ERR_WRITE,ERR_IMGEXIST,ERR_NOHDDDIR,ERR_NOFDDDIR,ERR_DISKFULL,ERR_DISK,ERR_FILEREAD,ERR_MOUTFAIL


STR_WCMErr DB '-WCmd Err ',0
STR_RESET  DB 'Fatal: Reset Failed ',0

%macro PM_Read_DataL 0
    MOV DX,PM_BasePort
    INC DX
    IN AL,DX		; Read the Data L Register
%endmacro

%macro PM_Read_DataH 0
    MOV DX,PM_BasePort
    INC DX
	INC DX
    IN AL,DX		; Read the Data L Register
%endmacro

%macro PM_Read_DataW 0
    MOV DX,PM_BasePort
    INC DX
    IN AX,DX		; Read the Data L Register
%endmacro

; Read in Loop the test port and check if the next value is the value +1
; DC : Base Port
; CX : Number of time the port is tested
PM_TestReadPort:
  MOV DX,PM_BasePort
%if DOS_COM=1
  MOV AH,0
  RET
%endif
;  MOV DX,PM_BasePort
  ADD DX,PORT_TEST
  MOV CX,10
  IN AL,DX
  MOV AH,AL
.TestReadLoop:
  IN AL,DX
  INC AH
  CMP AL,AH
  JNE .TestRead_Fail
  LOOP .TestReadLoop
  MOV AL,1
  JMP .TestReadEnd
.TestRead_Fail:
  XOR AX,AX
.TestReadEnd:
  RET
  
; Load the list of HDD
; Return 
; Nb of HDD in AL and SD_HDDTotal
PM_ReadHDDList:
%if DOS_COM=1
    MOV byte [PM_HDDTotal],17
	MOV byte [ListLoaded],2
	RET
%endif
    CMP byte [ListLoaded],2
	JE PM_HDDLoaded
	CLI                     ; SD Access : Disable IRQ (Not needed in the BIOS, just for info)
	MOV AH,CMD_HDD_Getlist
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end
	STI
	JNC CMD_HDD_GetlistOk
	CALL PM_GetErrorString

	STC		; Ko
    RET	
CMD_HDD_GetlistOk:

    MOV DX,PM_BasePort
	ADD DX,PORT_PM_CMD_DATAL
	IN AL,DX
    MOV [PM_HDDTotal],AL    ; The command return the Nb of disk found in a register
	MOV byte [ListLoaded],2
PM_HDDLoaded:	
	CLC 	; Ok
    RET

; Load the list of FDD
; Return 
; Nb of FDD in AL and SD_FDDTotal
; Return : C=0,Ok CF=1,Fail

PM_ReadFDDList:
 %if DOS_COM=1
    MOV byte [PM_FDDTotal],3
	MOV byte [ListLoaded],1
	RET
%endif
	
    CMP byte [ListLoaded],1
	JE PM_FDDLoaded
	CLI                     ; SD Access : Disable IRQ (Not needed in the BIOS, just for info)
	MOV AH,CMD_FDD_Getlist
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end
	STI
	JNC CMD_FDD_GetlistOk
	CALL PM_GetErrorString

	STC		; Ko
    RET	
CMD_FDD_GetlistOk:
	
    MOV DX,PM_BasePort
	ADD DX,PORT_PM_CMD_DATAL
	IN AL,DX
    MOV [PM_FDDTotal],AL   ; The command return the Nb of disk found in a register
	MOV byte [ListLoaded],1	
PM_FDDLoaded:	
	CLC 	; Ok
    RET

; Load the list of Floppy
; Return 
; Nb of HDD in AL and SD_DiskTotal
PM_ReadROMList:
%if DOS_COM=1
    MOV byte [PM_ROMTotal],3
	MOV byte [ListLoaded],3
	RET
%endif
    MOV byte [PM_ROMTotal],0  ; Debug, to remove
	MOV byte [ListLoaded],3

    CMP byte [ListLoaded],3
	JE PM_ROMLoaded
	CLI                      ; SD Access : Disable IRQ (Not needed in the BIOS, just for info)
	MOV AH,CMD_HDD_Getlist
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end
	STI

    MOV DX,PM_BasePort
	ADD DX,PORT_PM_CMD_DATAL
	IN AL,DX
    MOV [PM_ROMTotal],AL   ; The command return the Nb of disk found in a register
	MOV byte [ListLoaded],3
PM_ROMLoaded:
    RET


PM_Do_FDD_Mount:
    MOV AH,CMD_FDD_Mount
	CALL PM_SendCMD           ; Do CMD_FDD_Mount Command
	CALL PM_WaitCMDEnd        ; Wait for the command End
	RET

STR_TO     DB '-TimeOut ',0

; **** PM_WaitReady : Wait if the PM is ready to receive command
PM_WaitReady_Timeout:
%if DOS_COM=1   ; Avoid Crash
 	CLC 	; Ok
    RET
%endif
    PUSH DX
    PUSH CX
    MOV DX,PM_BasePort
    XOR CX,CX
PM_WaitRDYLOOP:
    IN AL,DX
    CMP AL,STAT_READY
    JE PM_WaitRDYOk
    NOP
    NOP
    NOP
    NOP
    LOOP PM_WaitRDYLOOP
    POP CX
    PUSH AX
    printstr_w STR_TO	; Display TimeOut First
    POP AX
    CALL DISPLAY_Status
    STC  
PM_WaitRDYOk:
    POP CX
    POP DX
 	CLC 	; Ok
    RET

; **** PM_SendCMD *****
; AH : Command number
; BX : Command Parameter
; BH : PM_CmdDataL  BL : PM_CmdDataH
; DX : PM Base Port
; DS : Must be CS
; CF clear if successful
PM_SendCMD: 
	MOV DX,PM_BasePort
%if DOS_COM=1
 	CLC 	; Ok
	RET
%endif
; First, check if ready to send a command
Cmd_CheckReady:
	IN AL,DX
	OR AL,AL
	JNZ Cmd_NotReady
Cmd_Ready:  
	PUSH AX
	MOV AX,BX
	INC DX
	OUT DX,AX   ;Send CMD Data   !!! Must be changed
	POP AX
	DEC DX
	MOV AL,AH
	OUT DX,AL   ;Send the CMD
 	CLC 		; Ok
	RET
  
Cmd_NotReady:  ; If Unknown status, Ignore and retry (Should never happen, to debug)
	CMP AL,STAT_LAST
	JBE Display_StatusErr_SendCMD
	CALL PM_Catch_Unknown_Status
	JMP Cmd_CheckReady
  
Display_StatusErr_SendCMD:
	CALL DISPLAY_Status
	STC
	RET

;STR_ResetEnd DB 'Reset End',0

; **** PM_Reset
; Try to Reset/stop a blocked commands or just clean the Error status.
PM_Reset:
	MOV DX,PM_BasePort 
	IN AL,DX
	CMP AL,STAT_READY
	JE .Reset_Ok
	CMP AL,STAT_CMDINPROGRESS
	JNE .Reset_DoReset
; Reset the PicoMEM commands using the "PCC_PCS_RESET" State (Unlock locked disk read/Write)
	MOV byte CS:[PCCR_PCSTATE],PCC_PCS_RESET
.Reset_DoReset:  
	MOV AL,CMD_Reset
	OUT DX,AL		;Send the Reset commands
	XOR CX,CX
.Wait_Reset_Completed:
	IN AL,DX
	CMP AL,STAT_READY
	JE .Reset_Ok
	LOOP .Wait_Reset_Completed
	printstr_w STR_RESET	; Display Reset Error
.Reset_Ok:
	MOV byte CS:[PCCR_PCSTATE],PCC_PCS_NOTREADY
	
;	printstr_w STR_ResetEnd
	RET  

%if ADD_Debug=1

STR_WCMDTO DB '-WCmdTimeOut ',0

; PM_WaitCMDEnd_Timeout : Wait the command End with a Timeout
; !!! Do not use for Slow commands !!!
; DX : Base Port
; CF clear if successful
PM_WaitCMDEnd_Timeout:
	MOV DX,PM_BasePort
%if DOS_COM=1   ; Avoid Crash
 	CLC 	; Ok
	RET
%endif
	XOR CX,CX
PM_WaitCMDLOOP_TO:
	IN AL,DX
	CMP AL,STAT_READY
	JE PM_WaitCMDOk
	CMP AL,STAT_CMDINPROGRESS
	JNE PM_WaitCMD_CheckStatus
	LOOP PM_WaitCMDLOOP_TO
	PUSH AX
	printstr_w STR_WCMDTO	; Display TimeOut First
	POP AX
	CALL DISPLAY_Status  
	STC
	RET
PM_WaitCMD_CheckStatus:
	PUSH AX
	printstr_w STR_WCMErr	; Display Wait CMD Error (If return Status not Ready)
	POP AX  
	CALL DISPLAY_Status
	STC
	RET  
PM_WaitCMDOk:
 	CLC 	; Ok
	RET
%endif

; Wait for the current Command to finish  
; Outpout : C Set if Error

PM_WaitCMDEnd:
	MOV DX,PM_BasePort
%if DOS_COM=1   ; Avoid Crash
 	CLC 	; Ok
	RET
%endif

PM_WaitCMDLOOP:
	IN AL,DX
	CMP AL,STAT_READY
	JE PM_WaitCMDOk2
	NOP
	NOP
	CMP AL,STAT_CMDINPROGRESS
	JE PM_WaitCMDLOOP
	
	CMP AL,STAT_LAST
	JBE PM_WaitCMDLOOP_NoStatErr
	CALL PM_Catch_Unknown_Status
	JMP PM_WaitCMD_NotExpected  
PM_WaitCMDLOOP_NoStatErr:

	CMP AL,STAT_CMDERROR
	JNE PM_WaitCMD_NotExpected
	INC DX
	IN AL,DX				; Read the Error code from the Low Register
	STC
	RET
PM_WaitCMD_NotExpected:  
	PUSH AX
	printstr_w STR_WCMErr	; Display Wait CMD Error (If return Status not Ready)
	POP AX
	CALL DISPLAY_Status
	STC
	RET  
PM_WaitCMDOk2:  
 	CLC 	; Ok
	RET  

PM_Catch_Unknown_Status:
; I/O is FFh, try to catch on logic analyzer
	MOV AL,0AAh
	ADD DX,PORT_TEST ; Write to Test port generate an "IRQ"
	OUT DX,AL
  
	MOV DX,PM_BasePort
	RET
  
%if PM_Diag=1

STR_Test DB 0x0D,0x0A,'Port 2A2 Test: ',0x0D,0x0A,0
STR_Test2 DB 0x0D,0x0A,'BIOS MEM Test: ',0x0D,0x0A,0
STR_Test3 DB 0x0D,0x0A,'DISK MEM Test: ',0x0D,0x0A,0

; Test/DIAG Code, various test code to enable/Disable when needed
PM_Diag:
    PUSH CS
	POP DS
	
TestLoop:
	
	printstr_w STR_Test

	MOV CX,10
PortTestLoop2:
	PUSH CX

; Test Port (W/R) Loop
    MOV DX,PM_BasePort
	ADD DX,2
	MOV BX,0
    MOV CX,50000

PortTestLoop:
    MOV AL,0AAh
	OUT DX,AL
	IN AL,DX
	CMP AL,0AAh
	JNE Err_PortTest
    MOV AL,055h
	OUT DX,AL
	IN AL,DX
	CMP AL,055h
	JNE Err_PortTest
	LOOP PortTestLoop
	
	JMP PortTestEnd    	
Err_PortTest:	
    INC DX
	OUT DX,AL
	DEC DX
	INC BX
	LOOP PortTestLoop
PortTestEnd:

    MOV AX,BX
    MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Dec 	; display the Status number	
    printstr_w STR_DDOT
	
	POP CX
	LOOP PortTestLoop2

; BIOS MEM Test

	printstr_w STR_Test2

    MOV CX,10
BMEMLoop2:
    PUSH CX

; Test BIOS MEM (W/R) Loop
    MOV DX,PM_BasePort+3
	MOV BX,0
    MOV CX,50000
	PUSH CS
	POP ES
	MOV AX,BV_MEMTst
	MOV DI,AX

BMEMLoop:
    MOV AX,0AA55h
	MOV ES:[DI],AX
	MOV AX,ES:[DI]
	CMP AX,0AA55h
	JNE Err_BMEMTest
    MOV AX,055AAh
	MOV ES:[DI],AX
	MOV AX,ES:[DI]
	CMP AX,055AAh
	JNE Err_BMEMTest
	LOOP BMEMLoop
	
	JMP BMEMEnd
Err_BMEMTest:	
	OUT DX,AL
	INC BX
	LOOP BMEMLoop
BMEMEnd:

    MOV AX,BX
    MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Dec 	; display the Status number	
    printstr_w STR_DDOT
	
	POP CX
	LOOP BMEMLoop2

; DISK MEM Test

   printstr_w STR_Test3

   MOV CX,10
DMEMLoop2:
   PUSH CX

; Test BIOS MEM (W/R) Loop
    MOV DX,PM_BasePort+3
	MOV BX,0
    MOV CX,50000
	PUSH CS
	POP ES
	MOV AX,PM_DISKB
	MOV DI,AX

DMEMLoop:
    MOV AX,0AA55h
	MOV ES:[DI],AX
	MOV AX,ES:[DI]
	CMP AX,0AA55h
	JNE Err_DMEMTest
    MOV AX,055AAh
	MOV ES:[DI],AX
	MOV AX,ES:[DI]
	CMP AX,055AAh
	JNE Err_DMEMTest
	LOOP DMEMLoop
	
	JMP DMEMEnd
Err_DMEMTest:	
	OUT DX,AL
	INC BX
	LOOP DMEMLoop
DMEMEnd:

    MOV AX,BX
    MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Dec 	; display the Status number	
    printstr_w STR_DDOT
	
	POP CX
	LOOP DMEMLoop2

; Go back to the First Test (I/O)
	JMP TestLoop

; Send a command to generate the bug
	MOV AH,CMD_HDD_Getlist
	CALL PM_SendCMD
;	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end

StatusTest:
;    MOV CX,2048
;    MOV DX,PM_BasePort
StatusTestLoop:
;    DEC CX
;	JE StatusTestDisplay
    IN AL,DX
    CMP AL,0FFh
	JE StatusTestDisplay
	JMP StatusTestLoop
    CMP AL,0
    JE StatusTestLoop
	CMP AL,1
    JE StatusTestLoop
StatusTestDisplay:
	MOV DX,PM_BasePort+PORT_TEST 
    OUT DX,AL				; Generate an IRQ Signal
    MOV BL,7
	CALL BIOS_PrintAL_Hex 	; display the Status number
	JMP StatusTest

	ret ;PM_Diag
%endif

; PM_GetErrorString : Get the offset of the Error string
; Input  : AL Error number
; Output : SI Offset to the Error string
; Destroy BX
PM_GetErrorString:
	XOR BX,BX
	MOV BL,AL
	SHL BX,1

	MOV AX,CS:[CMDERR_Table+BX]
	MOV SI,AX
	
	PUSH SI
	CALL PM_Reset			  ; Need to Reset after an error
	POP SI
	RET ;PM_GetErrorString
	
PM_GetWifiInfos:
    MOV AH,CMD_Wifi_Infos
	CALL PM_SendCMD         ; Get the Wifi Status Infos
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end	
	RET

%endif