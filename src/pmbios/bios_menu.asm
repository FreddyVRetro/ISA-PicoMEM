%ifndef BIOS_MENU
%assign BIOS_MENU 1

STR_MB DB 'MB',0
STR_KB2 DB 'kB',0
STR_InvalidSize DB 'Error',0
STR_Off4 DB 'Off ',0
STR_Inactive DB '----',0
STR_Off DB 'Off',0
STR_On DB 'On ',0

Color_Profiles DW Profile_0,Profile_1,Profile_2

Profile_0 DB  4,7,23,48,63,112,113,0
Profile_1 DB  4,7,23,48,63,112,112,0
Profile_2 DB  4,7,23,48,63,112,113,0

I_R_BK  EQU 0
I_G_BK  EQU 1
I_G_B   EQU 2  ;Grey on Blue
I_BL_LB EQU 3  ;Blue on Light Blue
I_W_LB  EQU 4  ;White on Light Blue 
I_BK_G  EQU 5  ;Black on Grey
I_BL_G  EQU 6  ;Blue on Grey  Not working on most of the Mono screens
I_R_G   EQU 7  ;Red on Grey

R_BK  EQU 4    ; Red on Black  (not used in menu)
G_BK  EQU 7    ; Grey on Black (not used in menu)
G_B   EQU 23   ;Grey on Blue
BL_LB EQU 48   ;Blue on Light Blue
W_LB  EQU 63   ;White on Light Blue 
BK_G  EQU 0x70 ;Black on Grey
BL_G  EQU 0x71 ;Blue on Grey        Not working on most of the Mono screens
;BL_G  EQU 0xF1 ;Blue on Grey Blink 
R_G   EQU 0x74  ;Red on Grey

; Blue is not visible on some mono display

; Display Data Format : Screen Offset, Attribute,0 :Char 1: String, NbRepet (If Char) , Char or String

BIOS_MainScreen_TopBott DB 0,0,BL_LB,0,80,' '
  DB 0,1,23,0,80,' '
  DB 0,23,BL_LB,0,80*2,' '
  DB 0xFF,0xFF

BIOS_TopBar DB 1,0,BL_LB,1,'V0.4',0
            DB 30,0,BL_LB,1,'PicoMEM Setup Utility',0
			DB 69,0,BL_LB,1,'By FreddyV',0,0xFF,0xFF

BIOS_TopMenu:
BIOS_Menu0   DB 3,1,G_B,1,'Main ',0
BIOS_Menu1   DB 11,1,G_B,1,'Memory ',0
BIOS_Menu2   DB 21,1,G_B,1,'Disk ',0
BIOS_Menu3   DB 29,1,G_B,1,'Audio ',0
BIOS_Menu4   DB 38,1,G_B,1,'Other ',0
			 DB 0xFF,0xFF

BIOS_BotKey  DB 2,23,W_LB,1,'Esc',0
             DB 14,23,W_LB,1,25,24,0
			 DB 32,23,W_LB,1,'Enter',0
			 DB 14,24,W_LB,1,27,26,0

			 DB 0xFF,0xFF

BIOS_BotText DB 7,23,BL_LB,1,'Exit',0
             DB 18,23,BL_LB,1,'Select Item',0
             DB 38,23,BL_LB,1,'Change Value',0
             DB 18,24,BL_LB,1,'Select Menu',0
;             DB 39,24,BL_LB,1,'Select Sub-Menu',0		
			 DB 0xFF,0xFF			 

;*****************************************
;**** Sub Menu 0 Definitions (Main)   ****
;*****************************************
BIOS_SubMenu0 DB 3,4,BK_G,1,'BIOS Build: ',__DATE__,' ',__TIME__,0
;			  DB 3,6,BL_G,1,'System CPU:',0
              DB 3,6,BL_G,1,'System Memory:',0
              DB 22,6,BL_G,1,'- Floppy Drives:',0			  
              DB 41,6,BL_G,1,'- Hard Disks:',0
              DB 3,7,BL_G,1,'PicoMEM IRQ:',0
;              DB 3,10,BL_G,1,'Force Reboot:',0

              DB 3,9,BK_G,1,'- PicoMEM Base Settings -',0
			  
              DB 3,11,BL_G,1,'Fast Boot: ',0
			  DB 3,21,BL_G,1,'PicoMEM Firmware and DOC: https://github.com/FreddyVRetro/ISA-PicoMEM',0
              DB 0xFF,0xFF

BIOS_Sub0_Braket DB 15,11,1,3   ; Fast Boot
				 DB 0xFF,0xFF

BIOS_Sub0_Coord  DB 14,11	

BIOS_SubMenu0_Nb EQU 0  ; Nb of Sub menu we can select -1

MemSize_XY       EQU 256*6+18
PMFDD_XY         EQU 256*6+39
PMHDD_XY         EQU 256*6+55
PMIRQ_XY         EQU 256*7+16

FastBoot_XY      EQU 256*11+16


; Selection menu code and Data changed
BIOS_SelectSub0 DW SEL_OnOff_Main
BIOS_Sub0Data   DW PMCFG_FastBoot

;*****************************************
;**** Sub Menu 1 Definitions (Memory) ****
;*****************************************
BIOS_SubMenu1 DB 3,8,BL_G,1,'EMS  Port:',0
              DB 3,9,BL_G,1,'EMS  Addr:',0
              DB 3,10,BL_G,1,'PM RAM Extension :',0
              DB 3,11,BL_G,1,'PSRAM  Extension :',0
			  
              DB 3,13,BL_G,1,'Ignore Video Segment:',0
              DB 3,14,BL_G,1,'Maximize Conv MEM   :',0
			  
              DB 3,16,BL_G,1,'ROM0 File:',0
              DB 3,17,BL_G,1,'ROM1 File:',0

			  DB 45,16,BK_G,1,'- Not Implemented',0
			  
              DB 3,19,BK_G,1,'The memory map shows one character per 16kB :',0
              DB 3,20,BL_G,1,'Computer Memory - S:System V:Video R:ROM',0
              DB 3,21,BL_G,1,'PicoMEM  Memory - B:BIOS   M:RAM   P:PSRAM  E:EMS  0/1:ROM Bank',0
              DB 0xFF,0xFF

BIOS_Sub1_Braket DB 15,8,2,4    ; EMS Port /Addr
                 DB 23,10,2,3   ; PM / PSRAM 
				 DB 26,13,2,3   ; Ignore Video/ Maximize
                 DB 15,16,1,19  ; ROM0 File
                 DB 15,17,1,19  ; ROM1 File			 
				 DB 0xFF,0xFF

BIOS_Sub1_Coord DB 14,8
                DB 14,9
                DB 22,10
				DB 22,11
				DB 25,13
				DB 25,14				
				DB 14,16 ; ROM0
				DB 14,17

BIOS_SubMenu1_Nb EQU 7  ; Nb of Sub menu we can select -1

ROM_Addr_List DW 6,0xC000,0xC800,0xD000,0xD800,0xE000,0xE800
EMS_Addr_List DW 2,0xD000,0xE000
EMS_Port_List DW 5,0,0x268,0x288,0x298,0x2A8

; Sub Menu values text positions

ROM0_NameXY  EQU 256*16+16
ROM1_NameXY  EQU 256*17+16

MEMAList_XY     EQU 256*8+46  ; ROM/EMS
MEMAListRect_XY EQU 256*7+44
EMSPort_XY		EQU 256*8+16
EMSAddr_XY      EQU 256*9+16
PMRAM_XY        EQU 256*10+24
PSRAM_XY        EQU 256*11+24
IgnoreVideo_XY  EQU 256*13+27
MaxConv_XY		EQU 256*14+27
ROMList_XY      EQU 256*8+46
ROMListRect_XY  EQU 256*7+44
ROM0_Menuindex  EQU 6

BIOS_SelectSub1 DW SEL_EMSPort,SEL_EMSAddr,SEL_OnOff_Mem,SEL_OnOff_Mem,SEL_OnOff_Mem,SEL_OnOff_Mem,SEL_ROMImages,SEL_ROMImages
BIOS_Sub1Data   DW 0,SEL_EMSAddr,PMCFG_PMRAM_Ext,PMCFG_PSRAM_Ext,PMCFG_IgnoreAB,PMCFG_Max_Conv,PMCFG_ROM0Size,PMCFG_ROM1Size

T_Y	EQU 3

MEM_RectangleXY  EQU 256*T_Y+6      ; 256*Y+X
MEM_SegXY        EQU 256*(T_Y+1)+8  ; 256*Y+X
MEM_DisplayXY    EQU 256*(T_Y+2)+7  ; 256*Y+X

BIOS_Sub1_MemtableText DB 23,T_Y,BL_G,1,'Conventional',0
              DB 50,T_Y,BL_G,1,'Video',0
              DB 60,T_Y,BL_G,1,'ROM/EMS',0
              DB 0xFF,0xFF

;*****************************************
; **** Sub Menu 2 Definitions (Disk)  ****
;*****************************************
BIOS_SubMenu2 DB 3,4,BL_G,1,'Floppy Drive A:',0
              DB 3,5,BL_G,1,'Floppy Drive B:',0
			  DB 3,7,BL_G,1,'HD Drive 0:',0
			  DB 3,8,BL_G,1,'HD Drive 1:',0
			  DB 3,9,BL_G,1,'HD Drive 2:',0
			  DB 3,10,BL_G,1,'HD Drive 3:',0

			  DB 3,12,BL_G,1,'PicoMEM Boot Code:',0

			  DB 3,15,BL_G,1,'New Disk Image:',0
			  DB 20,15,BK_G,1,'Select',0

			  DB 45,12,BK_G,1,'- Set to off for XTIDE/Disk BIOS',0
			  
			  DB 3,20,BK_G,1,'Store the floppy and disk images under the FLOPPY\ and HDD\ folders',0
			  DB 3,21,BK_G,1,'Use image filenames with <13 characters and the extension .img',0

			  DB 0xFF,0xFF

BIOS_uSDError DB 20,18,R_G,1,'Warning: SD not present or error',0
			  DB 0xFF,0xFF

BOOT_HDD_List DB 4,0,1,2,3

BIOS_Sub2_Braket DB 20,4,2,20
                 DB 20,7,4,20
				 DB 23,12,1,3			 
				 DB 0xFF,0xFF

BIOS_Sub2_Coord  DB 19,4
                 DB 19,5
				 DB 19,7
				 DB 19,8
				 DB 19,9	
				 DB 19,10
				 DB 22,12
				 DB 19,15

BIOS_SubMenu2_Nb EQU 7  ; Nb of Sub menu we can select -1

; Sub Menu values text positions
FDD0_NameXY    EQU 256*4+21  ; 256*Y+X
HDD0_NameXY    EQU 256*7+21

