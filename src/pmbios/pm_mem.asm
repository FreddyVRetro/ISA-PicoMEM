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

; PM_MEM Library for the Memory detection and configuration

%ifndef PM_MEM
%assign PM_MEM 1

%define MEM_NULL   0   ;// No PicoMEM emulation in this address
%define MEM_RAM    1   ;// in the Pico RAM          WS or not (if >>1) !! Change Disk code if modified !!
%define MEM_DISK   2   ;// Disk Buffer RAM          WS or not (if >>2)

%define MEM_BIOS   8   ;// PicoMEM BIOS and RAM		WS
%define MEM_ROM0   9   ;// ROM in Segment C			WS
%define MEM_ROM1   10  ;// ROM in Segment D			WS

%define MEM_PSRAM  16  ;// in the PSRAM				WS
%define MEM_EMS    17  ;// EMS in the PSRAM			WS

%define SMEM_RAM   32  ;// System RAM
%define SMEM_VID   33  ;// Video RAM
%define SMEM_ROM   34  ;// System ROM

; 1) PC Memory MAP is done at the First Boot
; If the PM Config file is loaded:
;   - Compare the Memory map with the saved config
;   - If different, reset the memory settings : Something changed
; If the PM Config file is not loaded
;   > Not Loaded because no SD Card or no file Reset the Memory settings.
; 2) 


; PMCFG_PM_MMAP : Table loaded from the config file and / or defined by the BIOS

SEG_D EQU 52
SEG_E EQU 56

STR_RAMNULL      DB " Error: BIOS RAM=0",0x0D,0x0A,0
STR_RAMDifferent DB " Warning : Different BIOS RAM ",0x0D,0x0A,0

PM_GetBIOSRAMSize:
; Read the system Memory Size    
	MOV AX,040h
	MOV ES,AX
	MOV AX,word ES:[BIOSVAR_MEMORY]
	CMP AX,0						; Check : Error if BIOS RAM = 0
	JNE BIOSRAMOk
	printstr_w STR_RAMNULL
	MOV AX,16  ; Minimum : 16Kb
	RET
BIOSRAMOk:
	MOV BX,CS:[PC_MEM]
	CMP BX,0
	JE FirstBIOSRAMRead
	CMP BX,AX
	JE FirstBIOSRAMRead
	PUSH AX
	printstr_w STR_RAMDifferent
	POP AX
	RET		; If Different, keep the first one
FirstBIOSRAMRead:	
	MOV CS:[PC_MEM],AX
	RET

; To Execute only during the first Boot. (The PC RAM Should not change)
PM_Memory_FirstBoot:
	
	CALL PM_GetBIOSRAMSize

; Build the initial PC Memory MAP
; Detect the memory above the BIOS Detected RAM	
; Warning : AX must be the PC RAM Size
    CALL PM_BuildPCMemoryMap

; Declare the PicoMEM BIOS in the PC Memory Map	(From CS)
	PUSH CS
	POP BX
%if DOS_COM=1
    MOV BX,0xC800
%endif
    MOV CL,10
	SHR BX,CL
	MOV AL,MEM_BIOS  ; 5, Const for BIOS
	MOV CS:[PMCFG_PC_MMAP+BX],AL    ; 2 Blocks used by the PicoMEM
	MOV CS:[PMCFG_PC_MMAP+1+BX],AL  ; 2 Blocks used by the PicoMEM

    MOV byte CS:[PMCFG_PC_MMAP+63],SMEM_ROM  ; There is always ROM in the last 16k

; Detect the 16K Blocks containing ROM
    CALL MEM_Test_ROM

%if DOS_COM=1
	XOR AX,AX
;	MOV CS:[PMCFG_PC_MMAP+32],AX ; Seg 8
;	MOV CS:[PMCFG_PC_MMAP+38],AX 
;	MOV CS:[PMCFG_PC_MMAP+36],AX ; Seg 9
; 	MOV CS:[PMCFG_PC_MMAP+38],AX 

	MOV CS:[PMCFG_PC_MMAP+40],AX ; Seg A
	MOV CS:[PMCFG_PC_MMAP+42],AX 
	MOV CS:[PMCFG_PC_MMAP+52],AX ; Seg D
	MOV CS:[PMCFG_PC_MMAP+54],AX ; Seg D

	MOV CS:[PMCFG_PC_MMAP+56],AX ; Seg E
	MOV CS:[PMCFG_PC_MMAP+58],AX ; Seg E
