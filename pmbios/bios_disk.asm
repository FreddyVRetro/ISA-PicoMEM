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
%ifndef BIOS_DISK
%assign BIOS_DISK 1

;----------------------------------------------------------------------------;
; BDA Equipment Flags (40:10H)
;----------------------------------------------------------------------------;
; 00      |			- LPT : # of LPT ports
;   x     |			- X1  :  unused, PS/2 internal modem
;    0    |			- GAM : Game port present
;     000 |			- COM : # of COM ports present
;        0| 		- DMA : DMA (should always be 0)
;         |00	 	- FLP : Floppy drives present (+1) (0=1 drive,1=2,etc)
;         |  00		- VIDM: Video mode (00=EGA/VGA, 01=CGA 40x25, 
; 				-	10=CGA 80x25, 11=MDA 80x25)
;         |    11 	- MBRAM: MB RAM (00=64K, 01=128K, 10=192K, 11=256K+)
;         |      0	- FPU : FPU installed
;         |       1	- IPL : Floppy drive(s) installed (always 1 on 5160)
;----------------------------------------------------------------------------;
%define BIOSVAR_EQUIP  010h  ; Equipment flag to read the number of Floppy
%define BIOSVAR_EQUIPH 011h  ; Equipment flag to read the number of Floppy
%define BIOSVAR_DISKNB 075h  ; Nb of HDD
%define BIOSVAR_MEMORY 013h  ; Word : Conventionnal Memory size in Kb (Supposed to be <=640)

; Get the number of HDD and the number of Floppy disk
BIOS_GetPCDriveInfos:
; Read the NB of Hard Drive
    MOV AX,040h
	MOV ES,AX
	MOV AL,[ES:BIOSVAR_DISKNB]
	MOV [PC_DiskNB],AL

; Read the BN of Floppy
;--- INT 11 
; EQUIPMENT DETERMINATION
;	BIT 7,6 = NUMBER OF DISKETTE DRIVES	:00=1, 01=2, 10-3, 11=4 ONLY IF BIT 0 = 1
;	BIT 0 = 0 No Floppy
    INT 11h  	; Get Equipment Flag
	TEST AL,1
	JZ NO_Floppy
    MOV CL,6
    SHR AL,CL
    AND AL,3	
	INC AL
	JMP Floppy_Detected
NO_Floppy:
    XOR AL,AL
Floppy_Detected:	
    MOV byte [PC_FloppyNB],AL
	
    RET

BIOS_MountFDD:
    MOV AH,CMD_FDD_Mount
	CALL PM_SendCMD           ; Do CMD_FDD_Mount Command
	CALL PM_WaitCMDEnd        ; Wait for the command End
	RET

;INT 13 - DISK - GET DRIVE PARAMETERS (PC,XT286,CONV,PS,ESDI,SCSI)
;	AH = 08h
;	DL = drive (bit 7 set for hard disk)
;	ES:DI = 0000h:0000h to guard against BIOS bugs
;Return: CF set on error
;	    AH = status (07h) (see #00234)
;	CF clear if successful
;	    AH = 00h
;	    AL = 00h on at least some BIOSes
;	    BL = drive type (AT/PS2 floppies only) (see #00242)
;	    CH = low eight bits of maximum cylinder number
;	    CL = maximum sector number (bits 5-0)
;		 high two bits of maximum cylinder number (bits 7-6)
;	    DH = maximum head number
;	    DL = number of drives
;	    ES:DI -> drive parameter table (floppies only)
; DL : Drive Index
BIOS_GetDriveParam:
	XOR AX,AX
	MOV ES,AX
	MOV DI,AX
	MOV AH,08h  ; Read Disk parameter
	INT 13h
	RET

;* BIOS_HookIRQ
;AX: IRQ Number
;BX: Old IRQ Vector save Offset  > CS:BX
;DX: New IRQ Offset
;Don't change DS
BIOS_HookIRQ:
	SHL AX,1
	SHL AX,1       ; IRQ vector Offset
	MOV DI,AX      
    XOR AX,AX
	MOV ES,AX      ; ES:DI Ptr to the IRQ Vector
	               ; CS:BX Ptr to the Vector Save
				   ; CS:DX New Vector
				   
    CLI ; Avoid an IRQ during the change
	CLD
	MOV AX,[ES:DI]   ;Save the Old vector
	MOV [CS:BX],AX
	MOV AX,[ES:DI+2]
	MOV [CS:BX+2],AX
	
	MOV AX,DX        ; New Vector CS:DX
	STOSW
	MOV AX,CS
	STOSW
	STI
    RET ;BIOS_HookIRQ End

;* BIOS_RestoreIRQ
;AX: IRQ Number
;BX: Old IRQ Vector save Offset  > CS:BX
BIOS_RestoreIRQ:

	SHL AX,1
	SHL AX,1     
	MOV DI,AX    ; IRQ vector Offset
    XOR AX,AX
	MOV ES,AX	 ; ES:DI Ptr to the IRQ Vector
	
	CLI
	CLD
	MOV AX,CS:[BX]
	STOSW
	MOV AX,CS:[BX+2]
	STOSW
	STI
    RET	;BIOS_RestoreIRQ End

%endif