FDDList_XY     EQU 256*4+46
FDDListRect_XY EQU 256*3+44
HDDList_XY     EQU 256*4+46
HDDListRect_XY EQU 256*3+44
PMBOOT_XY	   EQU 256*12+24
BOOT_HDD_List_Sel_XY EQU 256*13+30

BIOS_SelectSub2 DW SEL_FDDImages ,SEL_FDDImages ,SEL_HDDImages ,SEL_HDDImages ,SEL_HDDImages ,SEL_HDDImages
                DW SEL_OnOff_Disk,Do_NewDiskImage
BIOS_Sub2Data   DW PMCFG_FDD0Size,PMCFG_FDD1Size,PMCFG_HDD0Size,PMCFG_HDD1Size,PMCFG_HDD2Size,PMCFG_HDD3Size
                DW PMCFG_PMBOOT,0
;				PMCFG_DFWrite,PMCFG_DL1Cache


;*********************************************
;**** Sub Menu 3 Definitions (Audio)   ****
;*********************************************
BIOS_SubMenu3 DB 2,4,BK_G,1,'Global Audio',0
			  DB 3,6,BL_G,1,'Output:',0

			  DB 2,8,BK_G,1,'Sound Cards',0
              DB 3,10,BL_G,1,'Adlib:',0


;			  DB 40,5,BK_G,1,'- XTIDE Default port is 300h',0
;			  DB 40,10,BK_G,1,'- PC Joystick is on Port 201h',0			  
;			  DB 40,12,BK_G,1,'- SD/RAM Speed not implemented',0

              DB 0xFF,0xFF

BIOS_SubMenu3_Nb EQU 1  ; Nb of Sub menu we can select -1

BIOS_Sub3_Braket DB 12,6,1,4
				 DB 12,10,1,3
				 DB 0xFF,0xFF

BIOS_Sub3_Coord  DB 11,6
                 DB 11,10


Enable_Output_XY  EQU 256*6+13
Enable_Adlib_XY   EQU 256*10+13

BIOS_SelectSub3 DW SEL_OnOff_Audio,SEL_OnOff_Audio
BIOS_Sub3Data   DW PMCFG_AudioOut,PMCFG_Adlib


;*********************************************
;**** Sub Menu 4 Definitions (Other)   ****
;*********************************************
BIOS_SubMenu4 DB 2,4,BK_G,1,'Wifi: ',0
              DB 2,6,BK_G,1,'NE2000 (network)',0
			  DB 3,7,BL_G,1,'Port :',0              
			  DB 3,8,BL_G,1,'IRQ  :',0

			  DB 2,10,BK_G,1,'Other',0
              DB 3,11,BL_G,1,'USB Host :',0
              DB 3,12,BL_G,1,'USB Joystick :',0
			  
              DB 3,14,BL_G,1,'SD  Speed (MHz) :',0
              DB 3,15,BL_G,1,'RAM Speed (MHz) :',0

			  DB 40,7,BK_G,1,'- XTIDE Default port is 300h',0
			  DB 40,12,BK_G,1,'- PC Joystick is on Port 201h',0			  
			  DB 40,14,BK_G,1,'- SD/RAM Speed not implemented',0

              DB 0xFF,0xFF

;PORT 0300-031F - NE2000 compatible Ethernet adapters
; Range:	may be placed at 0300h, 0320h, 0340h, or 0360h
;PORT 0320-0323 - XT HDC 1   (Hard Disk Controller)
;PORT 0330-0331 - MPU-401 MIDI UART
; Range: alternate address at PORT 0300h or PORT 0330h, occasionally at PORT 0320h
;PORT 0340-0357 - RTC (1st Real Time Clock for XT)
;Range:	alternate at 0240-0257
;PORT 0340-034F - Gravis Ultra Sound by Advanced Gravis
;Range: The I/O address range is dipswitch selectable from:
;      0200-020F and 0300-030F
;	   0210-021F and 0310-031F
;	   0220-022F and 0320-032F
;	   0230-023F and 0330-033F
;	   0240-024F and 0340-034F
;	   0250-025F and 0350-035F
;	   0260-026F and 0360-036F
;	   0270-027F and 0370-037F

; ! NE2000 Port range is 32 address !
Wifi_IRQList    DB 3,3,5,7;  ;9,10
NE2000_PortList DW 6,0,0x300,0x320,0x340,0x360,0x380

SD_Speed_List  DB 3,12,24,36  ; ! In Decimal
RAM_Speed_List DB 4,80,100,120,130

BIOS_SubMenu4_Nb EQU 5  ; Nb of Sub menu we can select -1

BIOS_Sub4_Braket DB 11,7,1,4
                 DB 11,8,1,3
				 DB 15,11,1,3
				 DB 19,12,1,3				 
				 DB 22,14,1,3
				 DB 22,15,1,3
				 DB 0xFF,0xFF

BIOS_Sub4_Coord  DB 10,7
                 DB 10,8
                 DB 14,11
                 DB 18,12				 
                 DB 21,14
				 DB 21,15

WifiInfos_XY      EQU 256*4+8
ne2k_Port_XY      EQU 256*7+12
ne2k_Port_Sel_XY  EQU 256*4+30
ne2k_IRQ_XY       EQU 256*8+13
Wifi_IRQ_Sel_XY   EQU 256*4+30
EnableUSB_XY      EQU 256*11+16
EnableJoy_XY      EQU 256*12+20 
SD_Speed_XY       EQU 256*14+23
SD_Speed_rect_XY  EQU 256*12+30
RAM_Speed_XY      EQU 256*15+23
RAM_Speed_rect_XY EQU 256*12+30

BIOS_SelectSub4 DW SEL_NE2000_Port,SEL_Wifi_IRQ,SEL_OnOff_Sub4,SEL_OnOff_Sub4,SEL_SD_Speed,SEL_RAM_Speed ; SEL_RAM_Speed
BIOS_Sub4Data   DW 0,0,PMCFG_EnableUSB,PMCFG_EnableJOY,PMCFG_SD_Speed,PMCFG_RAM_Speed


Menu_Max EQU 4
MenuTopList    DW BIOS_Menu0,BIOS_Menu1,BIOS_Menu2,BIOS_Menu3,BIOS_Menu4
SubMenuDisplay DW Display_Sub0,Display_Sub1,Display_Sub2,Display_Sub3,Display_Sub4

D_Any   EQU 0
S_YesNo Equ 1

DT_Any   DB 3,0
DT_YesNo DB 'Y/N',0

; Dialog Box Definition : 

D_Exit_Save   DW 256*12+30
              DB 0

D_EMS_NoSeg   DW 256*14+30
;             DB 20,3,DT_Any,'Can''t Add EMS: Segment D and E Not free',0

D_EMS_NoPSRAM DW 256*14+30
;             DB 20,3,DT_Any,'Can''t Add EMS: Memory Failure',0

; Get the real attribute based on a color "Code"
 ; Input  : BL, "Color" needed
; Output : Attribute
GetAttribute:
	PUSH BX
	PUSH AX
	MOV AL,BH
	XOR BH,BH
	SHL BX,1
	ADD BX,[ColorOffset]
	MOV BL,[BX]
	MOV BH,AL
	POP AX
	POP BX
	RET

; Start the BIOS Menu, then restore the video mode at the end
Do_BIOS_Menu:

	BIOSPOST 0x30		; 30h Start the Menu

    MOV AL,[PMCFG_ColorPr]
	CMP AL,4
	JB ColorProfileOk
	MOV AL,0
    MOV byte [PMCFG_ColorPr],0
ColorProfileOk:	

    MOV AH,0Fh  ; Get Video Mode
	INT 10h
;	PUSH AX		; Save Video Mode

	CALL BIOS_PrintAL_Hex

JMP Skipnew

;00  40x25 B/W text
;01  40x25 color text
;02  80x25 shades of gray text
;03  80x25 color text
;04  320x200x4
;05  320x200x4
;06  640x200x2
;07  80x25 Monochrome text
	
	CMP AL,07
	JE MODE_8025M
	CMP AL,03
	JE MODE_8025C
	
	CMP AL,0
	JNE MODE_SET8025C
	; Is 40x25 16Gray Go to 80x25 16Gray

	MOV AL,2
	XOR AH,AH
	INT 10h     ; Set 80x25 shades of gray text
	JMP MODE_8025C

MODE_SET8025C:
	MOV AL,3
	XOR AH,AH
	INT 10h     ; Set 80x25 color text
	JMP MODE_8025C
	
MODE_8025M:
; Video mode is 80x25 Mono (MDA)
	MOV byte [PMCFG_ColorPr],2	; Force MDA "Color" Profile
	
MODE_8025C:
; Video mode is 80x25 Mono (MDA)

;Set the Offset of the colors/Attribute table
	XOR AH,AH
	SHL AL,0
	MOV BX,AX
	MOV AX,[Color_Profiles+BX]
	MOV [ColorOffset],AX

Skipnew:

    MOV byte CS:[Conf_Changed],0			; Config is not changed

	BIOSPOST 0x31
    CALL BIOS_PrintMainScreen
	
	BIOSPOST 0x32	
	CALL Wait_Key_Menu

; clear Screen / Restore Video Mode 

;	POP AX		; Restore Video Mode
	MOV AL,3
	XOR AH,AH
	INT 10h     ; Set the same Video Mode

	BIOSPOST 0x3F
	RET ; Do_BIOS_Menu

; BIOS Menu code
BIOS_PrintMainScreen:
    MOV SI,BIOS_MainScreen_TopBott
    CALL BIOS_PrintScreenBloc

	MOV DX,256*2+0  ; X0 Y2
	MOV word CS:[RW_XY],DX
    MOV byte CS:[RW_DX],78
    MOV byte CS:[RW_DY],19
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_PrintRectangle

    MOV SI,BIOS_TopBar
    CALL BIOS_PrintScreenBloc
    CALL BIOS_PrintScreenBloc
    CALL BIOS_PrintScreenBloc
    CALL BIOS_PrintScreenBloc

    MOV byte [Menu_Current],0
	CALL Display_Menu

	RET ; BIOS_PrintMainScreen End
	
Wait_Key_Menu:
	CALL BIOS_Keypressed
	JZ Wait_Key_Menu
	
	MOV BL,7
	CALL BIOS_ReadKey

; Keys for the navigation	
	CMP AL,KeyRight
	JNE M_TestKeyLeft
	JMP Do_NextMenu
M_TestKeyLeft:	
	CMP AL,KeyLeft
	JNE M_TestKeyUp
	JMP Do_PrevMenu
M_TestKeyUp:
	CMP AL,KeyUp
	JNE M_TestKeyDown
	JMP Do_PrevSubMenu