%endif	

    CALL PM_Check_EMS

	RET

; Input : AX : PC Memory size
PM_BuildPCMemoryMap:

; Disable the PSRAM emulated RAM if PSRAM not working / disabled
    CMP byte CS:[BV_PSRAMInit],0F0h	; Does nothing if the PSRAM is not enabled/working
	JB PM_BuildPCMemoryMap_PSRAMOk
	MOV byte CS:[PMCFG_PSRAM_Ext],0		; Force the PSRAM emulated RAM to be Off if PSRAM is failing/disabled
PM_BuildPCMemoryMap_PSRAMOk:


    ;Use the BIOS Memory size to fill the first segments
	MOV DI,PMCFG_PC_MMAP
	MOV CL,4
	SHR AX,CL  ; Number of 16Kb Memory Block (Nb of KB / 16)

	PUSH AX
    MOV CX,AX
	MOV AL,SMEM_RAM
	PUSH DS
	POP ES
	CLD
	REP STOSB
	POP AX
	
    CLI
	MOV DI,AX  ; DI is used as memory block index and counter
Test_RAM_Loop:
	MOV AX,DI
; Compute the 16Kb Memory Block begining address to DS:SI
	MOV CL,10
	SHL AX,CL
	MOV DS,AX
	XOR AX,AX
	MOV SI,AX

; Test is there is Memory in DS:SI
	MOV DX,DS:[SI]
    MOV word DS:[SI],0xAA55
    MOV BX,DS:[SI]
    MOV DS:[SI],DX
    CMP BX,0xAA55
    JNE CheckMEM_Ko
    MOV DX,DS:[SI]
    MOV word DS:[SI],0x55AA
    MOV BX,DS:[SI]
    MOV DS:[SI],DX
    CMP BX,0x55AA
    JNE CheckMEM_Ko
; This MEM Block is RAM	
    MOV AX,DI
	CMP AL,0xA*4
	JB CheckMEM_SysRAM
	CMP AL,0xC*4
	JAE CheckMEM_SysRAM
	MOV AL,SMEM_VID  ; Video RAM (If Segment A or B)
	JMP Check16KRAMOk	
CheckMEM_SysRAM:	
	MOV AL,SMEM_RAM  ; System RAM
	JMP Check16KRAMOk
CheckMEM_Ko:
; This Block is ROM or "Free"
    MOV AL,0
Check16KRAMOk:
    MOV CS:[PMCFG_PC_MMAP+DI],AL
	INC DI
	CMP DI,64      ; Test 64 16Kb Block in Total (1024Kb)
	JNE Test_RAM_Loop
	STI

	PUSH CS
	POP DS
	RET

;DOk DB 'Seg D Ok',0
;EOk DB 'Seg E Ok',0
;PSKO DB 'PSRAM Ko',0
; Check for the segment D and E
; If none are Free, "disable" EMS
PM_Check_EMS:
    MOV SI,PMCFG_PC_MMAP+SEG_D
    XOR AX,AX
	
	MOV BH,0
	CMP CS:[SI],AX
	JNE _SegD_NotFree
	INC SI
	INC SI
	CMP CS:[SI],AX
	JNE _SegD_NotFree
    MOV BL,1
	JMP _SegD_SetState
_SegD_NotFree:
    MOV BL,0
_SegD_SetState:
    MOV Byte CS:[Seg_D_NotUsed],BL

    MOV SI,PMCFG_PC_MMAP+SEG_E
    XOR AX,AX
	CMP CS:[SI],AX
	JNE _SegE_NotFree
	INC SI
	INC SI
	CMP CS:[SI],AX
	JNE _SegE_NotFree
    MOV BL,1
	JMP _SegE_SetState	
_SegE_NotFree:
    MOV BL,0
_SegE_SetState:	
    MOV Byte CS:[Seg_E_NotUsed],BL

	CMP Byte CS:[Seg_D_NotUsed],0
	JE _SegD_Is_Used
	MOV BH,1		; Seg D Not used : EMS Possible
;	PUSH BX
;	printstr_w DOk
;	POP BX
_SegD_Is_Used:
	CMP Byte CS:[Seg_E_NotUsed],0
	JE _SegE_Is_Used
	MOV BH,1		               ; Seg E Not used : EMS Possible
