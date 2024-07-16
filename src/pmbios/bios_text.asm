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
%ifndef BIOS_TEXT
%assign BIOS_TEXT 1

%assign PAGE 0

;   	  Normal		Bright
; 000b	0 black			dark gray
; 001b	1 blue			light blue
; 010b	2 green			light green
; 011b	3 cyan			light cyan
; 100b	4 red			light red
; 101b	5 magenta		light magenta
; 110b	6 brown			yellow
; 111b	7 light gray	white

;INT 10 - VIDEO - TELETYPE OUTPUT
;	AH = 0Eh
;	AL = character to write
;	BH = page number
;	BL = foreground color (graphics modes only)
;Return: nothing
;Desc:	display a character on the screen, advancing the cursor and scrolling
;	  the screen as necessary

; BIOS_Printstrc : Print an NULL Terminated String With colours
; BL    : Attribute
; DS:SI : String Position
BIOS_Printstrc:
	MOV BH,PAGE
    MOV CX,1	; Print one char
.PrintStrLoop:
	LODSB
	OR AL,AL
	JZ .PrintStrEnd
    PUSH AX
	MOV AX,0x0920 ; Print a space to set the Attribute at the current location
	INT 10h
	POP AX
	MOV AH,0Eh
    INT 10h       ; Write One Char
	JMP .PrintStrLoop
.PrintStrEnd:	
	RET

; BIOS_Printstr : Print an NULL Terminated String
; DS:SI : String Position
BIOS_Printstr:
	MOV BH,PAGE
.PrintStrLoop:
	LODSB
	OR AL,AL
	JZ .PrintStrEnd
	MOV AH,0Eh
    INT 10h
	JMP .PrintStrLoop
.PrintStrEnd:	
	RET

BIOS_PrintAL_Hex:
    mov cx,2			; 8Bit
	jmp BIOS_Print_Hex

BIOS_PrintAX_Hex3:
    mov cx,3			; 12Bit
	jmp BIOS_Print_Hex
	
BIOS_PrintAX_Hex:
    mov cx,4			; 16Bit
	
BIOS_Print_Hex:
    mov si,bx			; Save the Attribute
	mov bx,cx
LoopSaveHex:
    mov dx,ax
    and dx,000Fh
    shr ax,1
    shr ax,1
    shr ax,1
    shr ax,1
    cmp dl,10
    jb  Hex_09
    add dl,7
Hex_09:
    add dl,30h	
    push dx
    loop LoopSaveHex
    mov cx,bx
    mov bx,si
    jmp DoPrintStackList
	
BIOS_PrintAX_Dec:
    mov si,bx    ; Save the Attribute
    xor cx,cx
LoopSaveDecimal:
    xor dx,dx
    inc cx
    mov bx,10
    div bx
    add dl,'0'
    push dx
    or AX,AX
    jnz LoopSaveDecimal	
    mov bx,si	
    jmp DoPrintStackList	
	