M_TestKeyDown:
	CMP AL,KeyDown
	JNE M_TestKeyPgUp
	JMP Do_NextSubMenu
M_TestKeyPgUp:
	CMP AL,KeyPgUp	
	JNE M_TestKeyPgDn
	JMP Do_FirstSubMenu
M_TestKeyPgDn:
	CMP AL,KeyPgDn
	JNE M_TestKeyEnter	
	JMP Do_LastSubMenu

; Keys to start an Action
M_TestKeyEnter:
    CMP AL,KeyEnter
	JNE M_TestKeyPlus
	JMP M_DoSubMenuAction
M_TestKeyPlus:
    CMP AL,KeyPlus
	JNE M_TestKeyMinus
	JMP M_DoSubMenuAction   
M_TestKeyMinus:
    CMP AL,KeyMinus
	JNE M_TestKey3
	JMP M_DoSubMenuAction  

	
M_DoSubMenuAction:
    MOV SI,Word [SubMenu_Select]
	XOR BH,BH
	MOV BL,Byte [SubMenu_Current]
	SHL BX,1	
	CALL [SI+BX]
    JMP Wait_Key_Menu
	
M_TestKey3:

; Add Here

	CMP AL,KeyEsc
	JNE TestKeyEnd
    RET  ; Quit
TestKeyEnd:

; DEBUG : Print Key    
;	XOR AH,AH
;	CALL BIOS_PrintAX_Dec

	JMP Wait_Key_Menu
	RET

Do_ChangeMenu_Skip:
   JMP Wait_Key_Menu
   
Do_NextMenu:
    CMP byte [Menu_Current],Menu_Max
	JE Do_ChangeMenu_Skip
    CALL Clean_Menu
	INC byte [Menu_Current]
    JMP Do_ChangeMenu
Do_PrevMenu:
    CMP byte [Menu_Current],0
	JE Do_ChangeMenu_Skip
    CALL Clean_Menu	
	DEC byte [Menu_Current]
;! Don't add code here !
Do_ChangeMenu:
    CALL Display_Menu
    JMP Wait_Key_Menu
; Do_NextMenu and Do_PrevMenu End

Do_FirstSubMenu:
    CALL Clean_SubMenuCursor
    MOV Byte [SubMenu_Current],0
	JMP Do_ChangeSubMenu
	
Do_LastSubMenu:
    CALL Clean_SubMenuCursor
    MOV AL,Byte [SubMenu_Nb]	
    MOV Byte [SubMenu_Current],AL
	JMP Do_ChangeSubMenu
	
Do_PrevSubMenu:
    CMP Byte [SubMenu_Current],0
	JE Do_ChangeMenu_Skip
    CALL Clean_SubMenuCursor
    DEC Byte [SubMenu_Current]
	JMP Do_ChangeSubMenu

Do_NextSubMenu:
    MOV AL,Byte [SubMenu_Nb]
    CMP Byte [SubMenu_Current],AL
    JB Do_NextSubMenuOk
    JMP Wait_Key_Menu   
Do_NextSubMenuOk:
    CALL Clean_SubMenuCursor
    INC Byte [SubMenu_Current]
;! Don't add code here !
Do_ChangeSubMenu:
	CALL Display_SubMenuCursor
    JMP Wait_Key_Menu
; Do_FirstSubMenu, Do_LastSubMenu, Do_PrevSubMenu and Do_NextSubMenu End

; * Menu and Sub Menu Display

; Display_Menu : Display the current Menu (Top Text and Sub Menu)
; Change the selected menu backgroud color to Blue
Display_Menu:
; Change the Top Menu background color
	MOV BL,byte [Menu_Current]
	XOR BH,BH
	SHL BX,1
    PUSH BX
	MOV SI,word [MenuTopList+BX]
	MOV BL,BL_G
	CALL BIOS_PrintTextCol
	POP BX

;Display the sub Menu text	
	CALL [SubMenuDisplay+BX]
	CALL Display_SubMenuCursor
	
	RET ; Display_Menu End

; Clean_Menu : Clean the current Menu  (Top Text and Sub Menu)
; Change the selected menu backgroud color to Grey	
Clean_Menu:
; Change the Top Menu background color
	MOV BL,byte [Menu_Current]
	XOR BH,BH
	SHL BX,1
	MOV SI,word [MenuTopList+BX]
	MOV BL,G_B ; Attribute
	CALL BIOS_PrintTextCol

    MOV DX,256*3+1
    MOV AX,76
	MOV CX,19
	CALL Clean_Rectangle

	RET	; Clean_Menu End

; Display/Clean the Sub menu selection arrow
Display_SubMenuCursor:
    MOV AL,'>'
	JMP WriteChar_Display_SubMenuCursor

Clean_SubMenuCursor:
    MOV AL,' '

WriteChar_Display_SubMenuCursor:
    PUSH AX
	MOV SI,Word [SubMenu_Coord]
	XOR BH,BH
	MOV BL,byte [SubMenu_Current]
	SHL BX,1    ; Each Entry is 2 Bytes long
	ADD SI,BX
	LODSW
	MOV DX,AX
	POP AX
	MOV BL,BL_G
	CALL BIOS_Printcharxy
	
	RET ;Display_SubMenuCursor and Clean_SubMenuCursor End

; ******* Sub menu 0 Display (Main) *********
Display_Sub0:
    MOV SI,BIOS_SubMenu0
    CALL BIOS_PrintScreenBloc

    MOV SI,BIOS_Sub0_Braket
	MOV BL,BK_G ; Black on Grey
    CALL Draw_Brackets

; Print the Memory Size
    MOV DX,MemSize_XY ;256*5+20 ; Y*256+X
	CALL BIOS_SetCursorPosDX
	MOV AX,CS:[PC_MEM]
	MOV BL,BL_G
	CALL BIOS_PrintAX_Dec

; Print the detected PicoMEM IRQ Number
    MOV DX,PMIRQ_XY   ;256*5+20 ; Y*256+X
	CALL BIOS_SetCursorPosDX
	XOR AX,AX
	MOV AL,CS:[BV_IRQ]
	MOV BL,BL_G
	CALL BIOS_PrintAX_Dec

    MOV DX,PMFDD_XY   ;256*5+20 ; Y*256+X
	CALL BIOS_SetCursorPosDX
	XOR AX,AX
	MOV AL,CS:[PC_FloppyNB]
	MOV BL,BL_G
	CALL BIOS_PrintAX_Dec
	
    MOV DX,PMHDD_XY   ;256*5+20 ; Y*256+X
	CALL BIOS_SetCursorPosDX
	XOR AX,AX
	MOV AL,CS:[PC_DiskNB]
	MOV BL,BL_G
	CALL BIOS_PrintAX_Dec
	
	CALL Disp_FastBootOnOff
    
	CALL Disp_Sub0_Values	; Wifi IRQ

; Initialize the Sub Menu variables
	MOV Byte [SubMenu_Current],0
	MOV Byte [SubMenu_Nb],BIOS_SubMenu0_Nb
	MOV Word [SubMenu_Coord],BIOS_Sub0_Coord
	MOV Word [SubMenu_Select],BIOS_SelectSub0	
	MOV Word [SubMenu_Data],BIOS_Sub0Data	
	
	RET ;Display_Sub0 End
	
Disp_FastBootOnOff:
	MOV DX,FastBoot_XY
	MOV AL,CS:[PMCFG_FastBoot]
	CALL DISPLAY_OnOff_xy

	RET ; Disp_FastBootOnOff End	

Disp_Sub0_Values:

	RET

; ******* Sub menu 1 Display (Memory) *********
Display_Sub1:   ; Memory Menu
    MOV SI,BIOS_SubMenu1
    CALL BIOS_PrintScreenBloc

    MOV SI,BIOS_Sub1_Braket
	MOV BL,BK_G ; Black on Grey
    CALL Draw_Brackets

; Display ROM Names

	CALL Disp_EMSInfos
    CALL Disp_RAMOnOff
	CALL Disp_ROMNames

	MOV DX,MEM_RectangleXY  ; X Y
	MOV word CS:[RW_XY],MEM_RectangleXY
    MOV byte CS:[RW_DX],65
    MOV byte CS:[RW_DY],2	
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_PrintRectangle
	
; Display the segment number in the Rectangle	
    XOR AL,AL
	MOV DX,MEM_SegXY
	MOV CX,16
Display_Seg_Numbers:
    PUSH CX
	CALL BIOS_SetCursorPosDX
	PUSH AX
	CALL BIOS_PrintAL_Hex
	POP AX
	INC AL
	ADD DL,2
    POP CX	
    CMP CX,7
    JNE DisplaySegNb_NoSpace
; Insert one space
    INC DL
DisplaySegNb_NoSpace:
	LOOP Display_Seg_Numbers

; Vertical line between conventionnal and video memory
    MOV DX,MEM_RectangleXY+41
	MOV BL,BK_G ; Black on Grey
	MOV AL,194
    CALL BIOS_Printcharxy	
    INC DH
	MOV AL,179
    CALL BIOS_Printcharxy	
    INC DH
	MOV AL,179
    CALL BIOS_Printcharxy
    INC DH
	MOV AL,193
    CALL BIOS_Printcharxy

; Display Aditionnal text
    MOV SI,BIOS_Sub1_MemtableText
    CALL BIOS_PrintScreenBloc
	
;	CALL BIOS_SetCursorPos
; Display the PicoMEM Memory MAP
    MOV SI,PMCFG_PM_MMAP
	MOV AX,MEM_DisplayXY	
    CALL DisplayMEMType

; Initialize the Sub Menu variables
	MOV Byte [SubMenu_Current],0
	MOV Byte [SubMenu_Nb],BIOS_SubMenu1_Nb	
	MOV Word [SubMenu_Coord],BIOS_Sub1_Coord
	MOV Word [SubMenu_Select],BIOS_SelectSub1
	MOV Word [SubMenu_Data],BIOS_Sub1Data
	RET ;Display_Sub1 End

Disp_RAMOnOff:

    CMP byte CS:[BV_PSRAMInit],0F0h	; Does nothing if the PSRAM is not enabled/working
	JB Disp_RAMOnOff_PSRAMOk
	MOV byte CS:[PMCFG_PSRAM_Ext],0		; Force the PSRAM emulated RAM to be Off if PSRAM is failing/disabled
Disp_RAMOnOff_PSRAMOk:

	MOV DX,PMRAM_XY
	MOV AL,CS:[PMCFG_PMRAM_Ext]
	CALL DISPLAY_OnOff_xy

	MOV DX,PSRAM_XY
	MOV AL,CS:[PMCFG_PSRAM_Ext]
	CALL DISPLAY_OnOff_xy

	MOV DX,IgnoreVideo_XY
	MOV AL,CS:[PMCFG_IgnoreAB]
	CALL DISPLAY_OnOff_xy
	
	MOV DX,MaxConv_XY
	MOV AL,CS:[PMCFG_Max_Conv]
	CALL DISPLAY_OnOff_xy	
	RET ; Disp_RAMOnOff End