;	PUSH BX
;	printstr_w EOk
;	POP BX	
_SegE_Is_Used:

    CMP byte CS:[BV_PSRAMInit],0F0h
	JB PM_Check_EMS_PSRAMOK        ; BV_PSRAMInit Contains the PSRAM Size
	MOV BH,0					   ; PSRAM KO: EMS Impossible
;	PUSH BX
;	printstr_w PSKO
;	POP BX	
PM_Check_EMS_PSRAMOK:
    MOV byte CS:[EMS_Possible],BH
	CMP BH,1
	JE EMS_Is_Possible
	MOV byte CS:[PMCFG_EMS_Port],0	; Disable EMS (Port=0)
EMS_Is_Possible:

;EMS_Possible

	RET ;PM_Check_EMS

STR_MEMChanged DB 0x0D,0x0A,' Memory change : Reset the RAM Config.',0x0D,0x0A,0

; If the PicoMEM config file is loaded and there is a change, reset all.
MEM_Check_MEM_Changed:
    CMP byte CS:[BV_CFGInit],0
	JNE MEM_DoResetMEMCfg			; If there is no config file loaded, Initialize the Config

	XOR BX,BX
Check_MEM_Change_Loop:
	MOV AL,byte CS:[PMCFG_PC_MMAP+BX]
	CMP AL,SMEM_RAM		; ROM, System RAM or Video
	JAE MEM_DoCompare
    CMP AL,MEM_BIOS		; PM BIOS
	JE MEM_DoCompare

; Check if no system RAM/ROM/Video, but present in PicoMEM Config
	MOV AL,byte CS:[PMCFG_PM_MMAP+BX]
	CMP AL,MEM_BIOS
	JE MEM_Changed
	CMP AL,SMEM_RAM
	JAE MEM_Changed

	JMP MEM_Check_Next	
MEM_DoCompare:	
	CMP AL,byte CS:[PMCFG_PM_MMAP+BX]
	JNE MEM_Changed

MEM_Check_Next:
	INC BX
	CMP BX,63
	JNE Check_MEM_Change_Loop

 	CLC 	; Ok (Not changed)
	RET

MEM_Changed:
	printstr_w STR_MEMChanged
	
; DEBUG
;   PUSH CS
;   POP DS
;   MOV SI,PMCFG_PC_MMAP
;   MOV AX,4
;   CALL DISPLAY_MEM_Bytes
;   MOV SI,PMCFG_PM_MMAP
;   MOV AX,4
;   CALL DISPLAY_MEM_Bytes

	CALL MEM_DoResetMEMCfg

	STC
    RET ; MEM_Check_MEM_Changed End


; Copy the Detected PC Memory config to the PicoMEM Config
MEM_DoResetMEMCfg:
; Set the default memory settings
    MOV AL,0
    MOV byte CS:[PMCFG_EMS_Port],AL
	MOV byte CS:[PMCFG_PMRAM_Ext],AL
	MOV byte CS:[PMCFG_PSRAM_Ext],AL
	MOV byte CS:[PMCFG_Max_Conv],AL
	MOV byte CS:[PMCFG_IgnoreAB],1
MEM_Copy_PCtoPM:
    PUSH CS
	POP DS
	PUSH CS
	POP ES
	MOV AX,PMCFG_PC_MMAP   ; DS:SI > Detected PC Memory Map
	MOV SI,AX
	MOV AX,PMCFG_PM_MMAP   ; ES:DI > PicoMEM Memory Map
	MOV DI,AX
	MOV CX,32
	REP MOVSW			   ; Move 64 Bytes
	
 	CLC 	; Ok
	RET 
;MEM_DoResetMEMCfg End
	
; Detect the ROM / Not available 16Kb Block by testing if emulated RAM Work
MEM_Test_ROM:  ; Ok	

%if DOS_COM=1
	RET
%endif
   
	XOR BX,BX
Search_ROM_Loop:   
	CMP byte CS:[PMCFG_PC_MMAP+BX],0
	JNE _16KBlock_IsUsed

;Configure the Mem Block as RAM (16K Granularity)
	PUSH BX
	MOV AH,CMD_SetMEMType
	MOV BH,MEM_RAM
	CALL PM_SendCMD		; BL is the block number
	CALL PM_WaitCMDEnd
	POP BX
