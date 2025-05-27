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

%define PCC_IN8           1	  ; IO Read  8Bit
%define PCC_IN16          2   ; IO Read  16Bit
%define PCC_OUT8          3   ; IO Write 8Bit
%define PCC_OUT16         4   ; IO Write 16Bit
%define PCC_MemCopyB      5
%define PCC_MemCopyW      6
%define PCC_MemCopyW_Odd  7
%define PCC_MemCopyW_512b 8
%define PCC_MEMR16        9   ; // Memory Read 16Bit
%define PCC_MEMW8         10  ; // Memory Write 16Bit
%define PCC_MEMW16        11  ; // Memory Write 16Bit
%define PCC_DMA_READ      12  ; // Read requested DMA channel Page:Offset and size
%define PCC_DMA_WRITE     13  ; // Update the DMA channel Offset and size
%define PCC_IRQ13         14  ; // Send an IRQ13 and get the result (BIOS)
%define PCC_IRQ21         15  ; // Send an IRQ21 and get the result (DOS )

%define PCC_Printf        16  ; // Display a 0 terminiated string with Int10h
%define PCC_GetScreenMode 17  ; // Get the currently used Screen Mode
%define PCC_GetPos        18  ; // Get Screen Offset
%define PCC_SetPos        19  ; // Segment + Size for the Checksum
%define PCC_KeyPressed    20  ;
%define PCC_ReadKey       21  ;
%define PCC_SendKey       22  ;
%define PCC_ReadString    23  ;


%define PCC_Checksum      24  ; // Perform a Checksum from one Segment Start with a Size
%define PCC_SetRAMBlock16 25  ; // Write the same value to a memory Block
%define PCC_MEMWR8        26  ; // Write then read a byte
%define PCC_MEMWR16       27  ; // Write then read a word

%define MAX_PCCMD         27


CMD_TABLE DW PCC_DoEndCommand
CMD_1     DW PCC_Do_IN8
CMD_2	  DW PCC_Do_IN16
CMD_3	  DW PCC_Do_OUT8
CMD_4	  DW PCC_Do_OUT16
CMD_5	  DW PCC_Do_MemCopyB
CMD_6	  DW PCC_Do_MemCopyW
CMD_7	  DW PCC_Do_MemCopyW_Odd
CMD_8	  DW PCC_Do_MemCopyW_512b
CMD_9     DW PCC_Do_MR16
CMD_10    DW PCC_Do_MW8		  
          DW PCC_Do_MW16
		  DW PCC_Do_DMA_READ
		  DW PCC_Do_DMA_WRITE
		  DW PCC_NoCMD
		  DW PCC_NoCMD	  
		  
          DW PCC_Do_Printf
          DW PCC_Do_GetScreenMode
		  DW PPC_Do_Getpos
		  DW PCC_Do_SetPos
		  DW PCC_Do_KeyPressed
		  DW PCC_Do_ReadKey
		  DW PCC_Do_SendKey
		  DW PCC_Do_ReadString
		  
          DW PCC_Do_Checksum
		  DW PCC_Do_SetRAMBlock16
		  DW PCC_Do_MEMWR8
		  DW PCC_Do_MEMWR16
		  
;STR_CMDNB DB "CMD: ",0
;STR_WAIT DB "-Wait-",0
;STR_WAITCMD DB "-Wait CMD-",0
;STR_END  DB "-CMD End-",0

BIOS_DoPCCMD:
;	MOV byte CS:[PCCR_SectNB],0
	
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

PCC_DoEndCommand:
	MOV byte CS:[PCCR_PCSTATE],PCC_PCS_NOTREADY
	RET

PCC_NoCMD:
    JMP PCCMD_Completed
	
; * PCC_Printf
; * Param : Null terminated string to print
PCC_Do_Printf: ;Ok
	PUSH ES
	PUSH SI
    PUSH CS
	POP DS
	MOV SI,PCCR_Param
    CALL BIOS_Printstr
	POP SI
	POP ES
	mov_ds_cs			  ; Restore DS
    JMP PCCMD_Completed

; * PCC_GetScreenMode
; * Return in Param the Screen Mode, number of column and page
PCC_Do_GetScreenMode:
	MOV AH,03h
	INT 10h
	mov_ds_cs			  ; Restore DS	
    MOV [PCCR_Param],AL   ; Mode
	MOV [PCCR_Param+1],AH ; Columns
	MOV [PCCR_Param+2],BH ; Current Page	
    JMP PCCMD_Completed

; * PPC_Getpos
; * Return in Param the Cursor Position (Row, Column)
PPC_Do_Getpos:
	MOV BH,0       ; Page 0
	MOV AH,03h
	INT 10h
	mov_ds_cs	   	         ; Restore DS
    MOV [PCCR_Param],DH   ; Row
	MOV [PCCR_Param+1],DL ; Column
    JMP PCCMD_Completed	
; ! CX already changed	

; * PCC_SetPos
; * Param : Cursor Position (Row, Column)
PCC_Do_SetPos:
    MOV DH,[PCCR_Param]   ; Row
	MOV DL,[PCCR_Param+1] ; Column
	MOV BH,0       ; Page 0
	MOV AH,02h
	INT 10h
	mov_ds_cs				  ; Restore DS
    JMP PCCMD_Completed