Disp_ROMNames:
	MOV BL,BK_G
	MOV SI,PMCFG_ROM0Size
	MOV AX,0101h ; Display 1, Kb
	MOV DX,ROM0_NameXY
	CALL DISPLAY_IMGList
	MOV SI,PMCFG_ROM1Size
	MOV AX,0101h ; Display 1, Kb
	MOV DX,ROM1_NameXY
	CALL DISPLAY_IMGList
	RET  ;Disp_ROMNames End

Disp_EMSInfos:

	XOR BH,BH
	MOV DX,EMSPort_XY
	CALL BIOS_SetCursorPosDX
	XOR BH,BH
	MOV BL,CS:[PMCFG_EMS_Port]
	INC BL	
	SHL BX,1
    MOV AX,CS:[EMS_Port_List+BX]
	MOV BL,BK_G ; Black on Grey	
	CALL DISPLAY_12bit_Hex


	MOV DX,EMSAddr_XY
	CALL BIOS_SetCursorPosDX
	
    CMP byte CS:[PMCFG_EMS_Port],0
	JNE Do_Display_EMSAddr	
	MOV SI,STR_Inactive
	CALL BIOS_Printstrc
	RET
Do_Display_EMSAddr:

	XOR BH,BH
	MOV BL,CS:[PMCFG_EMS_Addr]
	INC BL
	SHL BX,1
    MOV AX,CS:[EMS_Addr_List+BX]
	MOV BL,BK_G ; Black on Grey
	CALL DISPLAY_Addr
	RET ; Disp_EMSInfos End
	
Disp_ROMAddr:

	RET

; ******* Sub menu 2 Display (Disk) *********
Display_Sub2: ; Disk Menu
    MOV SI,BIOS_SubMenu2
    CALL BIOS_PrintScreenBloc

	MOV BL,BK_G ; Black on Grey
    MOV SI,BIOS_Sub2_Braket
    CALL Draw_Brackets

	CALL Disp_FDDNames
    CALL Disp_HDDNames
	CALL Disp_Sub2_DiskOnOff

    CMP byte CS:[BV_SDInit],0
	JE Display_Sub2_SDPresent
	MOV SI,BIOS_uSDError
    CALL BIOS_PrintScreenBloc
Display_Sub2_SDPresent:

	
; Initialize the Sub Menu variables
	MOV Byte [SubMenu_Current],0
	MOV Byte [SubMenu_Nb],BIOS_SubMenu2_Nb	
	MOV Word [SubMenu_Coord],BIOS_Sub2_Coord	
	MOV Word [SubMenu_Select],BIOS_SelectSub2
	MOV Word [SubMenu_Data],BIOS_Sub2Data
	RET ;Display_Sub2 End

Disp_FDDNames:
	MOV SI,PMCFG_FDD0Size
	MOV BL,BK_G ; Black on Grey	
	MOV AX,0102h   ; 2 Disk to display, Kb
	MOV DX,FDD0_NameXY
	CALL DISPLAY_IMGList
	RET

Disp_HDDNames:
	MOV SI,PMCFG_HDD0Size
	MOV BL,BK_G ; Black on Grey	
	MOV AX,4
	MOV DX,HDD0_NameXY
	CALL DISPLAY_IMGList
	RET

Disp_Sub2_DiskOnOff:

	MOV DX,PMBOOT_XY
	MOV AL,CS:[PMCFG_PMBOOT]
	CALL DISPLAY_OnOff_xy
	RET ; Disp_Sub2_DiskOnOff End


; ******* Sub menu 3 Display (Audio) *********
Display_Sub3:
    MOV SI,BIOS_SubMenu3
    CALL BIOS_PrintScreenBloc
	
	MOV BL,BK_G ; Black on Grey
    MOV SI,BIOS_Sub3_Braket
    CALL Draw_Brackets

	CALL Disp_Sub3OnOff
	
; Initialize the Sub Menu variables
	MOV Byte [SubMenu_Current],0
	MOV Byte [SubMenu_Nb],BIOS_SubMenu3_Nb
	MOV Word [SubMenu_Coord],BIOS_Sub3_Coord
	MOV Word [SubMenu_Select],BIOS_SelectSub3
	MOV Word [SubMenu_Data],BIOS_Sub3Data
	RET	;Display_Sub3 End

Disp_Sub3OnOff:

	MOV DX,Enable_Output_XY
	MOV AL,CS:[PMCFG_AudioOut]
	CALL DISPLAY_OnOff_xy

	MOV DX,Enable_Adlib_XY
	MOV AL,CS:[PMCFG_Adlib]
	CALL DISPLAY_OnOff_xy

	RET ; Display_Sub3 End


; ******* Sub menu 4 Display (Other) *********
Display_Sub4:
    MOV SI,BIOS_SubMenu4
    CALL BIOS_PrintScreenBloc
	
	MOV BL,BK_G ; Black on Grey
    MOV SI,BIOS_Sub4_Braket
    CALL Draw_Brackets

	CALL Disp_Sub4OnOff
	CALL Disp_Sub4Speed
	CALL Disp_Sub4Wifi
	
; Initialize the Sub Menu variables
	MOV Byte [SubMenu_Current],0
	MOV Byte [SubMenu_Nb],BIOS_SubMenu4_Nb	
	MOV Word [SubMenu_Coord],BIOS_Sub4_Coord
	MOV Word [SubMenu_Select],BIOS_SelectSub4
	MOV Word [SubMenu_Data],BIOS_Sub4Data	
	RET	;Display_Sub4 End

Disp_Sub4OnOff:
	MOV DX,EnableUSB_XY
	MOV AL,CS:[PMCFG_EnableUSB]
	CALL DISPLAY_OnOff_xy

	MOV DX,EnableJoy_XY
	MOV AL,CS:[PMCFG_EnableJOY]
	CALL DISPLAY_OnOff_xy

	RET ; Disp_Sub4OnOff End

; Display the Speed Values (SD, PSRAM)
Disp_Sub4Speed:

	MOV DX,SD_Speed_XY
	MOV AL,CS:[PMCFG_SD_Speed]
	MOV AH,0
	call DISPLAY_Byte_Dec_xy
		
	MOV DX,RAM_Speed_XY
	MOV AL,CS:[PMCFG_RAM_Speed]
	MOV AH,0	
    call DISPLAY_Byte_Dec_xy
	
	RET

%if DOS_COM=1
FakeSSID DB 'MySSID',0
FakeStatus DB ' Connected, -65dB',0
%endif	
	
Disp_Sub4Wifi:

	CMP byte [BV_WifiInit],0FFh		; Disabled
	JE Disp_Sub4Wifi_NoWifi

	CALL PM_GetWifiInfos
    MOV DX,WifiInfos_XY
    CALL BIOS_SetCursorPosDX

	CMP byte [BV_WifiInit],0FCh		; Skipped
	JE Disp_Sub4Wifi_OnlyStatus

	MOV BL,BK_G
%if DOS_COM=1	
	MOV SI,FakeSSID
%else
    MOV SI,PCCR_Param + 6		; Print SSID
%endif	
	CALL BIOS_Printstrc

Disp_Sub4Wifi_OnlyStatus:
	
%if DOS_COM=1	
	MOV SI,FakeStatus
%else
	MOV SI,PCCR_Param + 102		; Print Wifi connection status string
%endif
	CALL BIOS_Printstrc 
	
	JMP Disp_Sub4Wifi_Ok
	
Disp_Sub4Wifi_NoWifi:
	
Disp_Sub4Wifi_Ok:

	MOV DX,ne2k_IRQ_XY
	MOV AL,CS:[PMCFG_WifiIRQ]
	MOV AH,0	
    call DISPLAY_Byte_Dec_xy
	
	MOV DX,ne2k_Port_XY
	MOV AX,CS:[PMCFG_ne2000Port]
    call DISPLAY_12bit_Hex_xy
	
	RET	

; ********************************************************************************************************
; *****   Sub Menu Actions : Perform the selection actions (Values changes) for the sub Menu Items   *****
; ********************************************************************************************************

; ** Selection : On/Off (Memory Menu)
; Switch the value between 0 and 1 (Use Directly the Sub Menu Data)
; One function per sub Menu : Memory, Disk, Sub4

SEL_OnOff:
    CALL Get_SubMenu_DataPtr
	XOR byte ES:[DI],1			 ; Revert the value
    MOV byte CS:[Conf_Changed],1 ; Config is changed

; ** Selection : None
SEL_None:	
	RET

SEL_OnOff_Main:
	CALL SEL_OnOff	
	
	CALL Disp_FastBootOnOff
    CALL Display_SubMenuCursor
	RET

SEL_OnOff_Mem:
	CALL SEL_OnOff 
    
    CALL MEM_Init				; Initialize/Configure the memory
    MOV SI,PMCFG_PM_MMAP
 	MOV AX,MEM_DisplayXY	
	CALL DisplayMEMType

	CALL Disp_RAMOnOff			; Update the display for Memory page
    CALL Display_SubMenuCursor
	RET ;SEL_OnOff_Mem End
	
SEL_OnOff_Disk:
	CALL SEL_OnOff 

	CALL Disp_Sub2_DiskOnOff
    CALL Display_SubMenuCursor
	RET  ;SEL_OnOff_Disk End

SEL_OnOff_Audio:
	CALL SEL_OnOff 

	CALL Disp_Sub3OnOff
    CALL Display_SubMenuCursor
	RET  ;SEL_OnOff_Audio End

SEL_OnOff_Sub4:
	CALL SEL_OnOff

	CALL Disp_Sub4OnOff	
    CALL Display_SubMenuCursor
	RET	;SEL_OnOff_Sub4 End

; ** Selection : EMS I/O Port and On/Off
SEL_EMSPort:

    CMP byte CS:[EMS_Possible],1	; Does nothing if D and E Segment not available
	JE SEL_EMSPort_EMSOK
    RET
SEL_EMSPort_EMSOK:
    CMP byte CS:[BV_PSRAMInit],0F0h	; Does nothing if the PSRAM is not enabled/working
	JB SEL_EMSPort_PSR_OK
	RET
SEL_EMSPort_PSR_OK:

; Display the EMS Selection rectangle
    MOV DX,MEMAListRect_XY
	MOV word [RW_XY],MEMAListRect_XY
    MOV byte [RW_DX],5	
	MOV AH,Byte [EMS_Port_List]
    MOV byte [RW_DY],AH
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_PrintRectangle