; Test the 16K Block if it is memory

	PUSH BX
	CALL Test16KFast
	POP BX
	
	JNC SearchROM_NoROM
	MOV byte CS:[PMCFG_PC_MMAP+BX],SMEM_ROM
SearchROM_NoROM:

    PUSH BX
	MOV AH,CMD_SetMEMType
	MOV BH,MEM_NULL
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd
    POP BX
	
_16KBlock_IsUsed:
    INC BL
	CMP BL,63
	JNE Search_ROM_Loop

	PUSH CS
	POP DS
   
	RET  
; PM_Test_ROM End

; ** Fast RAM Test (16Kb Bloc)
; Input  : BL: Block number
; Output : C Set if Failure
;          ! Change DS
Test16KFast:
    XOR BH,BH
	MOV CL,10
	SHL BX,CL
	MOV CX,4
	MOV DS,BX
	XOR SI,SI
	
Test16KFastLoop:	
    MOV byte DS:[SI],0xAA
	CMP byte DS:[SI],0xAA
	JNE Test16KFail
    MOV byte DS:[SI],0x00
	CMP byte DS:[SI],0x00
	JNE Test16KFail
	ADD SI,4096
	LOOP Test16KFastLoop

 	CLC 	; Ok
	RET

Test16KFail:
    STC
    RET  
;Test16KFast End

; ** MEM_Init : Create a new memory MAP from the detected PC Map and PicoMEM settings
; Input  : PM RAM, PSRAM and EMS Config
; Output : Nem Memory map in CS:PMCFG_PM_MMAP
MEM_Init:
    PUSH CS
	POP DS
		
; 1- Reinit the Memory Map
    CALL MEM_Copy_PCtoPM

; 2- Add the EMS Memory pages
	CMP byte [PMCFG_EMS_Port],0
	JE MEM_Init_NoEMS
	
	MOV BX,PMCFG_PM_MMAP+0x34		; Table index for Segment D (52)
	CMP byte [PMCFG_EMS_Addr],0
	JE EMS_SegIsD
    ADD BX,4                        ; EMS Seg is E
EMS_SegIsD:
	MOV AX,MEM_EMS+256*MEM_EMS
    MOV word [BX],AX
	MOV word [BX+2],AX
	
MEM_Init_NoEMS:

; Search for the memory Bottom
	MOV CX,64
	MOV SI,PMCFG_PM_MMAP
	XOR AX,AX
MEM_SearchBottomLoop:
	INC AH
    CMP byte CS:[PMCFG_IgnoreAB],1 	; If Ignore Video segment, don't add RAM there
	JNE MEM_SearchBottom_NoVideoIgnore
	CMP AH,41		;40: Begining of segment A (10*4)
	JB MEM_SearchBottom_NoVideoIgnore
	CMP AH,48
	JA MEM_SearchBottom_NoVideoIgnore
	INC SI	
	JMP MEM_SearchBottom_VideoIgnore
MEM_SearchBottom_NoVideoIgnore:
	LODSB
	CMP AL,0
	JE MEM_Bottom
MEM_SearchBottom_VideoIgnore:	
	LOOP MEM_SearchBottomLoop
	JMP MEM_NoMEMAvailable
MEM_Bottom:

; 3- Add the 128Kb PM Internal RAM zone
	CMP byte [PMCFG_PMRAM_Ext],0
	JE MEM_Init_NoPMRAM

; Configure MAX 128Kb as Internal Pico RAM emulated memory
	XOR CX,CX
    MOV CL,byte [PMCFG_MAX_PMRAM]	; 8*16Kb=128Kb Maximum, Sent by the Pico, via Cmakefile.ini
	CMP CX,0
	JE MEM_Init_NoPMRAM
	CMP CX,8 
	JA MEM_Init_NoPMRAM		; Avoid invalid value

MEM_FillPMRAM_Loop:
    PUSH SI
	CMP AL,0
	JNE MEM_FillPMRAM_Skip
	
    CMP byte CS:[PMCFG_IgnoreAB],1 	; If Ignore Video segment, don't add RAM there
    JNE FillPMRAM_NoSkipVideo
	MOV AX,SI
	SUB AX,PMCFG_PM_MMAP					   ; Compute the Memory table index to AL
	CMP AL,0xA*4
	JBE FillPMRAM_NoSkipVideo
	CMP AL,0xC*4
	JAE FillPMRAM_NoSkipVideo
	JMP MEM_FillPMRAM_Skip
