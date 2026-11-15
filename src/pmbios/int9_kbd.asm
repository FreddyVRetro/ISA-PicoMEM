
;***********************************************************************************************
;*                           IRQ 9h Redirect the Keyboard IRQ   (Hardware IRQ 1)               *
;***********************************************************************************************
; PicoMEM IRQ9 is installed via a BIOS Command

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

        MOV AX,0040h            ; Check BIOS Data Area segment for keyboard buffer pointers
        MOV DS,AX               ; DS=BIOS Variables
        MOV DX,Word DS:[001Ch]	; Read Tail
        MOV BX,Word DS:[001Ah]  ; Read Head
        CMP DX,BX
        JZ  Int9_end            ; Empty buffer > End

		MOV AL,DS:[17h]         ; Read the Keyboard Flag
		AND AL,00000011b        ; Test if Left or Right Shift pressed
		JZ Int9_end
		
        MOV AX,word DS:[BX]             ; Read the Key Code
        
;		PUSH AX
;        MOV BL,15        ; Attribute
;        CALL BIOS_PrintAX_Hex
;		POP AX

        CMP AX,CtrlI
		JE  SCI_Pressed ; Shift+Control+I Pressed

        CMP AX,CtrlF1
		JE  SCI_Pressed ; Shift+Control+F1 Pressed

        CMP AX,CtrlF4
		JE  CSF4_Pressed ; Shift+Control+F4 Pressed

        CMP AX,CtrlF2
		MOV AL,0
		JE  CSF2F3_Pressed ; Shift+Control+F2 Pressed
        
		CMP AX,CtrlF3
		MOV AL,1
		JE  CSF2F3_Pressed ; Shift+Control+F2 Pressed

; Don't add more check after (AL modified)

        JMP Int9_end

CSF4_Pressed:		; Change CD-ROM Image
        MOV word DS:[001Ah],DX       ; Clean the Keyboard buffer
        PUSH SI
        PUSH ES
		PUSH DI
		PUSH CX

		PUSH AX
	    MOV AH,03h  ; GET Cursor position(DH DL)
	    INT 10h
		POP AX
		PUSH DX		; Save Cursor Position

        CALL BM_ChangeCDImage

		POP DX		; Restore Cursor Position
		MOV AH,02	; Set Cursor position
		INT 10h
		JMP PM_Int09h_Skip

CSF2F3_Pressed:		; Swap Floppy (AL = A:0 B:1)
        MOV word DS:[001Ah],DX       ; Clean the Keyboard buffer
        PUSH SI
        PUSH ES
		PUSH DI
		PUSH CX

		PUSH AX
	    MOV AH,03h  ; GET Cursor position(DH DL)
	    INT 10h
		POP AX
		PUSH DX		; Save Cursor Position

        CALL BM_ChangeFDDImage

		CMP byte [Conf_Changed],0
		JE SkipMountFDD
		CALL BIOS_MountFDD
        CALL FDD_UpdateBIOSVar
SkipMountFDD:

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

        PUSH CS
		POP DS
		
        CMP byte CS:[Int13h_Flag],0
		JE I9_Int13_NotActive 
		printstr_w STR_INT13Act
		JMP PM_Int09h_Skip   ; Don't start if there is a disk access in progress.
I9_Int13_NotActive:
		
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