; Display the Port List
	MOV BL,BK_G 	; Black on Grey
    MOV DX,MEMAList_XY
    MOV SI,EMS_Port_List
    CALL DISPLAY_12bit_List_Hex

; Start the image selection
    MOV DX,MEMAListRect_XY+256+1
	MOV Word [PM_I],DX     ; PM_I : Cursor Position
	MOV byte [PM_J],0      ; PM_J : Selection Index
    MOV AL,Byte [EMS_Port_List]
	DEC AL
	MOV Byte [PM_K],AL     ; Nb of Item to select from

    CALL Do_SelectList_OnePage
    PUSH AX

; Clean the selection list		
	MOV DX,MEMAListRect_XY
    MOV AX,8
	MOV CX,7
	CALL Clean_Rectangle
    POP AX ; Restore the selected item Nb

	CMP AL,0FFh
	JE SEL_ROMAddr_End	; Pressed Escape in the selection

	MOV byte CS:[PMCFG_EMS_Port],AL	; Saved the selected port number
    CMP AL,0
	JE EMS_Is_Disabled

; Set a default value

; Set the EMS Segment to a not used Segment (D or E)
	CMP byte CS:[Seg_D_NotUsed],0
	JE EMS_DontSetD
	MOV byte CS:[PMCFG_EMS_Addr],0
    MOV byte CS:[Conf_Changed],1			; Config is changed	
EMS_DontSetD:
	CMP byte CS:[Seg_E_NotUsed],0
	JE EMS_DontSetE
	MOV byte CS:[PMCFG_EMS_Addr],1
    MOV byte CS:[Conf_Changed],1			; Config is changed	
EMS_DontSetE:

EMS_Is_Disabled:

    CALL MEM_Init	; Initialize/Configure the memory
    MOV SI,PMCFG_PM_MMAP
	MOV AX,MEM_DisplayXY	
	CALL DisplayMEMType

    CALL Disp_EMSInfos

SEL_ROMAddr_End:
    CALL Display_SubMenuCursor
	RET 	; SEL_EMSPort End

; ** Selection : EMS Base Address
SEL_EMSAddr:

	CMP byte CS:[PMCFG_EMS_Port],0	; Can't selec the address if no port
	JE Sel_EMS_Addr_Ko
	CMP Byte CS:[Seg_D_NotUsed],1
	JNE Sel_EMS_Addr_Ko
	CMP Byte CS:[Seg_E_NotUsed],1
	JE Sel_EMS_Addr_Ok
Sel_EMS_Addr_Ko:
	RET
Sel_EMS_Addr_Ok:

; Display the EMS Selection rectangle
    MOV DX,MEMAListRect_XY
	MOV word [RW_XY],MEMAListRect_XY
    MOV byte [RW_DX],6	
	MOV AH,Byte [EMS_Addr_List]
    MOV byte [RW_DY],AH	
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_PrintRectangle

; Display the Add List
    MOV DX,MEMAList_XY
    MOV SI,EMS_Addr_List
	MOV BL,BK_G ; Black on Grey
    CALL DISPLAY_AddrList	

; Start the image selection
    MOV DX,MEMAListRect_XY+256+1
	MOV Word [PM_I],DX     ; PM_I : Cursor Position
	MOV byte [PM_J],0      ; PM_J : Selection Index
    MOV AL,Byte [EMS_Addr_List]
	DEC AL
	MOV Byte [PM_K],AL     ; Nb of Item to select from

    CALL Do_SelectList_OnePage
    PUSH AX

; Clean the selection list		
	MOV DX,MEMAListRect_XY
    MOV AX,8
	MOV CX,13
	CALL Clean_Rectangle
    POP AX ; Restore the selected item Nb

	CMP AL,0FFh
	JE SEL_EMSAddr_Cancel

	MOV CS:[PMCFG_EMS_Addr],AL  ; Update the EMS Addr variable
    MOV byte CS:[Conf_Changed],1			; Config is changed	

    CALL MEM_Init	; Initialize/Configure the memory
    MOV SI,PMCFG_PM_MMAP
	MOV AX,MEM_DisplayXY	
	CALL DisplayMEMType
	
    CALL Disp_EMSInfos

SEL_EMSAddr_Cancel:

    CALL Display_SubMenuCursor

    RET ; SEL_EMSAddr End

; ** Select a ROM image
SEL_ROMImages:
    CALL PM_ReadROMList

	CMP Byte [PM_ROMTotal],0
	JNE SEL_ROMImagesOk
	RET
SEL_ROMImagesOk:

; Print the FDD Selection rectangle
    MOV DX,ROMListRect_XY
	MOV word [RW_XY],ROMListRect_XY	
    MOV byte [RW_DX],21	
	MOV AH,Byte [PM_ROMTotal]
	INC AH    ;+1 to Add None
    MOV byte [RW_DY],AH	
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_PrintRectangle

; Print None in the first line
	MOV DX,ROMList_XY
	CALL Print_None
; Print the image list
	MOV DX,ROMList_XY	
    MOV SI,PCCR_Param
    MOV AH,1	; Kb
	MOV AL,Byte [PM_ROMTotal]
	INC DH     ;+1 to Add None
	MOV BL,BK_G ; Black on Grey
    CALL DISPLAY_IMGList

    CALL Clean_SubMenuCursor
	
; Start the image selection
    MOV DX,ROMListRect_XY+256+1
	MOV Word [PM_I],DX     ; PM_I : Cursor Position
	MOV byte [PM_J],0      ; PM_J : Selection Index
	MOV AL,Byte [PM_ROMTotal]
	MOV Byte [PM_K],AL     ; PM_K : Nb of value in the selection list

    CALL Do_SelectList_OnePage
    CALL Copy_ImageParam
    JNC Sel_ROM_NotUpdateDisplay

    MOV DX,ROM0_NameXY
    MOV AX,15
	MOV CX,2
	CALL Clean_Rectangle

; Update the ROM Names Display
	CALL Disp_ROMNames
	
    CALL MEM_Init	; Initialize/Configure the memory
    MOV SI,PMCFG_PM_MMAP
	MOV AX,MEM_DisplayXY	
	CALL DisplayMEMType
	
Sel_ROM_NotUpdateDisplay:

; Clean the selection list		
	CALL Clean_Rectangle_var
    CALL Display_SubMenuCursor

    RET ;SEL_ROMImages End

; ********************************************************************************************************
; ****************************** Change Floppy Image on the fly  *****************************************
; ********************************************************************************************************

;STR_SelectA DB 2,46,G_B,1,'Select A: Disk',0
ChFDDListRect_XY EQU 256*2+46

BM_ChangeFDD0Image:

    PUSH CS
	POP DS
    MOV byte [Conf_Changed],0
    MOV byte [ListLoaded],0		; Force to Relead the list
    CALL PM_ReadFDDList

	CMP Byte [PM_FDDTotal],0
	JNE Change_FDD0ImageOk
	RET
Change_FDD0ImageOk:

    MOV word [L_RectXY],ChFDDListRect_XY
	MOV AL,Byte [PM_FDDTotal]
    MOV byte [L_TotalEntry],AL
	MOV byte [PM_L],1		 ; 1 Display Kb
    CALL Do_SelectImageList  ; Currently only display the list
    
	MOV Word [SubMenu_Data],BIOS_Sub2Data
	MOV byte [SubMenu_Current],0 	; Sub menu 0 is FDD0
    CALL Copy_ImageParam			; Copy the Image entry to the Parameter Address
    JNC Change_FDD0_NotUpdated

    MOV byte CS:[Conf_Changed],1	; Config is changed	
	
Change_FDD0_NotUpdated:
    RET ;BM_ChangeFDD0Image


; ** Select a Floppy Disk image
SEL_FDDImages:
    CALL PM_ReadFDDList

	CMP Byte [PM_FDDTotal],0
	JNE SEL_FDDImagesOk
	RET
SEL_FDDImagesOk:

    MOV word [L_RectXY],FDDListRect_XY
	MOV AL,Byte [PM_FDDTotal]
    MOV byte [L_TotalEntry],AL	
	MOV byte [PM_L],1		 ; 1 Display Kb	
    CALL Do_SelectImageList  ; Currently only display the list

; Start the image selection
;    MOV DX,word [L_RectXY]
;	ADD DX,256+1
;	MOV Word [PM_I],DX     ; PM_I : Cursor Position
;	MOV byte [PM_J],0      ; PM_J : Selection Index
;	MOV AL,[L_CurrPageEntry]
;	MOV Byte [PM_K],AL     ; PM_K : Nb of value in the selection list

;    CALL Do_SelectList
	
    CALL Copy_ImageParam	; Copy the Image entry to the Parameter Address
    JNC Sel_FDD_NotUpdateDisplay

    MOV byte CS:[Conf_Changed],1			; Config is changed	

	CALL Disp_FDDNames
Sel_FDD_NotUpdateDisplay:

; Clean the selection list		
    CALL Display_SubMenuCursor

    RET ;SEL_FDDImages End

; ** Select an HDD image	
SEL_HDDImages:
    CALL PM_ReadHDDList

	CMP Byte [PM_HDDTotal],0
	JNE SEL_HDDImagesOk
	RET
SEL_HDDImagesOk:

    MOV word [L_RectXY],HDDListRect_XY
	MOV AL,Byte [PM_HDDTotal]
    MOV byte [L_TotalEntry],AL	
	MOV byte [PM_L],0		 ; 0 Display Mb
    CALL Do_SelectImageList  ; Currently only display the list
	
; Start the image selection
;    MOV DX,word [L_RectXY]
;	ADD DX,256+1
;	MOV Word [PM_I],DX     ; PM_I : Cursor Position
;	MOV byte [PM_J],0      ; PM_J : Selection Index
;	MOV AL,[L_CurrPageEntry]
;	MOV Byte [PM_K],AL     ; PM_K : Nb of value in the selection list

;    CALL Do_SelectList
	
    CALL Copy_ImageParam
    JNC Sel_HDD_NotUpdateDisplay

    MOV byte CS:[Conf_Changed],1			; Config is changed	

	CALL Disp_HDDNames
Sel_HDD_NotUpdateDisplay:

; Clean the selection list	
    CALL Display_SubMenuCursor
    RET

; Select the ne2000 IRQ number from a byte list
SEL_Wifi_IRQ:
	MOV DX,Wifi_IRQ_Sel_XY
	MOV SI,Wifi_IRQList
	MOV AL,3
	CALL Do_Select_Byte_List
	JNC SEL_Wifi_IRQ_Cancelled
	
	MOV CS:[PMCFG_WifiIRQ],AL
	CALL Disp_Sub4Wifi

SEL_Wifi_IRQ_Cancelled:
	CALL Display_SubMenuCursor
	RET


