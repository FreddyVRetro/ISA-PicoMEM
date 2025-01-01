The PicoMEM BIOS redirect multiple Interrupt / BIOS Interrupt:
--------------------------------------------------------------

- PicoMEM Hardware IRQ : 3,5 or 7

The Hardware IRQ Code can be used to execute other Hardware IRQ Code (IRQ Multiplexer) or execute PicoMEM specific commands


- PicoMEM specific Interrupt fonctions (in IRQ 13):

;PM BIOS Function 0 : Detect the PicoMEM BIOS and return config  > To use by PMEMM, PMMOUSE ...
;                     Also redirect the Picomem Hardware IRQ if not done
; Input : AH=60h AL=00h
;         DX=1234h
; Return: AX : Base Port
;         BX : BIOS Segment
;         CX : Available devices Bit Mask
;             * Bit 0 : PSRAM Available
;			  * Bit 1 : uSD Available
;			  * Bit 2 : USB Host Enabled
;			  * Bit 3 : Wifi Enabled
;         DX : AA55h (Means Ok)

;PM BIOS Function 1 : Get RAM Config
; Input : AH=60h AL=01h
;         DX=1234h
; Return AH : PMRAM RAM Emulation Enabled (If 1)
;        AL : PSRAM RAM Emulation Enabled (If 1)
;        BX :
;        CX : EMS Base Port (0 if Disabled)
;        DX : EMS Address Segment

;PM BIOS Function 2 : Return the NE2000 / Wifi Connection Status
; Input : AH=60h AL=02h
;         DX=1234h
; Return AL : Connection Status
;        AH : IRQ
;        BX : Base Port (0 if not Present)

;PM BIOS Function 3 : Get various PicoMEM BIOS RAM offset
; Input : AH=60h AL=03h
;         DX=1234h
; Return AX : 
;        BX : Base Port (0 if not Present)

;PM BIOS Function 10h : Enable the Mouse
; Return AL : 0 Success or Error

;PM BIOS Function 11h : Disable the Mouse
; Return AL : 0 Success or Error

;PM BIOS Function 12h : Reinstall the Keyboard interrupt

PicoMEM Board BIOS Files list
-----------------------------

pm_cmd	       : The "PC" Commands API, when the Pi Pico send commands to the PC
bios_menu      : The Setup menu code
pm_vars.asm    : Define the variables in the "BIOS RAM"

trunc.exe      : To extend the compiled file size to a given value
romcksum32.exe : To perform the file Checksum
