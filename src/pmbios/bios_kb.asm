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
%ifndef BIOSKB
%assign BIOSKB 1

; List of Ascii values

Key_0  EQU 0x30 ;48
Key_1  EQU 0x31 ;49
Key_2  EQU 0x32 ;50
Key_3  EQU 0x33 ;51
Key_9  EQU 0x39 ;57

KeyF1  EQU 59   ;3B
KeyF2  EQU 60   ;3C
KeyF3  EQU 61
KeyF4  EQU 62
KeyF5  EQU 63
KeyF6  EQU 64
KeyF7  EQU 65
KeyF8  EQU 66
KeyF9  EQU 67
KeyF10 EQU 68   ;44

KeyEnter  EQU 13
KeyBack   EQU 8
KeyTab    EQU 9
KeyEspace EQU 32
KeyPlus   EQU 45
KeyMinus  EQU 43

KeyUp     EQU 72
KeyDown   EQU 80
KeyLeft   EQU 75
KeyRight  EQU 77
KeyHome   EQU 71
KeyEnd    EQU 79
KeyPgUp   EQU 73
KeyPgDn   EQU 81
KeyEsc    EQU 27

;INT 16 - KEYBOARD - CHECK FOR KEYSTROKE
;	AH = 01h
;Return: ZF set if no keystroke available
;	ZF clear if keystroke available
;	    AH = BIOS scan code
;	    AL = ASCII character
; Return Z=1 if no Key
BIOS_Keypressed:
    MOV AH,01h
    INT 16h
    RET

;INT 16 - KEYBOARD - GET KEYSTROKE
;	AH = 00h
;Return: AH = BIOS scan code
;	AL = ASCII character
;Notes:	on extended keyboards, this function discards any extended keystrokes,
;	  returning only when a non-extended keystroke is available
;	the BIOS scan code is usually, but not always, the same as the hardware
;	  scan code processed by INT 09.  It is the same for ASCII keystrokes
;	  and most unshifted special keys (F-keys, arrow keys, etc.), but
;	  differs for shifted special keys
BIOS_ReadKey:
    MOV AH,00h
    INT 16h
    OR AL,AL
    JZ .Extended
    XOR AH,AH
    JMP .End
.Extended:
    MOV AL,AH
    MOV AH,1
.End:
    RET

;INT 16 - KEYBOARD - READ KeyBoard Flag
;	AH = 02h
;Return: AH = BIOS keyboard Flag
;		|7|6|5|4|3|2|1|0|  AL or BIOS Data Area 40:17
;		 | | | | | | | `---- right shift key depressed
;		 | | | | | | `----- left shift key depressed
;		 | | | | | `------ CTRL key depressed
;		 | | | | `------- ALT key depressed
;		 | | | `-------- scroll-lock is active
;		 | | `--------- num-lock is active
;		 | `---------- caps-lock is active
;		 `----------- insert is active
BIOS_ReadFlag:
    MOV AH,02h
    INT 16h
    RET

UpCase:
    OR AH,AH
    JNZ .End
    CMP AL,'a'
    JB .End
    CMP AL,'z'
    JA .End
    SUB AL,32
.End:
    RET

BIOS_CleanBuffer:
    CLI
    MOV DX,40h
    MOV ES,DX
    MOV AL,ES:[1Ch]
    MOV ES:[1Ah],AL
    STI    
    RET

;--------------------------------------------------------------------------------------------------
; Wait for a Key during one second
; Output 
; C Flag  : Key was presed, return in AL
; C Clear : Delay expired
;--------------------------------------------------------------------------------------------------
WaitKey_1s:

	STI					; Enable interrupts so timer can run
	XOR AX,AX
	MOV ES,AX
	MOV BX,18
	ADD	BX,ES:[46Ch]	; Add pause ticks to current timer ticks
						;   (0000:046C = 0040:006C)
WaitKey_1s_Loop:
	CALL BIOS_Keypressed
	JNZ	WaitKey_1s_Key	; End pause if key pressed

	MOV	CX,ES:[46Ch]	; Get current ticks
	SUB	CX,BX			; See if pause is up yet
	JC	WaitKey_1s_Loop	; Nope
	CLC

WaitKey_1s_End:
	CLI					; Disable interrupts
	RET

WaitKey_1s_Key:
	CALL BIOS_ReadKey
	STC
	JMP WaitKey_1s_End


; BIOS_StringInput
; Input : 
; CX : Number of char
; ES:DI Pointer to a Memory zone, to store the string
; DX : Screen Position
; BL : Text Attribute
; Output : DX:AX Value

BIOS_StringInput:

    PUSH DI

; Clean the Input Buffer
	PUSH CX
	MOV AL,0FFh
	MOV CL,5
	REP STOSB
	SUB DI,5
	POP CX

	MOV BH,PAGE
	MOV AH,02h   	; GotoXY (DL DH)
	INT 10h
	MOV AX,0x0920 	; Print "CX" space to set the Attribute at the current location
	INT 10h
    
	MOV CH,CL		; Save the number of Char to enter in CH
SI_WaitKey:
	CALL BIOS_Keypressed
	JZ SI_WaitKey
	CALL BIOS_ReadKey

    CMP AL,KeyEnter
	JE SI_Ok

    CMP AL,KeyBack
	JNE SI_NotBack
	CALL Input_Delete
	JMP SI_WaitKey	