FillPMRAM_NoSkipVideo:
	
    MOV byte [SI-1],MEM_RAM
MEM_FillPMRAM_Skip:	
    POP SI
	LODSB
	CMP SI,PMCFG_PM_MMAP+0x40
	JE MEM_Init_NoPSRAM     ; Stop if end is reached
	LOOP MEM_FillPMRAM_Loop
	
MEM_Init_NoPMRAM:

; 4- Fill all the free 16Kb zone with PSRAM emulated RAM
	CMP byte [PMCFG_PSRAM_Ext],0
	JE MEM_Init_NoPSRAM
	
MEM_FillPSRAM_Loop:
    PUSH SI
	CMP AL,0
	JNE MEM_FillPSRAM_Skip
	
    CMP byte CS:[PMCFG_IgnoreAB],1 ; If Ignore Video segment, don't add RAM there
    JNE FillPSRAM_NoSkipVideo
	MOV AX,SI
	SUB AX,PMCFG_PM_MMAP					   ; Compute the Memory table index to AL
	CMP AL,0xA*4
	JBE FillPSRAM_NoSkipVideo
	CMP AL,0xC*4
	JAE FillPSRAM_NoSkipVideo
	JMP MEM_FillPSRAM_Skip
FillPSRAM_NoSkipVideo:
	
    MOV byte [SI-1],MEM_PSRAM
MEM_FillPSRAM_Skip:	
	POP SI
	LODSB
	CMP SI,PMCFG_PM_MMAP+0x40
	JNE MEM_FillPSRAM_Loop     ; Stop if end is reached	
	
MEM_Init_NoPSRAM:

	RET ; MEM_Init
MEM_NoMEMAvailable:
; Should "never" happens to have no free Memory zone
	RET
; MEM_Init End

; ** MEM_GetMaxConv : Get the maximum conv Memory size
; Output : AX: Max Conv Memory
MEM_GetMaxConv:
    
	PUSH CS
	POP DS

    XOR BX,BX
	MOV CX,64
	MOV SI,PMCFG_PM_MMAP 	; DS:SI Memory Map
MEM_ConvSizeLoop:
	LODSB
	CMP AL,SMEM_RAM ;System RAM
	JE MEM_GetMaxConv_IsRAM
	CMP AL,MEM_RAM
	JE MEM_GetMaxConv_IsRAM
	CMP AL,MEM_PSRAM
	JE MEM_GetMaxConv_IsRAM
	JMP MEM_GetMaxConvEnd
MEM_GetMaxConv_IsRAM:
	ADD BX,16		; Add 16Kb of RAM
	LOOP MEM_ConvSizeLoop

MEM_GetMaxConvEnd:
    MOV AX,BX
	RET  
;MEM_Init End


;#define MEM_NULL   0   // No Memory in this address
;#define MEM_RAM    1   // in the Pico RAM          WS or not (if >>1)
;#define MEM_DISK   2   // Disk Buffer RAM          WS or not (if >>2)
;#define MEM_BIOS   8   // PicoMEM BIOS and RAM		WS
;#define MEM_ROM0   9   // ROM in Segment C			WS
;#define MEM_ROM1   10  // ROM in Segment D			WS
;#define MEM_PSRAM  16  // in the PSRAM				WS
;#define MEM_EMS    17  // EMS in the PSRAM			WS
;#define SMEM_RAM   32  // System RAM
;#define SMEM_VID   33  // Video RAM
;#define SMEM_ROM   34  // System ROM

; String for the Memory Display
MemTypeStr DB '-MB     B01     PE,'
PCMemTypeStr DB 'S','V','R'
; Input : AX : Screen position
;         SI : Memory table Offset (PMCFG_PM_MMAP)
DisplayMEMType:

	CALL BIOS_SetCursorPos
	MOV BL,113
	MOV CX,64
Display_MEMTypeLoop:
	PUSH CX
    CMP CX,24
	JNE DisplayMEM_NoSpace
; Insert one space (CX is changed as well)
	PUSH CX
    PUSH AX
	PUSH DX
	MOV AH,03h 					; Get position
	INT 10h	
	INC DL						; Go to the next column
	MOV AH,02h 					; Set position
	INT 10h
	POP DX
	POP AX
	POP CX