; Do a 8Bit IN, change AX, DX
PCC_Do_IN8:
    MOV DX,[PCCR_Param]   ; Port number
	IN AL,DX
    MOV [PCCR_Param],AL   ; Return the result	
    JMP PCCMD_Completed

; Do a 16Bit IN, change AX, DX
PCC_Do_IN16:
    MOV DX,[PCCR_Param]   ; Port number
	IN AX,DX
    MOV [PCCR_Param],AX   ; Return the result
    JMP PCCMD_Completed

; Do a 8Bit OUT, change AX, DX
PCC_Do_OUT8:
    MOV DX,[PCCR_Param]   ; Port number
    MOV AL,[PCCR_Param+2] ; Value
	OUT DX,AL
    JMP PCCMD_Completed

; Do an 16Bit IN, change AX, DX
PCC_Do_OUT16:
    MOV DX,[PCCR_Param]   ; Port number
    MOV AX,[PCCR_Param+2] ; Value
	OUT DX,AX
    JMP PCCMD_Completed

PCC_Do_MR16:
    LES DI,[PCCR_Param]   ; Address
    MOV AX,ES:[DI]        ; Read the data
	MOV [PCCR_Param],AX   ; Send the result
    JMP PCCMD_Completed

PCC_Do_MW8:
    LES DI,[PCCR_Param]   ; Address
	MOV AL,[PCCR_Param+4] ; Data
    MOV ES:[DI],AL        ; Write the data
    JMP PCCMD_Completed
	
PCC_Do_MW16:
    LES DI,[PCCR_Param]   ; Address
	MOV AX,[PCCR_Param+4] ; Data
    MOV ES:[DI],AX        ; Write the data
    JMP PCCMD_Completed

; * PCC_MemCopyB : Copy a memory block from DS:SI to ES:DI (Bytes)
; *	Param: DS:SI, ES:DI and number of Bytes
PCC_Do_MemCopyB:
    LES DI,[PCCR_Param+4]  	      ; Read DI then ES (Targer Ptr)
	MOV CX,word [PCCR_Param+8]    ; Read the Nb of Bytes to move (Target Ptr)
    LDS SI,CS:[PCCR_Param]    	  ; Read SI then DS
	REP MOVSB
	mov_ds_cs					  ; Restore DS
    JMP PCCMD_Completed

; * PCC_MemCopyW : Copy a memory block from DS:SI to ES:DI (Word)
; *	Param: DS:SI, ES:DI and number of Word
PCC_Do_MemCopyW:
    LES DI,[PCCR_Param+4]  	      ; Read DI then ES
	MOV CX,word [PCCR_Param+8]    ; Read the Nb of Word to move
    LDS SI,CS:[PCCR_Param]    	  ; Read SI then DS
	REP MOVSW
	mov_ds_cs					  ; Restore DS
    JMP PCCMD_Completed

; * PCC_MemCopyW_Odd : Copy a memory block from DS:SI to ES:DI (Word plus one byte)
; *	Param: DS:SI, ES:DI and number of Word
PCC_Do_MemCopyW_Odd:
    LES DI,[PCCR_Param+4]  	      ; Read DI then ES
	MOV CX,word [PCCR_Param+8]    ; Read the Nb of Word to move
    LDS SI,CS:[PCCR_Param]    	  ; Read SI then DS
	REP MOVSW
	MOVSB						  ; Move one aditionnal byte
	mov_ds_cs					  ; Restore DS
    JMP PCCMD_Completed

; * PCC_MemCopyW_512b : Copy 512 Bytes from DS:SI to ES:DI > Used to copy sectors
; *	Param: DS:SI and ES:DI
PCC_Do_MemCopyW_512b:
    LES DI,[PCCR_Param+4]     ; Read DI then ES (Target Ptr)
    LDS SI,CS:[PCCR_Param]    ; Read SI then DS (Source Ptr)
	MOV CX,256                ; Set Nb of Word to move
	REP MOVSW
	mov_ds_cs				  ; Restore DS
    JMP PCCMD_Completed	


PCC_Do_DMA_READ:  ; Get the DMA controller minimal info to perform a Data Transfer

; Get the DMA number
		XOR AX,AX
        CLI
		OUT 0Ch,AL           ; clear DMA flipflop (To read the values in the correct order)
		CMP byte [PCCR_Param],1	 ; Read the DMA number
		JNE READ_DMA3

; Read the DMA Values (Address and Transfer size)

; DMA 1
; Read the DMA page
        IN AL,83h
		MOV [PCCR_Param+1],AL	; Return the page
; Read the DMA offset
        IN AL,02h	; LSB
		XCHG AH,AL
		IN AL,02h	; MSB
		XCHG AH,AL
		MOV [PCCR_Param+2],AX   ; Return the Offset
; Read the length
		IN AL,03h
		XCHG AH,AL
		IN AL,03h
		XCHG AH,AL
		MOV [PCCR_Param+4],AX   ; Return the length
		
	    STI
		JMP PCCMD_Completed

