; Color profile patch for the PicoMEM 2 BIOS (pm2bios.bin)
;
; The PicoMEM 2 BIOS source is not published, so this file rebuilds the
; original ROM with "incbin" and replaces a few instructions with calls to
; new code placed in the free space at the end of the ROM.
; It adds the same feature as the bios_menu.asm change for the PicoMEM 1:
; key 'C' in the setup menu cycles through color profiles.
;
; Supported original BIOS build (BUILD define):
;   0616 : BIOS Build 2026-06-16, from PM2-6-21-26.uf2 (default)
;          It matches the firmware C source in src/ (June 2026)
;
; Build (SRC must be the ORIGINAL BIOS, not an already patched one):
;   nasm -f bin -o pm2bios.bin pm2bios_colors.asm -dSRC='"orig.bin"' -dBUILD=0616
; then fix the checksum (last byte, so that the sum of all bytes is 0).
; The addresses below are only valid for this exact build.

%ifndef SRC
%define SRC "pm2bios_orig.bin"
%endif
%ifndef BUILD
%define BUILD 0616
%endif

BITS 16
ORG 0

PMCFG_ColorPr   EQU 0x4142
; Free RAM, not used by the BIOS (vars end below 0x4C50) or by the Pico
OldInt10h       EQU 0x4F80  ; DD
Color_Bypass    EQU 0x4F84  ; DB
ColorOffset     EQU 0x4F86  ; DW

; Copy the original ROM from the current position up to %1
%macro COPY_TO 1
	incbin SRC, ($-$$), (%1)-($-$$)
%endmacro

; Replace the 3 byte CALL at %1 by a CALL to %2
%macro PATCH_CALL 2
	COPY_TO %1
	CALL %2
%endmacro

; Patched instructions, in increasing address order:
; - CMP AL,4 (Color profile max)               > CMP AL,Color_Prof_Nb
; - MOV byte [PMCFG_ColorPr],2 (Mono, 5 bytes) > CALL New_ForceMono
; - CALL BIOS_PrintMainScreen (menu start)     > CALL New_MenuStart
; - CALL Wait_Key_Menu (menu start)            > CALL New_WaitKeyMenu
; - MOV byte [Menu_Current],0 in PrintMainScreen (5 bytes) > NOP
; - CMP AL,1Bh / JNZ in Wait_Key_Menu (4 bytes) > JMP New_TestKeys
; - CALL BIOS_RestoreRectangle                 > CALL New_RestoreRectangle

%if BUILD = 0616

BIOS_RestoreRectangle EQU 0x0558
BIOS_PrintMainScreen  EQU 0x1883
Wait_Key_Menu         EQU 0x18BA
BIOS_PrintScreenBloc  EQU 0x2462
Menu_Current          EQU 0x4C13
Conf_Changed          EQU 0x4C1C
O_NewCode             EQU 0x3700

	COPY_TO 0x182A
	CMP AL,Color_Prof_Nb
	COPY_TO 0x185A
	CALL New_ForceMono
	NOP
	NOP
	PATCH_CALL 0x186B, New_MenuStart
	PATCH_CALL 0x1874, New_WaitKeyMenu
	COPY_TO 0x18B1
	TIMES 5 NOP
	COPY_TO 0x192E
	JMP New_TestKeys
	NOP
	PATCH_CALL 0x21B7, New_RestoreRectangle
	PATCH_CALL 0x2313, New_RestoreRectangle

%else
%error "Unknown BUILD"
%endif

	COPY_TO O_NewCode

; ********** New code **********

; Same attributes as in bios_menu.asm (R_BK,G_BK,G_B,BL_LB,W_LB,BK_G,BL_G,R_G)
Color_Source DB 0x04, 0x07, 0x17, 0x30, 0x3F, 0x70, 0x71, 0x74
Color_Src_Nb EQU 8

Profile_0    DB 0x04, 0x07, 0x17, 0x30, 0x3F, 0x70, 0x71, 0x74 ; Default colors
Profile_1    DB 0x0F, 0x07, 0x07, 0x70, 0x0F, 0x70, 0x70, 0x0F ; Black, grey and white, no white on grey
Profile_2    DB 0x07, 0x07, 0x07, 0x70, 0x70, 0x70, 0x70, 0x70 ; Monochrome (MDA)

Color_Profiles DW Profile_0,Profile_1,Profile_2
Color_Prof_Nb   EQU 3
Color_Prof_Mono EQU 2

; "C Colors" in the bottom bar
BotKey_Colors DB 32,24,0x3F,1,'C',0
              DB 38,24,0x30,1,'Colors',0
              DB 0xFF,0xFF