DisplayMEM_NoSpace:	
	XOR AH,AH
	LODSB
	MOV CH,64
	SUB CH,CL		; Compute the position
	MOV CL,0		; Not Video segment
    CMP byte CS:[PMCFG_IgnoreAB],1 ; If Ignore Video segment, replace - by x
    JNE DM_NoVideo
	CMP CH,0xA*4
	JB DM_NoVideo
	CMP CH,0xC*4
	JAE DM_NoVideo
	MOV CL,1		; Video segment
DM_NoVideo:
;PUSH BX
	XOR BH,BH
	MOV BL,AL
	CMP BL,SMEM_RAM
	JAE DM_IsPCMEM
	MOV AL,CS:[MemTypeStr+BX]  ; Load the char assigned to the value
    MOV BL,113
	JMP DM_Display
DM_IsPCMEM:	
	SUB BL,SMEM_RAM
	MOV AL,CS:[PCMemTypeStr+BX]  ; Load the char assigned to the value
	MOV BL,112	
DM_Display:	
;    POP BX
	CMP CL,0
	JE DM_NoVideo2
	CMP AL,'-'
	JNE DM_NoVideo2
	MOV AL,'x'       ; replace - by x
DM_NoVideo2:
	PUSH SI
	CALL BIOS_Printchar	
	POP SI
	POP CX
    LOOP Display_MEMTypeLoop
    RET	
; DisplayMEMType End	

STR_ERAM DB ' ',254,' Emulated Memory',0

STR_Conv  DB ' - Conv ',0
STR_High  DB ' - High ',0
STR_EMS   DB ' - EMS Port: ',0
STR_EMS_Addr DB '@ ',0
STR_NONE2 DB ' - None',0

; Use [PM_I]
DisplayMEMConfig:

	PUSH CS
	POP DS
	
    MOV byte [PM_K],0
    printstr_w STR_ERAM
	
    XOR BX,BX
	XOR AX,AX
    MOV SI,PMCFG_PM_MMAP
	MOV CX,40
Count_ConvMEM:
	LODSB
	CMP AL,MEM_RAM
	JE Count_ConvFound
	CMP AL,MEM_PSRAM	
	JE Count_ConvFound
	JMP Count_ConvDoLoop
Count_ConvFound:
    INC BL
Count_ConvDoLoop:	
    LOOP Count_ConvMEM

    CMP BL,0
	JE No_Conv

    MOV byte [PM_K],1
    PUSH SI
	MOV AL,BL
	MOV CL,4
	SHL BX,CL  ; Nb of 16K Block *16 = Size
	PUSH BX
    printstr_w STR_Conv
    POP AX
	MOV BL,7    ; Light Grey	
	CALL BIOS_PrintAX_Dec
    printstr_w STR_KB
	POP SI
No_Conv:

    XOR BX,BX
	XOR AX,AX    
	MOV CX,24
Count_HighMEM:

	LODSB
	CMP AL,MEM_RAM
	JE Count_HighFound
	CMP AL,MEM_PSRAM	
	JE Count_HighFound
    JMP Count_HighDoLoop
Count_HighFound:
    INC BL
Count_HighDoLoop:	
    LOOP Count_HighMEM

    CMP BL,0
	JE No_High

    MOV byte [PM_K],1
    PUSH SI
	MOV AL,BL
	MOV CL,4
	SHL BX,CL  ; Nb of 16K Block *16 = Size
	PUSH BX
    printstr_w STR_High
    POP AX
	MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Dec
    printstr_w STR_KB
	POP SI	

No_High:

    CMP byte CS:[PMCFG_EMS_Port],0
	JE No_EMS_Display

    printstr_w STR_EMS    

    XOR BX,BX
	MOV BL,byte CS:[PMCFG_EMS_Port]
	SHL BX,1
	MOV AX,CS:[EMS_Port_List+2+BX]
	MOV BL,7    ; Light Grey
	CALL BIOS_PrintAX_Hex
    
	MOV byte [PM_K],1	

No_EMS_Display:

    CMP byte [PM_K],1
	JE DisplayMEMConfig_NotNone

	printstr_w STR_NONE2		; Display "None" if no emulated RAM is present

DisplayMEMConfig_NotNone:

    printstr_w STR_CRLF
	
    RET	
%endif