; Select the ne2000 port number from a word list
SEL_NE2000_Port:

	CMP byte [BV_WifiInit],0		; Enabled ?
	JNE SEL_NE2000_Port_Cancelled

	MOV DX,ne2k_Port_Sel_XY
	MOV SI,NE2000_PortList
	MOV AL,5
	CALL Do_Select_12Bit_List
	JNC SEL_NE2000_Port_Cancelled

	MOV CS:[PMCFG_ne2000Port],AX
	CALL Disp_Sub4Wifi

SEL_NE2000_Port_Cancelled:
	CALL Display_SubMenuCursor
	RET


; ** Selection : SD Speed
; Select from a Byte List
SEL_SD_Speed:
	MOV DX,SD_Speed_rect_XY
	MOV SI,SD_Speed_List
	MOV AL,5
	CALL Do_Select_Byte_List

	JNC SEL_SD_Speed_Cancelled
	MOV CS:[PMCFG_SD_Speed],AL  ; Update the Sd Speed variable
	
	CALL Disp_Sub4Speed	

SEL_SD_Speed_Cancelled:
	CALL Display_SubMenuCursor
	RET	; SEL_SD_Speed End

; ** Selection : RAM Speed
; Select from a Byte List
SEL_RAM_Speed:

	MOV DX,RAM_Speed_rect_XY
	MOV SI,RAM_Speed_List
	MOV AL,5

	CALL Do_Select_Byte_List

	JNC SEL_RAM_Speed_Cancelled
	MOV CS:[PMCFG_RAM_Speed],AL  ; Update the Sd Speed variable
	
	CALL Disp_Sub4Speed

SEL_RAM_Speed_Cancelled:
	CALL Display_SubMenuCursor
	RET	; SEL_RAM_Speed End


STR_DiskSize   DB 32,13,BL_G,1,'New Disk Size (MB):',0
STR_DiskName   DB 32,14,BL_G,1,'Disk Name:',0
STR_CreateDisk DB 32,15,BL_G,1,'Creating Image : ',0

STR_NDError  DB 32,16,BL_G,1,'Error',0

Do_NewDiskImage:

    CMP byte CS:[BV_SDInit],0
	JE Do_NewDiskImage_Continue
	RET
Do_NewDiskImage_Continue:

	MOV DX,256*12+30
	MOV word CS:[RW_XY],256*12+30	
    MOV byte CS:[RW_DX],32
    MOV byte CS:[RW_DY],5
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_PrintRectangle

	MOV BL,BL_G ; Black on Grey
	MOV SI,STR_DiskSize
	call BIOS_PrintTextCol
	
; Ask for the Disk Size
    PUSH DS
	POP ES
	MOV DI,PCCR_Param
	MOV BL,BK_G
	MOV DX,256*13+32+20
	MOV CX,4
	CALL BIOS_WordInput
	JNC  Do_NewDiskImage_Cancel
	
	PUSH AX

	MOV BL,BL_G ; Black on Grey
	MOV SI,STR_DiskName
	CALL BIOS_PrintTextCol

 	MOV DI,PCCR_Param+2
	MOV BL,BK_G
	MOV DX,256*14+22+21
	MOV CX,13
	CALL BIOS_StringInput
	POP AX
	JNC  Do_NewDiskImage_Cancel
	CMP byte CS:[PCCR_Param+2],0	; Cancel if name is empty
	JE  Do_NewDiskImage_Cancel

; Correct the Disk Size (>10Mb < 4Gb)
	MOV BX,10
	CMP AX,BX
	JB  Do_NewDiskImage_ChangeSize
	MOV BX,4*1024
	CMP AX,BX
	JBE Do_NewDiskImage_SizeOk
Do_NewDiskImage_ChangeSize:	
	MOV AX,BX
Do_NewDiskImage_SizeOk:

    MOV CS:[PCCR_Param],AX

	PUSH SI	; Save the Image name text offset
	MOV BL,BK_G ; Black on Grey
	MOV SI,STR_CreateDisk
	CALL BIOS_PrintTextCol
	
	MOV BL,BK_G ; Black on Grey
    MOV AX,CS:[PCCR_Param]
	CALL BIOS_PrintAX_Dec
	
	MOV BL,BK_G ; Black on Grey	
	MOV SI,STR_MB
	CALL BIOS_Printstrc 
	
	POP SI
  
%if DOS_COM=1
	MOV AL,4
%else
	MOV AH,CMD_HDD_NewImage
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end
	JNC CMD_HDD_NewImageOk

%endif ; DOS_COM=1
; ** PicoMEM CMD Returns an error **
	CALL PM_GetErrorString
	PUSH SI
	MOV DX,256*16+32
	CALL BIOS_SetCursorPosDX
	
	MOV SI,STR_ERROR
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_Printstrc
	POP SI
	CALL BIOS_Printstrc

	JMP CMD_HDD_NewImageEnd
	
CMD_HDD_NewImageOk:

	MOV DX,256*16+32
	CALL BIOS_SetCursorPosDX
	
	MOV SI,STR_DONE
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_Printstrc

CMD_HDD_NewImageEnd:	

	MOV DX,256*17+32
	CALL BIOS_SetCursorPosDX
	
	MOV SI,STR_PressAny
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_Printstrc	

	CALL WaitKey_Loop
	
	MOV byte [ListLoaded],0		; Force the HDD List reload

Do_NewDiskImage_Cancel:

	CALL Clean_Rectangle_var
	
	CALL Display_SubMenuCursor
	RET

;*****************************************************************************************************	

; SI : Number List Offset
; DX : Position on Screen
; AL : Width of the selection rectangle
; Return 
; CF=1 AL Contains the new yalue
; CF=0 Cancelled : Don't change Value

Do_Select_12Bit_List:
	MOV AH,0
	JMP Do_Select_Integer_List

Do_Select_Byte_List:
    MOV AH,1
	JMP Do_Select_Integer_List

Do_Select_Integer_List:	

	PUSH AX
	PUSH AX
; Display the Selection rectangle
	MOV word [RW_XY],DX
    MOV byte [RW_DX],AL
	MOV AH,Byte [SI]
    MOV byte [RW_DY],AH	
	MOV BL,BK_G ; Black on Grey
	CALL BIOS_PrintRectangle

	POP AX

	CMP AH,0
	JNE Do_Select_ByteDisplay
; Display the List (Hexa, 12Bit)
	ADD DX,256+2  	; Move initial cursor position
	MOV BL,BK_G ; Black on Grey
    CALL DISPLAY_12bit_List_Hex
	JMP Do_Select__DisplayEnd
	
Do_Select_ByteDisplay:
; Display the List (Decimal Byte)
	ADD DX,256+2  	; Move initial cursor position
	MOV BL,BK_G ; Black on Grey
    CALL DISPLAY_Byte_List_Dec 

Do_Select__DisplayEnd:

; Start the selection
	DEC DX			; Move initial cursor position
	MOV Word [PM_I],DX     ; PM_I : Cursor Position
	MOV byte [PM_J],0      ; PM_J : Selection Index
    MOV AL,Byte [SI]
	DEC AL
	MOV Byte [PM_K],AL     ; Nb of Item to select from

    PUSH DX
    CALL Do_SelectList_OnePage
	POP DX 
	
    PUSH AX
; Clean the selection list	
	SUB DX,256+1
    MOV AX,7
	XOR CX,CX
	MOV CL,Byte [SI]	; Read the Nb of values in the list
    ADD CX,2
	CALL Clean_Rectangle
    POP AX ; Restore the selected item Nb
	
	POP BX	; Restore AX (Byte or 12Bit ?)
	CMP AL,0FFh
	JE Select_Byte_List_Cancel

	CMP BH,0
	JNE Do_Read_ByteData
	
	XOR AH,AH
	SHL AX,1
	MOV BX,AX
	MOV AX,Word [SI+2+BX]	
	JMP Select_Byte_List_End
	
Do_Read_ByteData:
	XOR AH,AH
	MOV BX,AX
	MOV AL,Byte [SI+1+BX]

Select_Byte_List_End:	
    MOV byte  CS:[Conf_Changed],1  ; Config is changed	
	STC		; AL Contains the new Value
	RET
Select_Byte_List_Cancel:

	CLC		; cancelled : Don't change Value
    RET ; Do_Select_Byte_List End

; Manage the display and selection from an Image List

; PM_I : Display KB/MB

Do_SelectImageList:

; Initialize the variables
	MOV AL,byte [L_TotalEntry]
	DEC AL
	MOV CL,4
	SHR AL,CL
	INC AL
	MOV byte [L_TotalPage],AL	; Total Nb of page is Nb of entry / 16
	MOV byte [L_CurrentPage],0
	
SI_AnotherPage:

	MOV AL,byte [L_CurrentPage]
	MOV CL,4
	SHL AL,CL
	MOV AH,byte [L_TotalEntry]
	SUB AH,AL
	XCHG AL,AH
	CMP AL,16
	JB SI_Less16InPage
	MOV AL,16
SI_Less16InPage:

;PUSH AX
;PUSH AX
;MOV DX,5
;CALL BIOS_SetCursorPosDX
;POP AX
;XOR AH,AH
;CALL BIOS_PrintAX_Dec
;POP AX

    MOV byte [L_CurrPageEntry],AL

; Print the rectangle
    MOV DX,word [L_RectXY]
	MOV word [RW_XY],DX
    MOV byte [RW_DX],21
	MOV AH,Byte [L_CurrPageEntry]
	CMP byte [L_CurrentPage],0
	JNE PIL_NotAddNone
	INC AH    ;+1 to Add None
PIL_NotAddNone:	
    MOV byte [RW_DY],AH	
	MOV BL,BK_G ; Black on Grey

; Save the Background and print the disk list

	CALL BIOS_SaveRectangle
	CALL BIOS_PrintRectangle

; Print the selection page number	
    MOV DX,word [L_RectXY]
	ADD DL,byte [RW_DX]
	SUB DL,3
	ADD DH,byte [RW_DY]
	INC DH
	CALL BIOS_SetCursorPosDX

	XOR AX,AX
	MOV AL,byte [L_CurrentPage]
	INC AL
	CALL BIOS_PrintAX_Dec
	MOV AL,'/'
	CALL BIOS_Printchar	
	XOR AX,AX
	MOV AL,byte [L_TotalPage]
	CALL BIOS_PrintAX_Dec

; Print the image list
	MOV DX,word [L_RectXY]
	ADD DX,256+2
; Print None or not	
	CMP byte [L_CurrentPage],0
	JNE PIL_NoPrintNone
	PUSH DX
	CALL Print_None				    ; Print None in the first line
	POP DX
	INC DH     ;+1 to Add None