; Mono screen: use the MDA profile, unless the user selected another one
New_ForceMono:
	CMP byte CS:[PMCFG_ColorPr],0
	JNE .End
	MOV byte CS:[PMCFG_ColorPr],Color_Prof_Mono
.End:
	RET

; Replaces the call to BIOS_PrintMainScreen when the menu starts
New_MenuStart:
	MOV byte CS:[Menu_Current],0
	CALL Color_HookInt10h
; Draw the whole screen, keep the current menu
New_RedrawMainScreen:
	CALL BIOS_PrintMainScreen
	PUSH DS
	PUSH CS
	POP DS
	MOV SI,BotKey_Colors
	CALL BIOS_PrintScreenBloc
	POP DS
	RET

; Replaces the call to Wait_Key_Menu: remove the hook when the menu ends
New_WaitKeyMenu:
	CALL Wait_Key_Menu
	CALL Color_UnhookInt10h
	RET

; Jumped to from Wait_Key_Menu, in place of the Esc test
New_TestKeys:
	OR AH,AH		; Not an extended key (F9 has the same code as 'C')
	JNZ .TestEsc
	CMP AL,'C'
	JE .CPressed
	CMP AL,'c'
	JNE .TestEsc
.CPressed:
	MOV AL,CS:[PMCFG_ColorPr]
	INC AL
	CMP AL,Color_Prof_Nb
	JB .ProfileOk
	XOR AL,AL
.ProfileOk:
	MOV CS:[PMCFG_ColorPr],AL
	MOV byte CS:[Conf_Changed],1
	CALL Color_SetProfile
	CALL New_RedrawMainScreen
	JMP Wait_Key_Menu
.TestEsc:
	CMP AL,0x1B
	JNE .Continue
	RET				; Esc: quit Wait_Key_Menu
.Continue:
	JMP Wait_Key_Menu

; The saved attributes already have the menu colors: write them as they are
New_RestoreRectangle:
	INC byte CS:[Color_Bypass]
	CALL BIOS_RestoreRectangle
	DEC byte CS:[Color_Bypass]
	RET

; INT 10h hook, installed only while the setup menu is open.
; For "write char/attribute" (AH=09h), replace the attribute in BL with
; the one of the active color profile. Other functions are not changed.
PM_Int10h_Color:
	CMP AH,09h
	JNE .Chain
	CMP byte CS:[Color_Bypass],0
	JNE .Chain
	PUSH BX
	PUSH CX
	PUSH SI
	MOV SI,Color_Source
	MOV CX,Color_Src_Nb
.Search:
	CMP BL,CS:[SI]
	JE .Found
	INC SI
	LOOP .Search
	JMP .Call				; Not in the list: keep the attribute
.Found:
	SUB SI,Color_Source
	ADD SI,CS:[ColorOffset]
	MOV BL,CS:[SI]
.Call:
	POP SI
	POP CX					; CX: Nb of chars for the BIOS
	PUSHF
	CALL FAR [CS:OldInt10h]
	POP BX
	IRET
.Chain:
	JMP FAR [CS:OldInt10h]

; Select the color table of the profile in PMCFG_ColorPr
Color_SetProfile:
	PUSH BX
	XOR BH,BH
	MOV BL,CS:[PMCFG_ColorPr]
	SHL BX,1
	MOV BX,CS:[Color_Profiles+BX]
	MOV CS:[ColorOffset],BX
	POP BX
	RET

Color_HookInt10h:
	PUSH AX
	PUSH ES
	CALL Color_SetProfile
	MOV byte CS:[Color_Bypass],0
	XOR AX,AX
	MOV ES,AX
	CLI
	MOV AX,ES:[10h*4]
	MOV CS:[OldInt10h],AX
	MOV AX,ES:[10h*4+2]
	MOV CS:[OldInt10h+2],AX
	MOV word ES:[10h*4],PM_Int10h_Color
	MOV ES:[10h*4+2],CS
	STI
	POP ES
	POP AX
	RET

Color_UnhookInt10h:
	PUSH AX
	PUSH ES
	XOR AX,AX
	MOV ES,AX
	CLI
	MOV AX,CS:[OldInt10h]
	MOV ES:[10h*4],AX
	MOV AX,CS:[OldInt10h+2]
	MOV ES:[10h*4+2],AX
	STI
	POP ES
	POP AX
	RET

New_Code_End:

; Keep the rest of the ROM free space, last byte is the checksum
	TIMES 16383-($-$$) DB 0xFF
	DB 0			; Checksum, set after the build
