
;***********************************************************************************************
;*                            IRQ 13h DISK BIOS Emulation and PicoMEM IRQ                      *
;*  																	                       *
;*  https://stanislavs.org/helppc/int_13.html                                                  *
;***********************************************************************************************

;AH = Command 02h Read 03h Write 05h format ....         Plus 60h for PicoMEM
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

    CMP AH,60h			    ; Fonction 60h is PicoMEM BIOS
	JNE Not_PMBIOS_Call
	CMP DX,1234h			; Accept to go to the BIOS only if BX=1234h
	JNE Int13_CallPrevIRQ

    POPF
	JMP PM_BIOS
	
Not_PMBIOS_Call:	

	PUSH BX	      ; Save BX (Only BX is used here) Warning : don't change another reg    

;CALL IRQ_DisplayRegs

	MOV BL,DL     ; Drive Number is in DL
	
; El Torito CD-ROM (firmware EDD services, bios_disk.cpp INT13_CD):
; 0E0h = the no-emulation CD drive, 7Fh = the AH=4Bh wildcard drive
; used by ELTORITO.SYS-style probes. Both fully served by the firmware.
	CMP BL,0E0h
	JE PM_Int13_MyDisk
	CMP BL,7Fh
	JE PM_Int13_MyDisk

	CMP BL,80h    ; No Floppy for the moment
	JAE Int13_CheckHDDNb
	CMP BL,01h
	JA Int13_Device_Notenabled
	XOR BH,BH
	CMP byte CS:[FDD0_Attribute+BX],80h   ; Check if the disk is enabled.
	JAE PM_Int13_MyDisk	                  ; Ok, Floppy found

; Floppy is not in the PicoMEM
	MOV DL,byte CS:[FDD0_Attribute+BX]    ; Change DL (New Disk number)

    JMP Int13_Device_Notenabled			    ; No emulated Floppy, JMP to legacy BIOS
Int13_CheckHDDNb:     
	CMP BL,83h
	JA Int13_Device_Notenabled
	XOR BH,BH
	CMP byte [CS:BX+HDD0_Attribute-80h],80h ; Check if the disk is enabled.
	JAE PM_Int13_MyDisk                     ; Ok, HDD found
; Disk is not in the PicoMEM
	MOV DL,byte CS:[BX+HDD0_Attribute-80h]
	ADD DL,80h								; Change DL : Disk number

Int13_Device_Notenabled:	; Call the Real BIOS Int13h
	POP BX

Int13_CallPrevIRQ:

	DEC byte CS:[Int13h_Flag]

    CMP AH,0
    JE Int13h_Legacy_Reset

	POPF
	JMP FAR [CS:OldInt13h]  ; Call the saved other IRQ13h !

; Tell the disk controller the "real" Nb of disks, to not have it try to reset more disk
Int13h_Legacy_Reset:
	PUSH ES
	PUSH AX
    MOV AX,040h
	MOV ES,AX
	MOV AL,CS:[PC_DiskNB]
    MOV ES:[BIOSVAR_DISKNB],AL		; Set the Nb of disk to the Original value
	POP AX
	POP ES    

    POPF
	PUSHF
	CALL FAR [CS:OldInt13h]  ; Call the saved other IRQ13h !

    PUSHF
	PUSH ES
	PUSH AX
    MOV AX,040h	
	MOV ES,AX
    MOV AL,CS:[New_DiskNB]
    MOV ES:[BIOSVAR_DISKNB],AL		; Set the Nb of disk as computed by the PicoMEM
	POP AX
	POP ES
    POPF
	
	RETF	2 			     ; return from int with current flags


; PicoMEM Disk BIOS (int13h Emulation)
PM_Int13_MyDisk:
	POP BX
    POPF    ; Restore the Flags

    CALL PM_SaveRegs       ; Send registers to the PicoMEM

    PUSH CS
	POP DS

%if IRQ_DEBUGPORT=1
    PUSH AX    ; DEBUG "Move pin for Logic analyzer"
	PUSH DX
	MOV DX,2A5h
	MOV AL,IRQ_IRQ5
	OUT DX,AL
	POP DX
	POP AX
%endif

    STI    ; Enable IRQ
    CMP byte [BV_Int13hCLI],0
	JE Int13_NoCLI		; If no RAM is emulated in PSRAM, no conflict possible (PSRAM/MicroSD)
    CLI    ; Disable IRQ to avoid PSRAM Conflict
Int13_NoCLI:

;    CLI

; Change Stack to BIOS RAM  > Not Needed, Disk fontion does not use the Stack
;	printstr_w STR_IRQ13

	CALL PM_WaitCMDEnd		  ; Check if previous command ended

    MOV AH,CMD_Int13h
	CALL PM_SendCMD           ; Do CMD_Int13h Command	(Don't wait previous command end)
	
	CALL BIOS_DoPCCMD              ; The PicoMEM Will send commands to the BIOS (Copy data Block, return)
Int13h_SkipCommand:

    ; The PicoMEM will wait for the PC command to end before ending its disk command
	
;    CALL PM_WaitCMDEnd_Timeout ; Wait that the command complete
;	 CALL PM_GetRegs_int13      ; Get the registers returned by the PicoMEM (Even the Flag)
; 	 CALL IRQ_DisplayData

%if IRQ_DEBUGPORT=1
    PUSH AX
	PUSH DX
	MOV DX,2A6h
	MOV AL,IRQ_IRQ5
	OUT DX,AL
	POP DX
	POP AX
%endif	

	DEC byte CS:[Int13h_Flag]
	CALL PM_GetRegs_int13     ; Get the registers returned by the PicoMEM (Even the Flag)

    STI  ; Restore IRQ
	RETF	2 			      ; return from int with current flags
	
; Int 13h End	