PIL_NoPrintNone:	
; define the Image List pointer
    MOV SI,PCCR_Param
	XOR AX,AX
	MOV AL,[L_CurrentPage]
	MOV CL,8		; * 16 per page * 16 byte per entry
	SHL AX,CL
	ADD SI,AX	

	MOV AH,byte [PM_L]				; Get the saved value for Kb/Mb
	MOV AL,Byte [L_CurrPageEntry]
	MOV BL,BK_G ; Black on Grey
; Input :
; DX : 256*Y+X
; AL : Number of disk to Display
; AH : 0:MB 1:KB
; BL : Attribute
; DS:[SI] : Image list Ptr	
    CALL DISPLAY_IMGList

; Start the image selection
    MOV DX,word [L_RectXY]
	ADD DX,256+1
	MOV Word [PM_I],DX     ; PM_I : Cursor Position
	MOV byte [PM_J],0      ; PM_J : Selection Index
	MOV AL,[L_CurrPageEntry]
	CMP byte [L_CurrentPage],0
	JE PIL_NotRemoveNone
	DEC AL    ;No None, remove one
PIL_NotRemoveNone:
	MOV Byte [PM_K],AL     ; PM_K : Nb of value in the selection list

    CALL Do_SelectList

	PUSH AX
	CALL BIOS_RestoreRectangle	
	POP AX

;PUSH AX
;PUSH AX
;MOV DX,8
;CALL BIOS_SetCursorPosDX
;POP AX
;XOR AH,AH
;CALL BIOS_PrintAL_Hex
;POP AX

	CMP AL,0FEh                ; > Next Page
	JNE SI_NotNextPage
	INC byte [L_CurrentPage]
	JMP SI_AnotherPage
SI_NotNextPage:
	CMP AL,0FDh                ; > Prev Page
	JNE SI_NotPrevPage
	DEC byte [L_CurrentPage]
	JMP SI_AnotherPage
SI_NotPrevPage:	
	
	RET

Do_ImageList_ChangeFolder:

    MOV AL,byte [ListLoaded]  ; What was the list Loaded ?
	RET
	

; Manage the keys and Display the cursor to select from a list
; DX   : 256*y+x
; PM_I : Initial Position
; PM_J : Index
; PM_K : Number of Items
; Returns : AL, selected Index or FFh for escape

Do_SelectList_OnePage:
	MOV byte [L_CurrentPage],0
	MOV byte [L_TotalPage],1
Do_SelectList:
    CALL Display_SelectMenuCursor

; *** Manage the Keys in the Selection sub Menu
Wait_Key_SelectMenu:
	CALL BIOS_Keypressed
	JZ Wait_Key_SelectMenu
	
	MOV BL,7
	CALL BIOS_ReadKey

; Keys for the navigation
	CMP AL,KeyUp
	JNE SM_TestKeyDown
	JMP Do_PrevSelectMenu
SM_TestKeyDown:
	CMP AL,KeyDown
	JNE SM_TestKeyEspace
	JMP Do_NextSelectMenu

SM_TestKeyEspace:
	CMP AL,KeyEspace
	JNE SM_TestKeyEnter
	MOV AL,byte [PM_J]         ; > Return the Index as Selected
    JMP SM_Select_End
SM_TestKeyEnter:
	CMP AL,KeyEnter
	JNE SM_TestKeyPgUp
	MOV AL,byte [L_CurrentPage]
	MOV CL,4
	SHL AL,CL
	ADD AL,byte [PM_J]         ; > Return the Index as Selected
	CMP byte [L_CurrentPage],0
	JE NotIncRetIndex
	INC AL
NotIncRetIndex:	
    JMP SM_Select_End
	
SM_TestKeyPgUp:
	CMP AL,KeyPgUp
	JNE SM_TestKeyPgDn
    JMP Do_SelectMenu_PrevPage
SM_TestKeyPgDn:
	CMP AL,KeyPgDn
	JNE SM_TestKeyEscape
    JMP Do_SelectMenu_NextPage
	
SM_TestKeyEscape:
	CMP AL,KeyEsc
	JNE Wait_Key_SelectMenu    ; Loop Back
	MOV AL,0FFh                ; > Return 0FFh > Nothing Selected

SM_Select_End: ; Value selected or Escape
	RET

; *******  Select Item Code *********
Do_SelectMenu_Skip:
   JMP Wait_Key_SelectMenu

Do_SelectMenu_NextPage:
	MOV AL,byte [L_CurrentPage]
	INC AL
	CMP byte [L_TotalPage],AL
	JE Do_SelectMenu_Skip
	printstr_w STR_OK
	
	MOV AL,0FEh                ; > Return 0FEh > Next Page
	RET

Do_SelectMenu_PrevPage:
	CMP byte [L_CurrentPage],0
	JE Do_SelectMenu_Skip	   ; If first page, Skip
	MOV AL,0FDh                ; > Return 0FDh > Prev Page
	RET

Do_PrevSelectMenu:
;PUSH AX
;MOV AL,Byte [PM_J]
;PUSH AX
;MOV DX,10
;CALL BIOS_SetCursorPosDX
;POP AX
;XOR AH,AH
;CALL BIOS_PrintAL_Hex
;POP AX

    CMP Byte [PM_J],0
	JE Do_SelectMenu_PrevPage
    CALL Clean_SelectMenuCursor
    DEC Byte [PM_J]
	JMP Do_ChangeSelectMenu

Do_NextSelectMenu:
    MOV AL,Byte [PM_J]

;PUSH AX
;PUSH AX
;MOV DX,10
;CALL BIOS_SetCursorPosDX
;POP AX
;XOR AH,AH
;CALL BIOS_PrintAL_Hex
;POP AX
	
    CMP Byte [PM_K],AL
    JA  Do_NextSelectMenuOk
    JMP Do_SelectMenu_NextPage   
Do_NextSelectMenuOk:
    CALL Clean_SelectMenuCursor
    INC Byte [PM_J]

Do_ChangeSelectMenu:
	CALL Display_SelectMenuCursor
    JMP Wait_Key_SelectMenu
; Do_SelectList End


; Copy_ImageParam : Copy the selected image Date to the current menu item parameter
;
; Input : AL, Selected image number (FFh if escape)
;         SubMenu_Data and SubMenu_Current used to define where to copy the Image Data
; Output C Flag : Value changed
Copy_ImageParam:
; Test if Esc pressed.	
	CMP AL,0FFh
	JE CI_Nothing

; Put the BIOS Parameter Data Offset in ES:DI
    CALL Get_SubMenu_DataPtr

; Test if "None" Selected	
    CMP AL,1
	JAE CI_NotNone
; None : Size 0 and First Char 0 for none
    XOR AX,AX
	STOSW
	STOSB
    JMP CI_Changed
CI_NotNone:
    SUB AL,1
; Not None : Copy the Size/Name to the selected BIOS Parameter Data
	MOV AH,0
	MOV CL,4
	SHL AX,CL  ; *16
	ADD AX,PCCR_Param  ; PCCR_Param + selection index*16
    MOV SI,AX
	CMP Word [SI],0FFFFh  ; 0FFFFh
	JE CI_Nothing
    MOV CX,8
    REP MOVSW          ; Copy 16 Bytes

CI_Changed:    
    STC
	RET
CI_Nothing:
    CLC
    RET ; Copy_ImageParam End

; set ES:DI to a pointer to the current sub menu Data
Get_SubMenu_DataPtr:
    MOV SI,[SubMenu_Data]
	XOR BX,BX
	MOV BL,byte [SubMenu_Current]
	SHL BX,1
    MOV DI,word [SI+BX]
	PUSH DS
	POP ES
	RET	;Get_SubMenu_DataPtr End

; Input:
; word [PM_I] : Index = Position
; byte [PM_J] : Selection Index
Display_SelectMenuCursor:
    MOV AL,'>'
	JMP WriteChar_Display_SelectMenuCursor

Clean_SelectMenuCursor:
    MOV AL,' '
WriteChar_Display_SelectMenuCursor:
	MOV DX,Word [PM_I]
    ADD DH,byte [PM_J]
    MOV BL,BL_G
	CALL BIOS_Printcharxy
	RET	; Display_SelectMenuCursor End

STR_None DB '     -=None=-      ',0
	
Print_None:
 	CALL BIOS_SetCursorPosDX
	
	MOV SI,STR_None
    CALL BIOS_Printstrc
    RET ; Print_None End

; *******************	

BIOS_PrintScreenBloc:
.Print_ScreenlLoop:
	LODSW     ; Set Position
	CMP AX,0xFFFF
	JE .PSB_End
    CALL BIOS_SetCursorPos
	LODSW     ; AL : Attribute, AH: Char or String
	MOV BL,AL ; Set Attribute
	OR AH,AH
	JE .DoChar
	CALL BIOS_Printstrc
	JMP .Print_ScreenlLoop
.DoChar:
    LODSB
	MOV CL,AL
    XOR CH,CH
	MOV BH,PAGE
	MOV AH,09h
	LODSB
    INT 10h

	JMP .Print_ScreenlLoop	
.PSB_End:
    RET  ; BIOS_PrintScreenBloc End

; Print a Text, Skip the color and text type (To Highlight a text)
; BL: Attribute
BIOS_PrintTextCol:
	LODSW     ; Set Position
    CALL BIOS_SetCursorPos
	LODSW
	CALL BIOS_Printstrc
    RET ; BIOS_PrintTextCol end

; Draw a rectangle
; DH=Row DL=Column 
; BL=Attribute
; CX : Length -2
; Preserve BX
BIOS_PrintRectangle:
	MOV DX,CS:[RW_XY]
	
	PUSH BX
	PUSH SI
	PUSH DX
	
	MOV BH,PAGE
	
    PUSH DX
	MOV AL,0DAh ; First Char '┌'
	CALL BIOS_Printcharxy	
	INC DL       ; Next Column
 	CALL BIOS_SetCursorPosDX
	MOV CL,CS:[RW_DX] 	; Delta X
	MOV AX,09C4h     	; Line '─'
    INT 10h	
	ADD DL,CL    ; End of the line
	MOV AL,0BFh  ; Last Char '┐'
	CALL BIOS_Printcharxy	
	POP DX

	XOR CH,CH
	MOV CL,CS:[RW_DY] ; Delta Y
Draw_Rect_Loop:
	INC DH      ; Next Row
	PUSH CX
    PUSH DX

	MOV AL,0B3h ; First Char '│'
	CALL BIOS_Printcharxy

	INC DL      ; Next Column
	MOV AH,02h  ; GotoXY (DH DL)
	INT 10h
	MOV CL,CS:[RW_DX] 	; Delta X
	MOV AX,0920h     	; Space ' '
    INT 10h
	
	ADD DL,CL   ; End of the line
	MOV AL,0B3h ; Last Char '│'
	CALL BIOS_Printcharxy
	
    POP DX
	POP CX
	LOOP Draw_Rect_Loop

	INC DH      ; Next Row
	MOV AL,0C0h ; First Char '└'
	CALL BIOS_Printcharxy

	INC DL      ; Next Column
	MOV AH,02h  ; GotoXY (DH DL)
	INT 10h
	MOV CL,CS:[RW_DX] 	; Delta X
	MOV AX,09C4h     	; Line '─'
    INT 10h	
	
	ADD DL,CL   ; End of the line
	MOV AL,0D9h ; Last Char '┘'
	CALL BIOS_Printcharxy

	POP DX
	POP SI
	POP BX
    RET

