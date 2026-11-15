		
;***********************************************************************************************
;*                            IRQ 13h PicoMEM BIOS Functions                                   *
;*  																	                       *
;***********************************************************************************************

PM_BIOS:
    PUSH DS   		; Preserve DS

;push_all
;	CALL BIOS_PrintAL_Hex
;	printchar_w 'B'
;pop_all
	
	CMP AL,0
	JNE PMB_Check1
;PM BIOS Function 0 : Detect the PicoMEM BIOS and return config  > To use by PMEMM, PMMOUSE ...
; Input : AH=60h AL=00h
;         DX=1234h
; Return AX : Base Port
;        BX : BIOS Segment
;        CX : Available devices Bit Mask
;        DX : AA55h (Means Ok)
; Redirect the Picomem IRQ if not done
	PUSH DS
	PUSH ES
	PUSH SI
	PUSH DI
	CALL PM_InstallIRQ3_5_7	; Force the reinstall of the HW IRQ if not done
	POP DI
	POP SI
	POP ES
	POP DS

    XOR AX,AX
	CMP byte CS:[BV_PSRAMInit],0
	JNE PMB_NoPSRAM
	OR AL,00000001b
PMB_NoPSRAM:	
	CMP byte CS:[BV_SDInit],0
	JNE PMB_NoSD
	OR AL,00000010b
PMB_NoSD:
	CMP byte CS:[BV_USBInit],0
	JNE PMB_NoUSB
	OR AL,00000100b
PMB_NoUSB:
	CMP byte CS:[BV_WifiInit],0
	JNE PMB_NoWifi
	OR AL,00001000b
PMB_NoWifi:
	MOV AH,CS:[BV_IRQ]	; Return the IRQ number

	MOV CX,AX			; Return the available devices
	
	MOV AX,PM_BasePort	; Return the Base Port
	PUSH CS
	POP  BX				; Return the BIOS Segment

	MOV DX,0AA55h 		; Return the PicoMEM BIOS Signature

	JMP PMB_End
	
PMB_Check1:
	CMP AL,1
	JNE PMB_Check2
;PM BIOS Function 1 : Get RAM Config
; Return AH : PMRAM Enabled
;        AL : PSRAM Enable
;        BX :
;        CX : EMS Base Port (0 if Disabled) 
;        DX : EMS Address Segment

	MOV AH,CS:[PMCFG_PMRAM_Ext]
	MOV AL,CS:[PMCFG_PSRAM_Ext]
	XOR BX,BX
	MOV BL,CS:[PMCFG_EMS_Port]
	SHL BX,1
	MOV CX,CS:[EMS_Port_List+2+BX]
	MOV BL,CS:[PMCFG_EMS_Addr]
	SHL BX,1
	MOV DX,CS:[EMS_Addr_List+2+BX]
	MOV BX,0FFFFh

	JMP PMB_End
PMB_Check2:
	CMP AL,2
	JNE PMB_Check3
;PM BIOS Function 2 : Return the NE2000 / Wifi Connection Status
; Return AL : Connection Status
;        AH : IRQ
;        BX : Base Port (0 if not Present)
;        Connection status string returned in PCCR_Param
	CALL PM_GetWifiInfos

	MOV AH,byte CS:[PMCFG_WifiIRQ]
	MOV AL,byte CS:[BV_WifiInit]
    MOV BX,word CS:[PMCFG_ne2000Port]
	XOR CX,CX
	
	JMP PMB_End

PMB_Check3:
	CMP AL,3
	JNE PMB_Check4
;PM BIOS Function 3 : Return the PicoMEM Board ID and BIOS RAM Offset
; Added in Sept 2024
; Return AH : Pi Pico Board / chip ID
;        AL : PicoMEM Board ID
;        BX : Firmware Revision
;        CX : PCCR_Param (Offset of commands response)
;        DX : Reserved for the future

; Info : To detect if implemented, just check if BX is not changed after calling it.

    MOV AX,CS:[BV_BoardID]
	MOV BX,CS:[BV_FW_Rev]
	MOV CX,PCCR_Param
	MOV DX,0FFFFh
	JMP PMB_End	

PMB_Check4:
	CMP AL,4
	JNE PMB_Check05
	
;PM BIOS Function 4 : Get DFS infos
; Added in January 2024
; Return AL : DFS Code version  (Initial is 1)
;        CX : DFS buffer Offset
; Info : To detect if implemented, just check if BX is not changed after calling it.

    MOV AL,1
	MOV BX,PM_DISKB-60
	JMP PMB_End	
	
;PM BIOS Function 05h :  CALL the DFS fonction
PMB_Check05:
	CMP AL,05h
	JNE PMB_Check0F
;PM BIOS Function 05h :
    
;	STI
;    CMP byte [PMCFG_PSRAM_Ext],0
;	JE BIOS_0E_NoCLI		; If no RAM is emulated in PSRAM, no conflict possible (PSRAM/MicroSD)
    CLI    ; Test to avoid PSRAM Conflict
BIOS_0E_NoCLI:

; Change Stack to BIOS RAM  > Not Needed, Disk fonction does not use the Stack
	CALL PM_WaitCMDEnd		  ; Check if previous command ended

    MOV AH,CMD_EthDFS_Send
	CALL PM_SendCMD           ; Do CMD_Int13h Command	(Don't wait previous command end)

	CALL PM_WaitCMDEnd		  ; Check if previous command ended	

    STI  ; Restore IRQ
	
PMB_Check0F:
	CMP AL,0Fh
	JNE PMB_Check10
;PM BIOS Function 0Fh : Enable Int 21h and Int2Fh Redirection/Spy (Work if debug/Serial is enabled in the firmware)

;    CALL PM_InstallIRQ21h
    CALL PM_InstallIRQ2Fh
	
	JMP PMB_End	
PMB_Check10:
	CMP AL,10h
	JNE PMB_Check11

;PM BIOS Function 10h : Enable the Mouse
; Return AL : 0 Success or Error
	MOV AH,CMD_Mouse_Enable		; Ask to enable the Mouse
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end	
    
	PM_Read_DataL	; Read Data L to AL (Macro)
	
	JMP PMB_End

PMB_Check11:
	CMP AL,011h
	JNE PMB_Check12
;PM BIOS Function 11h : Disable the Mouse
; Return AL : 0 Success or Error

	MOV AH,CMD_Mouse_Disable	; Ask to disable the Mouse
	CALL PM_SendCMD
	CALL PM_WaitCMDEnd      ; !! WARNING, Crash if the Pico command does not end	
    
	PM_Read_DataL	; Read Data L to AL (Macro)
	
	JMP PMB_End

PMB_Check12:
	CMP AL,012h
	JNE PMB_Check13
;PM BIOS Function 12h : Reinstall the Keyboard interrupt

	CALL PM_InstallIRQ9

	JMP PMB_End

PMB_Check13:

; Fonction is not implemented

PMB_End:
	DEC byte CS:[Int13h_Flag]
	POP DS
	IRET