;INT 10 - VIDEO - WRITE CHARACTER AND ATTRIBUTE AT CURSOR POSITION
;	AH = 09h
;	AL = character to display
;	BH = page number (00h to number of pages - 1) (see #00010)
;	    background color in 256-color graphics modes (ET4000)
;	BL = attribute (text mode) or color (graphics mode)
;	    if bit 7 set in <256-color graphics mode, character is XOR'ed
;	      onto screen
;	CX = number of times to write character
; Print a char 
; AL : Chat to print
; BL : Attribute

BIOS_Printchar:
	MOV CX,1  ; Print one Char
BIOS_Print_cx_char:	
	MOV BH,PAGE
	MOV AH,09h
    INT 10h	
	MOV AH,3  ; Read current cursor Position
	INT 10h
    CMP DL,79
    JNE	PrintChar_NextColumn
	INC DH	  ; End of line > Move to the next line
	MOV DL,0ffh
PrintChar_NextColumn:
	INC DL    ; Move to next Column	
	MOV AH,2  ; Set Position
	INT 10h
	RET

; Print one Char at the XY Position, Don't move the cursor
BIOS_Printcharxy:
	CALL BIOS_SetCursorPosDX
	MOV AH,09h
	MOV CX,1  ; Print one Char
    INT 10h	
	RET

DoPrintStackList:
    pop ax              ; Put the Char in, AL
	PUSH CX
    call BIOS_Printchar
	POP CX
    loop DoPrintStackList
    ret


;INT 10 - VIDEO - SET CURSOR POSITION
;	AH = 02h
;	BH = page number
;	    0-3 in modes 2&3
;	    0-7 in modes 0&1
;	    0 in graphics modes
;	DH = row (00h is top)
;	DL = column (00h is left)

; BIOS_SetCursorPos
; AH = row AL = Column
; Use PAGE as Page number

BIOS_SetCursorPos:
	MOV DX,AX
BIOS_SetCursorPosDX:
    PUSH AX			; AL is modified in one 5150 BIOS Version
	MOV BH,PAGE
	MOV AH,02h      ; GotoXY (DL DH)
	INT 10h
	POP AX
	RET

; ! Save to CS:PM_DISKB+2048
BIOS_SaveRectangle:
	MOV BH,PAGE
	PUSH CS
	POP DS	
	MOV AX,CS
	MOV ES,AX
	MOV DI,PM_DISKB+2048
	
	ADD word [RW_DX],2
	ADD word [RW_DY],2
	MOV DX,[RW_XY]
	XOR CX,CX
	MOV CL,[RW_DY]
	
SaveRect_LoopY:
	PUSH CX
	MOV CL,[RW_DX]

SaveRect_LoopX:
	PUSH CX
	MOV AH,02h      ; GotoXY (DL DH)
	INT 10h
	MOV AH,8		; 8 READ ATTRIBUTE/CHARACTER AT CURRENT CURSOR
	INT 10h
	STOSW			; Store Char/Attribute
	POP CX
	INC DL
	LOOP SaveRect_LoopX
	INC DH
	SUB DL,[RW_DX]
	POP CX
	LOOP SaveRect_LoopY

	SUB word [RW_DX],2
	SUB word [RW_DY],2
	RET
	
BIOS_RestoreRectangle:
	MOV BH,PAGE
	PUSH CS
	POP DS
	MOV SI,PM_DISKB+2048
	
	ADD word [RW_DX],2
	ADD word [RW_DY],2
	MOV DX,[RW_XY]
	XOR CX,CX
	MOV CL,[RW_DY]
	
RestRect_LoopY:
	PUSH CX
	MOV CL,[RW_DX]

RestRect_LoopX:
	PUSH CX
	MOV AH,02h      ; GotoXY (DL DH)
	INT 10h
	LODSW
	MOV BL,AH
	MOV AH,9		; 9 WRITE ATTRIBUTE/CHARACTER AT CURRENT CURSOR POSITION
	MOV CX,1
	INT 10h
	POP CX
	INC DL
	LOOP RestRect_LoopX
	INC DH
	SUB DL,[RW_DX]
	POP CX
	LOOP RestRect_LoopY
	
	SUB word [RW_DX],2
	SUB word [RW_DY],2
	RET

;IBM PC BIOS Functions (Don't use other function)

;--- INT 10 -------------------------------------------------------------
; VIDEO_IO								:
;	THESE ROUTINES PROVIDE THE CRT INTERFACE			:
;	THE FOLLOWING FUNCTIONS ARE PROVIDED:				:
;	(AH)=0	SET MODE (AL) CONTAINS MODE VALUE			:
;		(AL)=0 40X25 BW (POWER ON DEFAULT)			:
;		(AL)=1 40X25 COLOR					:
;		(AL)=2 80X25 BW 					:
;		(AL)=3 80X25 COLOR					:
;		GRAPHICS MODES						:
;		(AL)=4 320X200 COLOR					:
;		(AL)=5 320X200 BW					:
;		(AL)=6 640X200 BW					:
;		CRT MODE=7 80X25 B&W CARD (USED INTERNAL TO VIDEO ONLY) :
;		*** NOTE BW MODES OPERATE SAME AS COLOR MODES, BUT	:
;			 COLOR BURST IS NOT ENABLED			:
;	(AH)=1	SET CURSOR TYPE 					:
;		(CH) = BITS 4-0 = START LINE FOR CURSOR 		:
;		       ** HARDWARE WILL ALWAYS CAUSE BLINK		:
;		       ** SETTING BIT 5 OR 6 WILL CAUSE ERRATIC 	:
;			  BLINKING OR NO CURSOR AT ALL			:
;		(CL) = BITS 4-0 = END LINE FOR CURSOR			:
;	(AH)=2	SET CURSOR POSITION					:
;		(DH,DL) = ROW,COLUMN  (0,0) IS UPPER LEFT		:
;		(DH) = PAGE NUMBER (MUST BE 0 FOR GRAPHICS MODES)	:
;	(AH)=3	READ CURSOR POSITION					:
;		(BH) = PAGE NUMBER (MUST BE 0 FOR GRAPHICS MODES)	:
;		ON EXIT (DH,DL) = ROW,COLUMN OF CURRENT CURSOR		:
;			(CH,CL) = CURSOR MODE CURRENTLY SET		:
;	(AH)=4	READ LIGHT PEN POSITION 				:
;		ON EXIT:						:
;		(AH) = 0 -- LIGHT PEN SWITCH NOT DOWN/NOT TRIGGERED	:
;		(AH) = 1 -- VALID LIGHT PEN VALUE IN REGISTERS		:
;			(DH,DL) = ROW, COLUMN OF CHARACTER LP POSN	:
;			(CH) = RASTER LINE (0-199)			:
;			(BX) = PIXEL COLUMN (0-319,639) 		:
;	(AH)=5	SELECT ACTIVE DISPLAY PAGE (VALID ONLY FOR ALPHA MODES) :
;		(AL)=NEW PAGE VAL (0-7 FOR MODES 0&1, 0-3 FOR MODES 2&3):
;	(AH)=6	SCROLL ACTIVE PAGE UP					:
;		(AL) = NUMBER OF LINES, INPUT LINES BLANKED AT BOTTOM	:
;		       OF WINDOW					:
;			AL = 0 MEANS BLANK ENTIRE WINDOW		:
;		(CH,CL) = ROW,COLUMN OF UPPER LEFT CORNER OF SCROLL	:
;		(DH,DL) = ROW,COLUMN OF LOWER RIGHT CORNER OF SCROLL	:
;		(BH) = ATTRIBUTE TO BE USED ON BLANK LINE		:
;	(AH)=7	SCROLL ACTIVE PAGE DOWN 				:
;		(AL) = NUMBER OF LINES, INPUT LINES BLANKED AT TOP	:
;		       OF WINDOW					:
;			AL = 0 MEANS BLANK ENTIRE WINDOW		:
;		(CH,CL) = ROW,COLUMN OF UPPER LEFT CORNER OF SCROLL	:
;		(DH,DL) = ROW,COLUMN OF LOWER RIGHT CORNER OF SCROLL	:
;		(BH) = ATTRIBUTE TO BE USED ON BLANK LINE		:
;									:
;	CHARACTER HANDLING ROUTINES					:
;									:
;	(AH) = 8 READ ATTRIBUTE/CHARACTER AT CURRENT CURSOR POSITION	:
;		(BH) = DISPLAY PAGE (VALID FOR ALPHA MODES ONLY)	:
;		ON EXIT:						:
;		(AL) = CHAR READ					:
;		(AH) = ATTRIBUTE OF CHARACTER READ (ALPHA MODES ONLY)	:
;	(AH) = 9 WRITE ATTRIBUTE/CHARACTER AT CURRENT CURSOR POSITION	:
;		(BH) = DISPLAY PAGE (VALID FOR ALPHA MODES ONLY)	:
;		(CX) = COUNT OF CHARACTERS TO WRITE			:
;		(AL) = CHAR TO WRITE					:
;		(BL) = ATTRIBUTE OF CHARACTER (ALPHA)/COLOR OF CHAR	:
;		       (GRAPHICS)					:
;			SEE NOTE ON WRITE DOT FOR BIT 7 OF BL = 1.	:
;	(AH) = 10 WRITE CHARACTER ONLY AT CURRENT CURSOR POSITION	:
;		(BH) = DISPLAY PAGE (VALID FOR ALPHA MODES ONLY)	:
;		(CX) = COUNT OF CHARACTERS TO WRITE			:
;		(AL) = CHAR TO WRITE					:
;	FOR READ/WRITE CHARACTER INTERFACE WHILE IN GRAPHICS MODE, THE	:
;		CHARACTERS ARE FORMED FROM A CHARACTER GENERATOR IMAGE	:
;		MAINTAINED IN THE SYSTEM ROM.  ONLY THE 1ST 128 CHARS	:
;		ARE CONTAINED THERE.  TO READ/WRITE THE SECOND 128	:
;		CHARS. THE USER MUST INITIALIZE THE POINTER AT		:
;		INTERRUPT 1FH (LOCATION 0007CH) TO POINT TO THE 1K BYTE :
;		TABLE CONTAINING THE CODE POINTS FOR THE SECOND 	:
;		128 CHARS (128-255).					:
;	FOR WRITE CHARACTER INTERFACE IN GRAPHICS MODE, THE REPLICATION :
;		FACTOR CONTAINED IN (CX) ON ENTRY WILL PRODUCE VALID	:
;		RESULTS ONLY FOR CHARACTERS CONTAINED ON THE SAME ROW.	:
;		CONTINUATION TO SUCCEEDING LINES WILL NOT PRODUCE	:
;		CORRECTLY.						:
;									:
;	GRAPHICS INTERFACE						:
;	(AH) = 11 SET COLOR PALETTE					:
;		(BH) = PALETTE COLOR ID BEING SET (0-127)		:
;		(BL) = COLOR VALUE TO BE USED WITH THAT COLOR ID	:
;		   NOTE: FOR THE CURRENT COLOR CARD, THIS ENTRY POINT	:
;			 HAS MEANING ONLY FOR 320X200 GRAPHICS. 	:
;			COLOR ID = 0 SELECTS THE BACKGROUND COLOR (0-15):
;			COLOR ID = 1 SELECTS THE PALETTE TO BE USED:	:
;				0 = GREEN(1)/RED(2)/YELLOW(3)		:
;				1 = CYAN(1)/MAGENTA(2)/WHITE(3) 	:
;			IN 40X25 OR 80X25 ALPHA MODES, THE VALUE SET	:
;				FOR PALETTE COLOR 0 INDICATES THE	:
;				BORDER COLOR TO BE USED (VALUES 0-31,	:
;				WHERE 16-31 SELECT THE HIGH INTENSITY	:
;				BACKGROUND SET. 			:
;	(AH) = 12 WRITE DOT						:
;		(DX) = ROW NUMBER					:
;		(CX) = COLUMN NUMBER					:
;		(AL) = COLOR VALUE					:
;			IF BIT 7 OF AL = 1, THEN THE COLOR VALUE IS	:
;			EXCLUSIVE OR'D WITH THE CURRENT CONTENTS OF     :
;			THE DOT 					:
;	(AH) = 13 READ DOT						:
;		(DX) = ROW NUMBER					:
;		(CX) = COLUMN NUMBER					:
;		(AL) RETURNS THE DOT READ				:
;									:
; ASCII TELETYPE ROUTINE FOR OUTPUT					:
;									:
;	(AH) = 14 WRITE TELETYPE TO ACTIVE PAGE 			:
;		(AL) = CHAR TO WRITE					:
;		(BL) = FOREGROUND COLOR IN GRAPHICS MODE		:
;		NOTE -- SCREEN WIDTH IS CONTROLLED BY PREVIOUS MODE SET :
;									:
;	(AH) = 15 CURRENT VIDEO STATE					:
;		RETURNS THE CURRENT VIDEO STATE 			:
;		(AL) = MODE CURRENTLY SET (SEE AH=0 FOR EXPLANATION)	:
;		(AH) = NUMBER OF CHARACTER COLUMNS ON SCREEN		:
;		(BH) = CURRENT ACTIVE DISPLAY PAGE			:
;									:
;	CS,SS,DS,ES,BX,CX,DX PRESERVED DURING CALL			:
;	ALL OTHERS DESTROYED						:
;------------------------------------------------------------------------

%endif