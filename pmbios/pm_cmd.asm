;This program is free software; you can redistribute it and/or
;modify it under the terms of the GNU General Public License
;as published by the Free Software Foundation; either version 2
;of the License, or (at your option) any later version.
;
;This program is distributed in the hope that it will be useful,
;but WITHOUT ANY WARRANTY; without even the implied warranty of
;MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License along with this program. 
; If not, see <https://www.gnu.org/licenses/>.


%macro mov_ds_cs 0 ; Print String in White
	MOV AX,CS
	MOV DS,AX
%endmacro

%macro mov_es_cs 0 ; Print String in White
	MOV AX,CS
	MOV ES,AX
%endmacro
; *
; * Command interpreter : The Commands are sent by the Pi Pico
; *
%ifndef PM_CMD
%assign PM_CMD 1

;** List of Status
; Status of the PC Code  !! Warning 2 codes redifines in  PM_HW
%define PCC_PCS_NOTREADY   0x00  ; PC Not Waiting for a Command (Set when the Picomem ask to stop)
%define PCC_PCS_WAITCMD    0x01  ; PC Waiting for an Command
%define PCC_PCS_INPROGRESS 0X02  ; PC Command in Progress
%define PCC_PCS_COMPLETED  0X03  ; PC Command completed, Wait the PicoMEM to ask to Wait for a command
%define PCC_PCS_ERROR      0x10  ; PC Command Error
%define PCC_PCS_RESET      0x20  ; When the Pi Pico is blocked waiting for the PC, use this to reset

; Status of the PicoMEM Code
%define PCC_PMS_DOWAITCMD    0x00  ; Ask the PC to Wait for a command
%define PCC_PMS_COMMAND_SENT 0x01  ; Tell the PC that a command is Ready
%define PCC_PMS_NOCMD        0x02  ; No More Command

;// List of Commands

%define PCC_EndCommand    0   ; // No more Command

%define PCC_Printf        2   ; // Display a 0 terminiated string with Int10h
%define PCC_GetScreenMode 3   ; // Get the currently used Screen Mode
%define PCC_GetPos        4   ; // Get Screen Offset
%define PCC_SetPos        5   ; // 
%define PCC_IN8           10
%define PCC_IN16          11
%define PCC_OUT8          12
%define PCC_OUT16         13
%define PCC_MemCopyB      14
%define PCC_MemCopyW      15
%define PCC_MemCopyW_512b 16
%define PCC_IRQ           17  ; //Send an IRQ and get the result

%define MAX_PCCMD         17

STR_CMDNB DB "CMD: ",0
STR_WAIT DB "-Wait-",0
STR_WAITCMD DB "-Wait CMD-",0
STR_END  DB "-CMD End-",0


BIOS_DoPCCMD:
	MOV byte CS:[PCCR_SectNB],0
	
IRQ_DoPCCMD:
	mov_ds_cs					  ; Set DS=CS							

; // ************ STATE WaitCCMD : Wait that the PicoMEM Send a Command
PCC_STATE_WaitCMD:
     CLD  ; Set Auto increment direction for Memory copy code
;printstr_w STR_WAITCMD

PCC_Do_WaitCMD:
	MOV byte [PCCR_PCSTATE],PCC_PCS_WAITCMD  ; Place the PC in the Wait Command State
LOOP_WaitCMD:
    CMP byte [PCCR_PMSTATE],PCC_PMS_COMMAND_SENT
	JNE LOOP_WaitCMD

; // ************ STATE Execute Command
	XOR BX,BX
	MOV BL,[PCCR_CMD]       ; Read the next command *
;	CMP BL,MAX_PCCMD
;	JA  PCC_DoCommandErr

	MOV byte [PCCR_PCSTATE],PCC_PCS_INPROGRESS ; Change State to Command in progress *
	SHL BX,1
	JMP [CMD_TABLE+BX]     ; Jump to the command *

; // ************ STATE Command Completed
; The commands jump here when completed or return

PCCMD_Completed:  ; When the command is completed, wait the PCC_PMS_DOWAITCMD command from the Pico
;printstr_w STR_END
	MOV byte CS:[PCCR_PCSTATE],PCC_PCS_COMPLETED    ; Place the PC in Command Completed State 
PCCMD_Completed_Loop:
	CMP byte CS:[PCCR_PMSTATE],PCC_PMS_DOWAITCMD    ; Wait that the Pico ask the PC to Wait for the next command	
	JNE PCCMD_Completed_Loop
	JMP PCC_Do_WaitCMD
	
PCC_DoCommandErr:  ; Error :  State is PCC_PCS_ERROR and Quit
	MOV byte CS:[PCCR_PCSTATE],PCC_PCS_ERROR
	RET

CMD_TABLE DW PCC_DoEndCommand       ;0
          DW PCC_NoCMD              ;1
          DW PCC_Do_Printf			;2
          DW PCC_Do_GetScreenMode	;3
		  DW PPC_Do_Getpos			;4
		  DW PCC_Do_SetPos			;5
		  DW PCC_NoCMD
		  DW PCC_NoCMD
		  DW PCC_NoCMD
		  DW PCC_NoCMD
		  DW PCC_Do_IN8	            ;10
		  DW PCC_Do_IN16            ;11
		  DW PCC_Do_OUT8            ;12
		  DW PCC_Do_OUT16           ;13
		  DW PCC_Do_MemCopyB        ;14
		  DW PCC_Do_MemCopyW        ;15
		  DW PCC_Do_MemCopyW_512b   ;16
		  DW PCC_NoCMD