READ_DMA3:
; Read the DMA page
		IN AL,82h
		MOV [PCCR_Param+1],AL	; Return the page
		
; Read the DMA offset
        IN AL,06h	; LSB
		XCHG AH,AL
		IN AL,06h	; MSB
		XCHG AH,AL
		MOV [PCCR_Param+2],AX   ; Return the Offset
; Read the length
		IN AL,07h
		XCHG AH,AL
		IN AL,07h
		XCHG AH,AL
		MOV [PCCR_Param+4],AX   ; Return the length

	    STI
		JMP PCCMD_Completed

PCC_Do_DMA_WRITE:
		
; Get the DMA number
		XOR AX,AX
        CLI
		OUT 0Ch,AL               ; Clear DMA flipflop (To read the values in the correct order)
		CMP byte [PCCR_Param],1	 ; Read the DMA number
		JNE WRITE_DMA3

; DMA 1
; Don't update the  DMA page
; Update the DMA offset
		MOV AX,[PCCR_Param+2]
        OUT 02h,AL	; LSB
		XCHG AH,AL
		OUT 02h,AL	; MSB
		XCHG AH,AL
; Update the length
		MOV AX,[PCCR_Param+4]
		OUT 03h,AL
		XCHG AH,AL
		OUT 03h,AL

	    STI
		JMP PCCMD_Completed

WRITE_DMA3:
; DMA 3
; Don't update the  DMA page
; Update the DMA offset
		MOV AX,[PCCR_Param+2]
        OUT 06h,AL	; LSB
		XCHG AH,AL
		OUT 06h,AL	; MSB
		XCHG AH,AL
; Update the length
		MOV AX,[PCCR_Param+4]
		OUT 07h,AL
		XCHG AH,AL
		OUT 07h,AL

	    STI
		JMP PCCMD_Completed

PCC_Do_Checksum:
		MOV CX,[PCCR_Param+2]
		MOV AX,[PCCR_Param]
		MOV DS,AX
		XOR AX,AX
		MOV SI,AX
Do_ChecksumLoop:
        ADD AL,[SI]
		LOOP Do_ChecksumLoop
		MOV [PCCR_Param],AL		; Return the checksum

	    mov_ds_cs               ; Restore DS
		JMP PCCMD_Completed
		

	
PCC_Do_KeyPressed:
        MOV AH,01h
        INT 16h
		MOV byte [PCCR_Param],1
		JNZ PCC_Do_KeyPressed_key
		MOV byte [PCCR_Param],0		
PCC_Do_KeyPressed_key:
        mov_ds_cs	   	         ; Restore DS
		JMP PCCMD_Completed
		
PCC_Do_ReadKey:
        CALL BIOS_ReadKey
		MOV byte [PCCR_Param],AH    ;BIOS scan code
		MOV byte [PCCR_Param+1],AL	;ASCII character	
        mov_ds_cs	   	         ; Restore DS
		JMP PCCMD_Completed
		
PCC_Do_SendKey:
        mov_ds_cs	   	         ; Restore DS
		JMP PCCMD_Completed

; BIOS_StringInput
; Input : 
; CX : Number of char
; ES:DI Pointer to a Memory zone, to store the string
; DX : Screen Position
; BL : Text Attribute
; Output : DX:AX Value
		
PCC_Do_ReadString:
        MOV DH,[PCCR_Param]   ; Row
	    MOV DL,[PCCR_Param+1] ; Column
		MOV BL,[PCCR_Param+2]
		XOR CX,CX
		MOV CL,[PCCR_Param+3]
		PUSH CS
		POP ES
		MOV DI,PCCR_Param
        CALL BIOS_StringInput
		JC  PCC_Do_ReadString_ok
        MOV byte [PCCR_Param],0	  ; If cancelled, Clean the string
PCC_Do_ReadString_ok:
        mov_ds_cs	   	          ; Restore DS
		JMP PCCMD_Completed

PCC_Do_SetRAMBlock16:
		LES DI,[PCCR_Param+4]     ; Read DI then ES (Target Ptr)
		MOV CX,[PCCR_Param+8]     ; Nb of Word to write
		MOV AX,[PCCR_Param+10]
		LDS SI,CS:[PCCR_Param]    ; Read SI then DS (Source Ptr)
		REP STOSW
		mov_ds_cs				  ; Restore DS
		JMP PCCMD_Completed

PCC_Do_MEMWR8:
		LES DI,[PCCR_Param]   ; Address
		MOV AL,[PCCR_Param+4] ; Data
		MOV ES:[DI],AL        ; Write the data
		MOV AL,ES:[DI]        ; Read the data	
		MOV [PCCR_Param],AL
		JMP PCCMD_Completed
	
PCC_Do_MEMWR16:
		LES DI,[PCCR_Param]   ; Address
		MOV AX,[PCCR_Param+4] ; Data
		MOV ES:[DI],AX        ; Write the data
		MOV AX,ES:[DI]        ; Read the data
		MOV [PCCR_Param],AX	
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