SI_NotBack:
	
	CMP AL,KeyEsc
	JNE SI_CheckIfPrintable
	
	POP DI
	JMP Input_Cancelled

SI_CheckIfPrintable:
    CMP AL,32
	JB SI_WaitKey
    CMP AL,126
	JA SI_WaitKey	

    CMP CL,0     ; No more digit to enter > Wait for another key
	JE SI_WaitKey
	DEC CL
    STOSB		; Store the Digit in the Buffer

	MOV AH,0Eh
    INT 10h    	; Write One Char
	JMP SI_WaitKey

SI_Ok:
    POP SI		; Restore the begining of the String

    XOR AX,AX
	STOSB		; Store 0 at the end of the String

	STC
	RET  ; BIOS_WordInput End


BIOS_InputSTR:
; Input : 
; CX : Number of char needed
; ES:DI Pointer to a Memory zone, to store the string
; DX : Screen Position
; BL : Text Attribute
; Output : DX:AX Value

	MOV BH,PAGE
	MOV AH,02h   	; GotoXY (DL DH)
	INT 10h
	MOV AX,0x0920 	; Print "CX" space to set the Attribute at the current location
	INT 10h
    
	MOV CH,CL		; Save the number of Char to enter in CH

STRI_WaitKey:
	CALL WaitKey_Loop

    CMP AL,KeyEnter
	JE STRI_Ok

    CMP AL,KeyBack
	JNE STRI_NotBack
	CALL Input_Delete
	JMP STRI_WaitKey	
STRI_NotBack:
	
	CMP AL,KeyEsc
	JNE STRI_CheckChar
	
	POP DI
	JMP STRI_Cancelled

STRI_CheckChar:

	CMP al,'-'
	JE STRI_CorrectChar

    SUB AL,'0'
	CMP AL,10
	JBE STRI_CorrectChar
	
	SUB AL,'A'-'9'
	CMP AL,'z'-'9'
	JA STRI_WaitKey

STRI_CorrectChar:

    CMP CL,0     ; No more digit to enter > Wait for another key
	JE STRI_WaitKey
	DEC CL
    STOSB		; Store the Digit in the Buffer

    ADD AL,48 	; Go back to the Ascii code
	MOV AH,0Eh
    INT 10h    	; Write One Char
	JMP STRI_WaitKey

STRI_Ok:
    POP SI		; Restore the begining of the Digit String
	
	STC
	RET  ; BIOS_WordInput End

STRI_Cancelled:
	CLC
    RET


; BIOS_WordInput
; Input : 
; CX : Number of Digit needed (Max 5)
; ES:DI Pointer to a Memory zone, to store the string
; DX : Screen Position
; BL : Text Attribute
; Output : DX:AX Value
;          CF=1 : Ok
; Use BIOS_TEXT.ASM

BIOS_WordInput:

    PUSH DI

; Clean the Input Buffer
	PUSH CX
	MOV AL,0FFh
	MOV CL,6
	REP STOSB
	SUB DI,6
	POP CX

	MOV BH,PAGE
	MOV AH,02h   	; GotoXY (DL DH)
	INT 10h
	MOV AX,0x0920 	; Print "CX" space to set the Attribute at the current location
	INT 10h
    
	MOV CH,CL		; Save the number of Char to enter in CH

WI_WaitKey:
	CALL WaitKey_Loop

    CMP AL,KeyEnter
	JE WI_Ok

    CMP AL,KeyBack
	JNE WI_NotBack
	CALL Input_Delete
	JMP WI_WaitKey	
WI_NotBack:
	
	CMP AL,KeyEsc
	JNE WI_CheckIfDigit
	
	POP DI
	JMP Input_Cancelled

WI_CheckIfDigit:
    SUB AL,48
	CMP AL,10
	JA WI_WaitKey

    CMP CL,0     ; No more digit to enter > Wait for another key
	JE WI_WaitKey
	DEC CL
    STOSB		; Store the Digit in the Buffer

    ADD AL,48 	; Go back to the Ascii code
	MOV AH,0Eh
    INT 10h    	; Write One Char
	JMP WI_WaitKey

WI_Ok:

    POP SI		; Restore the begining of the Digit String

	XOR DX,DX
	XOR AX,AX
	MOV BL,10
	SUB CH,CL
	XCHG CH,CL
    XOR CH,CH	; CX : Nb of Digit entered
	CMP CX,0
	JE Input_Cancelled  ; Nothing entered : Like cancel
ConvertDecLoop:
    MUL BX
	MOV DL,[SI]
	ADD AX,DX
	INC SI
    LOOP ConvertDecLoop
ConvertDecEnd:

	STC
	RET  ; BIOS_WordInput End

Input_Cancelled:
    XOR AX,AX
	CLC
    RET

Input_Delete:
    CMP CL,CH	; Don't delete if no entry
	JE Input_Delete_End 
    PUSH AX
    DEC DI ; Previous buffer entry
	INC CL ; Can accept one more entry
    PUSH CX
	
	MOV AH,03h   ; Read Cursor Position (DL DH)
	INT 10h	
	DEC DL       ; Prev Column
	MOV AH,02h   ; GotoXY (DL DH)
	INT 10h
	MOV CX,1
	MOV AX,0x0920 ; Print a space to set the Attribute at the current location
	INT 10h
	POP CX
	POP AX
Input_Delete_End:
	RET

%endif