PCC_DoEndCommand:
	MOV byte CS:[PCCR_PCSTATE],PCC_PCS_NOTREADY
	RET

PCC_NoCMD:
    JMP PCCMD_Completed
	
; * PCC_Printf
; * Param : Null terminated string to print
PCC_Do_Printf: ;Ok
    PUSH DS   ; Preserve DS
	PUSH ES
	PUSH SI
    PUSH CS
	POP DS
	MOV SI,PCCR_Param
    CALL BIOS_Printstr
	POP SI
	POP ES
	POP DS
    JMP PCCMD_Completed

; * PCC_GetScreenMode
; * Return in Param the Screen Mode, number of column and page
PCC_Do_GetScreenMode:
	MOV AH,03h
	INT 10h
    MOV CS:[PCCR_Param],AL   ; Mode
	MOV CS:[PCCR_Param+1],AH ; Columns
	MOV CS:[PCCR_Param+2],BH ; Current Page	
    JMP PCCMD_Completed

; * PPC_Getpos
; * Return in Param the Cursor Position (Row, Column)
PPC_Do_Getpos:
	MOV BH,0       ; Page 0
	MOV AH,03h
	INT 10h
    MOV CS:[PCCR_Param],DH   ; Row
	MOV CS:[PCCR_Param+1],DL ; Column
    JMP PCCMD_Completed	
; ! CX already changed	

; * PCC_SetPos
; * Param : Cursor Position (Row, Column)
PCC_Do_SetPos:
    MOV DH,CS:[PCCR_Param]   ; Row
	MOV DL,CS:[PCCR_Param+1] ; Column
	MOV BH,0       ; Page 0
	MOV AH,02h
	INT 10h
    JMP PCCMD_Completed

PCC_Do_IN8:
    MOV DX,[PCCR_Param]   ; Port number
	IN AL,DX
    MOV [PCCR_Param],AL   ; Return the result	
    JMP PCCMD_Completed

PCC_Do_IN16:
    MOV DX,[PCCR_Param]   ; Port number
	IN AX,DX
    MOV [PCCR_Param],AX   ; Return the result
    JMP PCCMD_Completed

PCC_Do_OUT8:
    MOV DX,[PCCR_Param]   ; Port number
    MOV AL,[PCCR_Param+2] ; Value
	OUT DX,AX
    JMP PCCMD_Completed

PCC_Do_OUT16:
    MOV DX,[PCCR_Param]   ; Port number
    MOV AX,[PCCR_Param+2] ; Value
	OUT DX,AX
    JMP PCCMD_Completed

; * PCC_MemCopyB : Copy a memory block from DS:SI to ES:DI (Bytes)
; *	Param: DS:SI, ES:DI and number of Bytes
PCC_Do_MemCopyB:
    LDS SI,CS:[PCCR_Param]    	  ; Read SI then DS
    LES DI,CS:[PCCR_Param+4]  	  ; Read DI then ES
	MOV CX,word CS:[PCCR_Param+8] ; Read the Nb of Byte to move
	REP MOVSB
	mov_ds_cs					  ; Restore DS							 
    JMP PCCMD_Completed	

; * PCC_MemCopyW : Copy a memory block from DS:SI to ES:DI (Word)
; *	Param: DS:SI, ES:DI and number of Word
PCC_Do_MemCopyW:
    LDS SI,CS:[PCCR_Param]    	  ; Read SI then DS
    LES DI,CS:[PCCR_Param+4]  	  ; Read DI then ES
	MOV CX,word CS:[PCCR_Param+8] ; Read the Nb of Byte to move
	REP MOVSW
	mov_ds_cs					  ; Restore DS							 
    JMP PCCMD_Completed	
	
; * PCC_MemCopyW_512b : Copy 512 Bytes from DS:SI to ES:DI
; *	Param: DS:SI and ES:DI
PCC_Do_MemCopyW_512b:
    LDS SI,CS:[PCCR_Param]    ; Read SI then DS
    LES DI,CS:[PCCR_Param+4]  ; Read DI then ES
	MOV CX,256                ; Read the Nb of Byte to move
	REP MOVSW
	mov_ds_cs					  ; Restore DS							 
    JMP PCCMD_Completed	
	
; DEBUG  Display
; MOV AL,CS:[PCCMD_SectNB]
; CMP AL,CS:[PM_I]
; JE NotDisplayNB
; MOV CS:[PM_I],AL
; MOV BL,7    ; Light Grey
; CALL BIOS_PrintAL_Hex
; MOV AL,byte CS:[PCCMD_NB]
; CALL BIOS_PrintAL_Hex 
; MOV AL,byte CS:[PCCR_PCSTATE]
; CALL BIOS_PrintAL_Hex 
; MOV AL,byte CS:[PCCMD_v2]
; CALL BIOS_PrintAL_Hex  
; printstr_w STR_Space 
;NotDisplayNB: 
; END DEBUG Display

%endif 