; Can't use AX, BX, DX
; CX : Nb of Row to clean
; AX : Nb of Column to Clean
Clean_Rectangle_var:  ; Read the parameters from the Table variable
	MOV DX,CS:word [RW_XY]
	XOR AX,AX
	MOV AL,CS:[RW_DX]
	INC AX		; Value used to draw rectangle is -2
	INC AX
	XOR CX,CX
	MOV CL,CS:[RW_DY]
	ADD CX,2		; Value used to draw rectangle is -2

Clean_Rectangle:
;Clean the sub Menu text
	MOV BH,PAGE
Clean_RectangleLoop:
    PUSH CX
	PUSH AX
	MOV AH,02h    ; GotoXY (DH DL)
	INT 10h
	INC DH        ; Next Line
    MOV BL,BL_G
	POP AX
	MOV CX,AX
	MOV AX,0920h  ; Space ' '
    INT 10h
    MOV AX,CX	
    POP CX
	LOOP Clean_RectangleLoop
	RET
	
; DS:SI The Bracket list
Draw_Brackets:

; Display the Brackets [] for a sub menu
Draw_NextBracketBlock:    
	LODSW    ; Load the Coordinate
	CMP AX,0FFFFh
	JNE Continue_Draw_Brackets
    RET 
Continue_Draw_Brackets:
    MOV DX,AX
	DEC DH
	LODSB
	MOV byte [PM_I],AL
	
Draw_Br_Loop:
	INC DH      ; Next Row
    PUSH DX
	MOV AL,5Bh  ; First Char '['
	CALL BIOS_Printcharxy

	INC DL       ; Next Column
	MOV AH,02h   ; GotoXY (DH DL)
	INT 10h
	MOV CL,[SI]  ; CL, Nb of Space
	MOV AX,0920h ; Space ' '
    INT 10h	
	ADD DL,CL    ; End of the line
	
	MOV AL,5Dh   ; Last Char ']'
	CALL BIOS_Printcharxy	
	
    POP DX
	DEC byte [PM_I] ; Nb of Row
	JNZ Draw_Br_Loop
	INC SI
	JMP Draw_NextBracketBlock

; Display an Address List
; DX : 256*Y+X
; BL : Attribute
; DS:[SI] : Image list Ptr
DISPLAY_AddrList:
    LODSW 	; Load the number of Address in the list
	CMP AL,0
	JNE DISPLAY_AddrListOk
	RET
DISPLAY_AddrListOk:
	MOV byte [PM_I],AL
	
    PUSH CS
	POP DS

Loop_DISPLAY_AddrList:
	MOV AH,02h         ; GotoXY (DL DH)
	INT 10h
	INC DH
	LODSW
	PUSH SI
	PUSH DX
	CALL DISPLAY_Addr  ; Display the Address in Hex
	POP DX
    POP SI

	DEC byte [PM_I]
	JNZ Loop_DISPLAY_AddrList
    RET

; Display an Address With "Off" if 0
DISPLAY_Addr:
    CMP AX,0
    JNE DISPLAY_Addr_NotOff
	MOV SI,STR_Off4
	CALL BIOS_Printstrc
	RET
DISPLAY_Addr_NotOff:
	CALL BIOS_PrintAX_Hex ; Display the Port in Hex
	RET ; DISPLAY_Addr End
	
; Display a word list as 12Bit Hexa (Same as @ but show 12Bit)
; Input : 
; DS : DS=CS
; DX : 256*Y+X
; BL : Attribute
; DS:[SI] : Word value list
; Preserve SI, DX
DISPLAY_12bit_List_Hex:
	PUSH SI
	PUSH DX
    LODSW 	; Load the number of Address in the list
	CMP AL,0
	JNE DISPLAY_12bit_List_HexOk
	POP DX
	POP SI	
	RET
DISPLAY_12bit_List_HexOk:
	MOV byte [PM_I],AL

Loop_DISPLAY_12bit_List_Hex:
	MOV AH,02h      	   ; GotoXY (DL DH)
	INT 10h
	INC DH
	LODSW
	PUSH SI
	PUSH DX
    CALL DISPLAY_12bit_Hex
	POP DX
    POP SI

	DEC byte [PM_I]
	JNZ Loop_DISPLAY_12bit_List_Hex

	POP DX
	POP SI	
    RET ; DISPLAY_12bit_List_Hex End

; Display a 12Bit value in Hex With "Off" if 0
; DX: Position (XY)
; AX: Value to display
DISPLAY_12bit_Hex_xy:
 	CALL BIOS_SetCursorPosDX
	
DISPLAY_12bit_Hex:
    CMP AX,0
    JNE DISPLAY_12bit_Hex_NotOff
	MOV SI,STR_Off4
	CALL BIOS_Printstrc
	RET
DISPLAY_12bit_Hex_NotOff:
	CALL BIOS_PrintAX_Hex3 ; Display the Port in Hex (12bit)	
	RET ; DISPLAY_12bit_Hex End

; Display a word list as 12Bit Hexa (Same as @ but show 12Bit)
; Input : 
; DS : DS=CS
; DX : 256*Y+X
; BL : Attribute
; DS:[SI] : Word value list
; Preserve SI, DX
DISPLAY_Byte_List_Dec:
	PUSH SI
	PUSH DX
    LODSB 	; Load the number of Address in the list
	CMP AL,0
	JNE DISPLAY_Byte_List_DecOk
	POP DX
	POP SI
	RET
DISPLAY_Byte_List_DecOk:
	MOV byte [PM_I],AL
	
Loop_DISPLAY_Byte_List_Dec:
	MOV AH,02h      	   ; GotoXY (DL DH)
	INT 10h
	INC DH
	LODSB
	PUSH SI
	PUSH DX
	MOV AH,0	
    CALL DISPLAY_Byte_Dec
	POP DX
    POP SI

	DEC byte [PM_I]
	JNZ Loop_DISPLAY_Byte_List_Dec
	
	POP DX
	POP SI ; Restore SI
    RET ; DISPLAY_Byte_List_Dec End

; Display a byte in Decimal With "Off" if 0
; Input : 
;  DX : Coordinates
;  AL : Value to display
;  AH : 0 Don't display Off 1 Display Off
DISPLAY_Byte_Dec_xy:
 	CALL BIOS_SetCursorPosDX

DISPLAY_Byte_Dec:	
	MOV BL,BK_G
    CMP AL,0
    JNE DISPLAY_Byte_Dec_xy_NotOff
	CMP AH,0
	JE  DISPLAY_Byte_Dec_xy_NotOff
	MOV SI,STR_Off4
	CALL BIOS_Printstrc
	RET
DISPLAY_Byte_Dec_xy_NotOff:
	XOR AH,AH
	CALL BIOS_PrintAX_Dec ; Display a byte in Decimal
	RET ; DISPLAY_Byte_Dec_xy and DISPLAY_Byte_Dec End

DISPLAY_OnOff_xy:
	MOV BH,PAGE
	MOV BL,BK_G
	PUSH AX
	MOV AH,02h      	   ; GotoXY (DL DH)
	INT 10h
	POP AX
	CMP AL,0
	JE DispOff
	MOV SI,STR_On
	CALL BIOS_Printstrc
	RET
DispOff:
	MOV SI,STR_Off
	CALL BIOS_Printstrc
	RET  ;DISPLAY_OnOff_xy End

; Display an Image List
; Input :
; DX : 256*Y+X
; AL : Number of disk to Display
; AH : 0:MB 1:KB
; BL : Attribute
; DS:[SI] : Image list Ptr
DISPLAY_IMGList:
	CMP AL,0
	JNE DISPLAY_IMGListOk
	RET
DISPLAY_IMGListOk:

    MOV byte [PM_K],AH
	MOV byte [PM_I],AL
	MOV BH,PAGE
	
    PUSH CS
	POP DS

Loop_DISPLAY_IMGList:
	MOV AX,[SI]
	MOV [PM_J],AX   ; Save the disk size in PM_J
    ADD SI,2

; Display xxx Space
	PUSH DX
 	INC DX
	MOV CX,18
	CALL BIOS_SetCursorPosDX
	MOV AL,' '
	MOV BL,BL_G	
	CALL BIOS_Print_cx_char
	POP DX

 	CALL BIOS_SetCursorPosDX

    PUSH BX
	PUSH DX
	PUSH SI

	CMP word [PM_J],0FFFEh
	JNE DI_NoFolder_1
	MOV BL,BL_G
	MOV AL,'['
	CALL BIOS_Printchar	
DI_NoFolder_1:

    CMP byte[SI],0  ; Disk name Empty ?
	JNE Display_DiskNameNotEmpty
	
; Display 'None'    
 	CALL BIOS_SetCursorPosDX
	MOV SI,STR_None
    CALL BIOS_Printstrc
	
	JMP Display_DiskNameEmpty
	
Display_DiskNameNotEmpty:

    CALL BIOS_Printstrc ; Display the Disk name
	
	MOV AX,[PM_J]
    CMP AH,0FEh					; FEh means Real disk (Don't display size)
	JE Display_DiskNameEmpty
    CMP AX,0FFFFh
    JE 	Print_InvalidFSize
	CMP AX,0FFFEh
	JNE DI_NoFolder_2
	MOV AL,']'
	CALL BIOS_Printchar	
	JMP FileSizeOk
DI_NoFolder_2:

	ADD DL,14
 	CALL BIOS_SetCursorPosDX	
	CALL BIOS_PrintAX_Dec ; Display the Disk Size
    MOV SI,STR_MB
    CMP byte [PM_K],0
    JE DI_Mb
    MOV SI,STR_KB2
DI_Mb:	
    CALL BIOS_Printstrc   ; Display 'Mb'
    JMP FileSizeOk
Print_InvalidFSize:
	ADD DL,14
 	CALL BIOS_SetCursorPosDX
    printstr_w STR_InvalidSize
FileSizeOk:	

Display_DiskNameEmpty:
	
	POP SI
	POP DX    	; Restore the position
	POP BX		; Restore the Attribute
	
	INC DH
    ADD SI,14 ; Move to the next Entry
	DEC byte [PM_I]
	JNZ Loop_DISPLAY_IMGList

    RET  ;DISPLAY_IMGList End
	
%endif