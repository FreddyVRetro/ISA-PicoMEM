; Cute Mouse Driver - a tiny mouse driver
;
; Copyright (c) 1997-2002 Nagy Daniel <nagyd@users.sourceforge.net>
;
; BIOS wheel mouse support: Ported from Konstantin Koll's public
; domain code into Cute Mouse by Eric Auer 2007 (places marked -X-)
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA
; -X- no longer: 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
;

; NOTE: memcopy and MOVSEG_ have "assume..." as side effects!
; %pagesize 255
; %noincl
;%macs
; %nosyms
;%depth 0
; %linum 0
;%pcnt 0
;%bin 0
; warn
; locals

; PicoMEM Version defines
; Int 33h AX=0060h > Update Mouse informations function
; Call mouseupdate directly

PICOMEM = 1
PM_KeepALL = 1


CTMVER		equ <"2.1">		; major driver version
if PICOMEM
CTMRELEASE	equ <"2.1b4 for PicoMEM">	; full driver version with suffixes
else
CTMRELEASE	equ <"2.1b4">	; full driver version with suffixes
endif
driverversion	equ 705h		; imitated Microsoft driver version
; at least 705h because our int 33 function _26 operates in 7.05+ style

FASTER_CODE	 = 0		; optimize by speed instead size
OVERFLOW_PROTECT = 0	; prevent variables overflow
FOOLPROOF	 = 1		; check driver arguments validness
USE28		 = 0		; include code for INT 33/0028 function
USERIL       = 0		; include code for INT 10/Fn EGA functions

; %define PS2DEBUG 1		; print debug messages for PS2serv calls

;------------------------------------------------------------------------

include asmlib/asm.mac
; *** include asmlib/hll.mac	; if_ loop_ countloop_ etc - TASM specific
include asmlib/code.def
include asmlib/code.mac
include asmlib/macro.mac
include asmlib/BIOS/area0.def	; ** int11 flags and various 40:xx gfx info
; HW_PS2	equ 4	; bit set in int 11h returned AX if PS/2 mouse present
; VIDEO_control VIDEO_switches VIDEO_width VIDEO_pageoff VIDEO_mode
; VIDEO_switches VIDSW_feature0 VIDSW_display VIDEO_ptrtable@
; VPARAM_SEQC VIDEO_paramtbl@ VIDEO_lastrow VIDEO_pageno
include asmlib/convert/digit.mac	; ** only count2x uses digits in ASCII
include asmlib/convert/count2x.mac	; ** only used as _word_hex for I/O port
include asmlib/DOS/MCB.def	; ** small, used to set ownerid and name
include asmlib/DOS/PSP.def	; ** only DOS_exit env_seg cmdline_len PSP_TSR used
; include asmlib/DOS/file.mac	; was only used once - DOSCloseFile
include asmlib/DOS/mem.mac
; include asmlib/hard/PIC8259A.def	; only PIC1_OCW2, PIC1_IMR const used:
PIC1_OCW2	equ 20h
PIC1_IMR	equ 21h
include asmlib/hard/UART.def

USE_286		equ <(@CPU and 4)>
USE_386		equ <(@CPU and 8)>

_ARG_DI_	equ <word ptr [bp]>
_ARG_SI_	equ <word ptr [bp+2]>
_ARG_BP_	equ <word ptr [bp+4]>

if USE_286

_ARG_BX_	equ <word ptr [bp+8]>
_ARG_DX_	equ <word ptr [bp+10]>
_ARG_CX_	equ <word ptr [bp+12]>
_ARG_AX_	equ <word ptr [bp+14]>
_ARG_ES_	equ <word ptr [bp+16]>
_ARG_DS_	equ <word ptr [bp+18]>
; arg offset: far pointer plus 8+2 words (push 2 segs, pusha)
_ARG_OFFS_	=   24

PUSHALL		equ <pusha>
POPALL		equ <popa>

else ; USE_286

_ARG_BX_	equ <word ptr [bp+6]>
_ARG_DX_	equ <word ptr [bp+8]>
_ARG_CX_	equ <word ptr [bp+10]>
_ARG_AX_	equ <word ptr [bp+12]>
_ARG_ES_	equ <word ptr [bp+14]>
_ARG_DS_	equ <word ptr [bp+16]>
_ARG_OFFS_	=   22

PUSHALL		macro
		push	ax
		push	cx
		push	dx
		push	bx
		push	bp
		push	si
		push	di
endm
POPALL		macro
		pop	di
		pop	si
		pop	bp
		pop	bx
		pop	dx
		pop	cx
		pop	ax
endm

endif ; USE_286

nl		equ <13,10>
eos		equ <0>

POINT		struc
  X		dw ?
  Y		dw ?
POINT ends

PS2serv		macro	serv:req,errlabel ; :vararg
		mov	ax,serv
ifdef PS2DEBUG
	push ax
	mov al,'<'
	int 29h
	pop ax
	push ax
	and al,15	; assume ax is c20n, only show low nibble
	add al,30h	; convert to digit
	int 29h
	mov al,'/'
	int 29h
	mov al,bh	; low nibble of bh is interesting, too
	add al,30h	; (func 7: es bx is pointer)
	int 29h
	pop ax
endif ; ifdef PS2DEBUG

		int	15h

ifdef PS2DEBUG
	pushf
	push ax
	mov al,20h	; space
	adc al,0	; if carry, exclamation mark
	int 29h
	pop ax
	push ax
	mov al,ah
	add al,30h	; usually ah is only 0..9 status
	int 29h
	mov al,'/'
	int 29h
	mov al,bh	; func 1, 4: device id (1: bl is aa if okay)
	add al,30h	; can be ff+30 or aa+30 now :-p
	int 29h
	mov al,'>'
	int 29h
	pop ax
	popf
endif ; ifdef PS2DEBUG

	ifnb <errlabel>
		jc	errlabel
		test	ah,ah
		jnz	errlabel
	endif
endm


;========================== SEGMENTS DEFINITION ==========================

.model tiny
assume ss:nothing

@TSRcode equ <DGROUP>
@TSRdata equ <DGROUP>
; TSR ends at TSRend, roughly line 3000 of the 4000 lines of ctmouse

TSRcref	equ <offset @TSRcode>	; offset relative TSR code group
TSRdref	equ <offset @TSRdata>	;	- " -	      data
coderef	equ <offset @code>	; offset relative main code group
dataref	equ <offset @data>	;	- " -	       data

.code
		org	0
TSRstart	label byte
		org	100h		; .COM style program
start:		jmp	real_start
TSRavail	label byte			; initialized data may come from here


;========================== UNINITIALIZED DATA ==========================

;!!! WARNING: don't init variables in uninitialized section because
;		they will not be present in the executable image

		org	PSP_TSR-3*16	; reuse part of PSP
spritebuf	db	3*16 dup (?)	; copy of screen sprite in modes 4-6

;----- application state -----

;!!! WARNING: variables order between RedefArea and szDefArea must be
;		syncronized with variables order after DefArea

		even
SaveArea = $
RedefArea = $

mickey8		POINT	<>		; mickeys per 8 pixel ratios
;;*doublespeed	dw	?		; double-speed threshold (mickeys/sec)
senscoeff	POINT	<>		; mickeys sensitivity, ~[1/3..3.0]*256
sensval		POINT	<>		; original 001A sensitivity values-1
startscan	dw	?		; screen mask/cursor start scanline
endscan		dw	?		; cursor mask/cursor end scanline

;----- hotspot, screenmask and cursormask must follow as is -----

hotspot		POINT	<>		; cursor bitmap hot spot
screenmask	db	2*16 dup (?)	; user defined screen mask
cursormask	db	2*16 dup (?)	; user defined cursor mask
nocursorcnt	db	?		; 0=cursor enabled, else hide counter
;;*nolightpen?	db	?		; 0=emulate light pen
		even
szDefArea = $ - RedefArea		; initialized by softreset_21

rangemax	POINT	<>		; horizontal/vertical range max
upleft		POINT	<>		; upper left of update region
lowright	POINT	<>		; lower right of update region
pos		POINT	<>		; virtual cursor position
granpos		POINT	<>		; granulated virtual cursor position
UIR@		dd	?		; user interrupt routine address

		even
ClearArea = $

sensround	POINT	<>		; rounding error in applying
					;  sensitivity for mickeys
rounderr	POINT	<>		; same in conversion mickeys to pixels
		even
szClearArea1 = $ - ClearArea		; cleared by setpos_04

rangemin	POINT	<>		; horizontal/vertical range min
		even
szClearArea2 = $ - ClearArea		; cleared by setupvideo

cursortype	db	?		; 0 - software, else hardware
callmask	db	?		; user interrupt routine call mask
mickeys		POINT	<>		; mouse move since last access
BUTTLASTSTATE	struc
  counter	dw	?
  lastrow	dw	?
  lastcol	dw	?
BUTTLASTSTATE ends
buttpress	BUTTLASTSTATE <>
buttrelease	BUTTLASTSTATE <>
wheel		BUTTLASTSTATE <>		; wheel counter since last access
wheelUIR	db	?		; wheel counter for UIR
		even
szClearArea3 = $ - ClearArea		; cleared by softreset_21
szSaveArea = $ - SaveArea

if USERIL					; -X-
;----- registers values for RIL -----

;!!! WARNING: registers order and RGROUPDEF contents must be fixed

		even
VRegsArea = $

regs_SEQC	db	5 dup (?)
reg_MISC	db	?
regs_CRTC	db	25 dup (?)
regs_ATC	db	21 dup (?)
regs_GRC	db	9 dup (?)
reg_FC		db	?
reg_GPOS1	db	?
reg_GPOS2	db	?

DefVRegsArea = $

def_SEQC	db	5 dup (?)
def_MISC	db	?
def_CRTC	db	25 dup (?)
def_ATC		db	21 dup (?)
def_GRC		db	9 dup (?)
def_FC		db	?
def_GPOS1	db	?
def_GPOS2	db	?

szVRegsArea = $ - DefVRegsArea

; ERRIF (szVRegsArea ne 64 or $-VRegsArea ne 2*64) "VRegs area contents corrupted!"
else
crtc		 dw ?			; 3d4 or 3b4
endif					; -X- USERIL

;----- old interrupt vectors -----

oldint33	dd	?		; old INT 33 handler address
oldIRQaddr	dd	?		; old IRQ handler address


;=========================== INITIALIZED DATA ===========================

		even
TSRdata		label byte

if (TSRdata lt TSRavail)
	;echo TSR uninitialized data area too small ( No error ).
	ORG TSRavail
endif

DefArea = $
		POINT	<8,16>			; mickey8
		POINT	<1*256,1*256>		; senscoeff
		POINT	<49,49>			; sensval
;;*		dw	64			; doublespeed
		dw	77FFh,7700h		; startscan, endscan
		POINT	<0,0>			; hotspot
		dw	0011111111111111b	; screenmask
		dw	0001111111111111b
		dw	0000111111111111b
		dw	0000011111111111b
		dw	0000001111111111b
		dw	0000000111111111b
		dw	0000000011111111b
		dw	0000000001111111b
		dw	0000000000111111b
		dw	0000000000011111b
		dw	0000000111111111b
		dw	0000000011111111b
		dw	0011000011111111b
		dw	1111100001111111b
		dw	1111100001111111b
		dw	1111110011111111b
		dw	0000000000000000b	; cursormask
		dw	0100000000000000b
		dw	0110000000000000b
		dw	0111000000000000b
		dw	0111100000000000b
		dw	0111110000000000b
		dw	0111111000000000b
		dw	0111111100000000b
		dw	0111111110000000b
		dw	0111110000000000b
		dw	0110110000000000b
		dw	0100011000000000b
		dw	0000011000000000b
		dw	0000001100000000b
		dw	0000001100000000b
		dw	0000000000000000b
		db	1			; nocursorcnt
;;*		db	0			; nolightpen?
	db 0	; JWASM would use 0fch, "CLD" as pad byte??
		even
; ERRIF ($-DefArea ne szDefArea) "Defaults area contents corrupted!"

;----- driver and video state begins here -----

		even
granumask	POINT	<-1,-1>

textbuf		label	word
buffer@		dd	?		; pointer to screen sprite copy
cursor@		dw	-1,0		; cursor sprite offset in videoseg;
					; -1=cursor not drawn
videoseg	equ <cursor@[2]>	; 0=not supported video mode

UIRunlock	db	1		; 0=user intr routine is in progress
videolock	db	1		; drawing: 1=ready,0=busy,-1=busy+queue
newcursor	db	0		; 1=force cursor redraw

;----- table of pointers to registers values for RIL -----

REGSET		struc
  rgroup	dw	?
  regnum	db	?
  regval	db	?
REGSET ends

	db 0	; JWASM would use 0fch, "CLD" as pad byte??
		even
		dw	(vdata1end-vdata1)/(size REGSET)
vdata1		REGSET	<10h,1>,<10h,3>,<10h,4>,<10h,5>,<10h,8>,<08h,2>
vdata1end	label word
		dw	(vdata2end-vdata2)/(size REGSET)
vdata2		REGSET	<10h,1,0>,<10h,4,0>,<10h,5,1>,<10h,8,0FFh>,<08h,2,0Fh>
vdata2end	label byte

RGROUPDEF	struc
  port@		dw	?
  regs@		dw	?
  def@		dw	?
  regscnt	db	1
  rmodify?	db	0
RGROUPDEF ends

if USERIL					; -X-
		even
videoregs@	label RGROUPDEF
	RGROUPDEF <3D4h,regs_CRTC,def_CRTC,25>	; CRTC
	RGROUPDEF <3C4h,regs_SEQC,def_SEQC,5>	; Sequencer
	RGROUPDEF <3CEh,regs_GRC, def_GRC, 9>	; Graphics controller
	RGROUPDEF <3C0h,regs_ATC, def_ATC, 20>	; VGA attrib controller
	RGROUPDEF <3C2h,reg_MISC, def_MISC>	; VGA misc output and input
	RGROUPDEF <3DAh,reg_FC,   def_FC>	; Feature Control
	RGROUPDEF <3CCh,reg_GPOS1,def_GPOS1>	; Graphics 1 Position
	RGROUPDEF <3CAh,reg_GPOS2,def_GPOS2>	; Graphics 2 Position
endif						; -X- USERIL


;============================= IRQ HANDLERS =============================

;========================================================================

IRQhandler	proc
		assume	ds:nothing,es:nothing
		cld
		push ds
		push es
		PUSHALL
		MOVSEG	ds,cs,,@TSRdata
;	CODE_	MOV_CX	IOdone,<db ?,0>		; processed bytes counter
		OPCODE_MOV_CX
IOdone		db ?,0
; -X- IRQproc	label	byte			; "mov al,OCW2<OCW2_EOI>"
; -X-		j	PS2proc			;  if serial mode
; -X-		out	PIC1_OCW2,al		; {20h} end of interrupt
;		out_	PIC1_OCW2,%OCW2<OCW2_EOI>
		mov	al,20h
		out	PIC1_OCW2,al

;	CODE_	MOV_DX	IO_address,<dw ?>	; UART IO address
		OPCODE_MOV_DX
IO_address	dw ?
		push	dx
		movidx	dx,LSR_index
		 in	al,dx			; {3FDh} LSR: get status
		xchg	bx,ax			; OPTIMIZE: instead MOV BL,AL
		pop	dx
		movidx	dx,RBR_index
		 in	al,dx			; {3F8h} flush receive buffer

		test	bl,mask LSR_break+mask LSR_FE+mask LSR_OE
;	if_ nz					; if break/framing/overrun
	jz @@irqhandlerz
		xor	cx,cx			;  errors then restart
		mov	[IOdone],cl		;  sequence: clear counter
;	end_
@@irqhandlerz:
		shr	bl,LSR_RBF+1
;	if_ carry				; process data if data ready
	jnc @@irqhandlernc
		call_	mouseproc,MSMproc	; never PS/2
;	end_
@@irqhandlernc:
		jmp	rethandler
IRQhandler	endp
		assume	ds:@TSRdata

;========================================================================
;				Enable PS/2
;========================================================================
;
; In:	none
; Out:	none
; Use:	none
; Modf:	AX, BX, DX, ES
; Call:	INT 15/C2xx, -X- setRateTSR
; -X- @disablePS2, INT 21/25 --> no longer (re)hooks IRQ
;
enablePS2	proc
; -X-		call	@disablePS2		; unhooked IRQ
		MOVSEG	es,cs,,@TSRcode
		mov	bx,TSRcref:PS2handler	; -X- real handler now
		PS2serv	0C207h			; set mouse handler in ES:BX
		mov	bh,1
		PS2serv	0C200h			; set mouse on (bh=1)
; -X-		DOSSetIntr 68h+12,,,@TSRcode:IRQhandler
		mov	bh,5			; -X- 100, KoKo uses 3 (60)
		call	setRateTSR		; -X-
		ret
; -X- PS2dummy:       retf
enablePS2	endp

;========================================================================
;				Disable PS/2
;========================================================================
;
; In:	none
; Out:	none
; Use:	oldIRQaddr
; Modf:	AX, BX, DX, ES
; Call:	INT 15/C2xx, -X- setRateTSR
; -X- INT 21/25 --> no longer unhooks IRQ  (irq 12, int 74)
;
disablePS2	proc
@disablePS2:	xor	bx,bx
		PS2serv	0C200h			; set mouse off (bh=0)
		MOVSEG	es,bx,,nothing
		PS2serv	0C207h			; clear mouse handler (ES:BX=0)
		mov	bh,5			; -X- KoKo uses 15/C201 here
		call	setRateTSR		; -X-

		ret
disablePS2	endp
		assume	ds:@TSRdata		; added...?

;========================================================================

; -X- A "light" version for the TSR... In: BH=0..6 for 20,40,60,80,100,200
; Set mouse sampling rate to "RATES[BH]" samples per second
setRateTSR	proc
		PS2serv	0C202h			; set rate, ignore errors
		ret
setRateTSR	endp

;========================================================================

; -X- this handler is called by the BIOS, it is no IRQ handler :-)
PS2handler	proc
		assume	ds:nothing,es:nothing
		push	ds
		push es
		PUSHALL
		MOVSEG	ds,cs,,@TSRdata

		mov	bp,sp

PS2WHEELCODE	label	byte		; jump to wheel or plain: test 0/-1
;		test	sp,0		; 2 byte opcode, sp always NZ here
;		jnz	PS2WHEEL
	j	@@PS2PLAIN
		; stack for non-wheel mice: ... - - Y - X - BTN -
		; stack for wheel mice:     ... - - W - Y - BTN X
		; flags: (yext) (xext) ysign xsign 1 btn3 btn1 btn2
		; ("ext" flag could be used to trigger "xor value,100h")
@@PS2PLAIN:	; old cutemouse 1.9 PS/2 handler, non-wheel
		; note: ctmouse 1.9 code uses only sign, not ext
		mov	al,[bp+_ARG_OFFS_+6]	; buttons and flags
if USE_286
		mov	bl,al			; backup, xchg will restore
		shl	al,3			; CF=Y sign bit, MSB=X sign
else
		mov	cl,3
		mov	bl,al			; backup, xchg will restore
		shl	al,cl			; CF=Y sign bit, MSB=X sign
endif
;		sbb	ch,ch			; extend Y sign bit
	db 1ah, 0edh	; JWASM and TASM use opposite encoding
		cbw				; extend X sign bit
		mov	al,[bp+_ARG_OFFS_+4]	; AX=X movement
		xchg	bx,ax			; X to BX, buttons to AL
		mov	cl,[bp+_ARG_OFFS_+2]	; CX=Y movement
		; wheelmask is 0 now, so no wheel data is expected in AH
		j	@@PS2DONE

		; stack for non-wheel mice: ... - - Y - X - BTN -
		; stack for wheel mice:     ... - - W - Y - BTN X
		; flags: (yext) (xext) ysign xsign 1 btn3 btn1 btn2
		; "ext" flag can be used to trigger "xor value,100h"
PS2WHEEL::	; handler based on public domain code from Konstantin Koll
		; old KoKo code used only ext, not sign, ok on all but Alps
		mov	al,[bp+_ARG_OFFS_+6]	; buttons and flags
if USE_286
		mov	bl,al			; backup, xchg will restore
		shl	al,3			; CF=Y sign bit, MSB=X sign
else
		mov	cl,3
		mov	bl,al			; backup, xchg will restore
		shl	al,cl			; CF=Y sign bit, MSB=X sign
endif
;		sbb	ch,ch			; extend Y sign bit
	db 1ah, 0edh	; JWASM and TASM use opposite encoding
		cbw				; extend X sign bit
		mov	al,[bp+_ARG_OFFS_+7]	; AX=X movement <--
		xchg	bx,ax			; X to BX, buttons to AL
		mov	cl,[bp+_ARG_OFFS_+4]	; CX=Y movement <--
		mov	ah,[bp+_ARG_OFFS_+2]	; AH=Wheel data <--
		; reverseY does not seem to support 9th bit, must skip that

@@PS2DONE:	call	reverseY		; AL flags AH wheel BX X CX Y
		POPALL
		pop	es
		pop	ds
		retf
PS2handler		endp
		assume	ds:@TSRdata		; added...?


;========================================================================
;			Enable serial interrupt in PIC
;========================================================================
;
; In:	none
; Out:	none
; Use:	IO_address
; Modf:	AX, DX, SI, IOdone, MSLTbuttons
; Call:	INT 21/25
;
enableUART	proc

;----- set new IRQ handler

		mov	dx,TSRcref:IRQhandler	; -X- IRQintnum 0 means none
;	CODE_	MOV_AX	IRQintnum,<db 0,25h>	; INT number of selected IRQ
		OPCODE_MOV_AX
IRQintnum	db 0,25h
		int	21h			; set INT in DS:DX

;----- set communication parameters

		mov	si,[IO_address]
		movidx	dx,LCR_index,si
;		 out_	dx,%LCR{LCR_DLAB=1}	; {3FBh} LCR: DLAB on
;		mov al,%LCR{LCR_DLAB=1}	; {3FBh} LCR: DLAB on
		mov 	al,80h
		out	dx,al
;		xchg	dx,si			; 1200 baud rate
	xchg si,dx	; JWASM and TASM use opposite encoding
		mov	ax,96
;		 outw	dx,96			; {3F8h},{3F9h} divisor latch
		out	dx,ax
;		xchg	dx,si
	xchg si,dx	; JWASM and TASM use opposite encoding
;	CODE_	 MOV_AX
		OPCODE_MOV_AX
LCRset	db 00000010b	; LCR	<0,,LCR_noparity,0,2>	; {3FBh} LCR: DLAB off, 7/8N1
	db 00001111b	; MCR	<,,,1,1,1,1>		; {3FCh} MCR: DTR/RTS/OUTx on
		 out	dx,ax

;----- prepare UART for interrupts

		movidx	dx,RBR_index,si,LCR_index
		 in	al,dx			; {3F8h} flush receive buffer
		movidx	dx,IER_index,si,RBR_index
;		 out_	dx,%IER{IER_DR=1},%FCR<>; {3F9h} IER: enable DR intr
						; {3FAh} FCR: disable FIFO
;		mov	al,%IER{IER_DR=1}
;		mov	ah,%FCR<>
		mov ax,1
		out	dx,ax
		dec	ax			; OPTIMIZE: instead MOV AL,0
		mov	[IOdone],al
		mov	[MSLTbuttons],al

;-----

		in	al,PIC1_IMR		; {21h} get IMR
;	CODE_	AND_AL	notPIC1state,<db ?>	; clear bit to enable interrupt
		OPCODE_AND_AL
notPIC1state	db ?
		out	PIC1_IMR,al		; {21h} enable serial interrupts
		ret
enableUART	endp

;========================================================================
;			Disable serial interrupt of PIC
;========================================================================
;
; In:	none
; Out:	none
; Use:	IO_address, oldIRQaddr
; Modf:	AX, DX
; Call:	INT 21/25
;
disableUART	proc
		in	al,PIC1_IMR		; {21h} get IMR
;	CODE_	OR_AL	PIC1state,<db ?>	; set bit to disable interrupt
		OPCODE_OR_AL
PIC1state	db ?
		out	PIC1_IMR,al		; {21h} disable serial interrupts

;-----
;!!! MS/Logitech ID bytes (4Dh)+PnP data looks as valid mouse packet, which
;	moves mouse; to prevent unnecessary send of PnP data after disabling
;	driver with following enabling, below DTR and RTS remained active

		movidx	dx,LCR_index,[IO_address] ; {3FBh} LCR: DLAB off
;		 out_	dx,%LCR<>,%MCR<,,,0,,1,1> ; {3FCh} MCR: DTR/RTS on, OUT2 off
;		mov	al,%LCR<>
;		mov	ah,%MCR<,,,0,,1,1>
	mov ax,300h
		out	dx,ax
		movidx	dx,IER_index,,LCR_index
		 ;mov	al,IER<>
		 out	dx,al			; {3F9h} IER: interrupts off

;----- restore old IRQ handler

		push	ds
		mov	ax,word ptr [IRQintnum]	; AH=25h
		lds	dx,[oldIRQaddr]
		assume	ds:nothing
		int	21h			; set INT in DS:DX
		pop	ds
		ret
disableUART	endp
		assume	ds:@TSRdata

;========================================================================
;		Process the Microsoft/Logitech packet bytes
;========================================================================

MSLTproc	proc
;	CODE_	MOV_DL	MSLTbuttons,<db ?>	; buttons state for MS3/LT/WM
		OPCODE_MOV_DL
MSLTbuttons	db ?
		test	al,01000000b	; =40h	; synchro check
;	if_ nz					; if first byte
	jz @@msltz
		mov	[IOdone],1		; request next 2/3 bytes
		mov	[MSLT_1],al
MSLTCODE1	label	byte			; "ret" if not LT/WM
		xchg	ax,cx			; OPTIMIZE: instead MOV AL,CL
		sub	al,3			; if first byte after 3 bytes
		jz	@@LTWMbutton3		;  then release middle button
		ret
;	end_
@@msltz:

;	if_ ncxz				; skip nonfirst byte at start
	jcxz @@msltcxz
		inc	[IOdone]		; request next byte
		loop	@@MSLT_3
		mov	[MSLT_X],al		; keep X movement LO
;	end_
@@msltcxz:
@@LTret:	ret

@@MSLT_3:	loop	@@LTWM_4
		;mov	cl,0
;	CODE_	MOV_BX	MSLT_1,<db ?,0>		; mouse packet first byte
		OPCODE_MOV_BX
MSLT_1		db ?,0
if USE_286
		ror	bx,2
else
		ror	bx,1
		ror	bx,1
endif
;		xchg	cl,bh			; bits 1-0: X movement HI
		xchg	bh,cl	; TASM and JWASM use opposite encoding
if USE_286		
		ror	bx,2			; bits 5-4: LR buttons
else
		ror	bx,1			; bits 5-4: LR buttons
		ror	bx,1
endif
		or	al,bh			; bits 3-2: Y movement HI
		cbw
		 xchg	cx,ax			; CX=Y movement
;	CODE_	OR_AL	MSLT_X,<db ?>
		OPCODE_OR_AL
MSLT_X		db ?
		cbw
		 xchg	bx,ax			; BX=X movement

		;cbw				; clear wheel value
		xor	al,dl
		 and	al,00000011b	; =3	; LR buttons change mask
		mov	dh,al
		 or	dh,bl			; nonzero if LR buttons state
		 or	dh,cl			;  changed or mouse moved
MSLTCODE2	label	byte
		j	@@MSLTupdate		; "jnz" if MS3
		or	al,00000100b	; =4	; empty event toggles button
		j	@@MSLTupdate

@@LTWM_4:	;mov	ch,0
		mov	[IOdone],ch		; request next packet
MSLTCODE3	label	byte			; if LT "mov cl,3" else
@@LTWMbutton3:	mov	cl,3			; if WM "mov cl,2" else "ret"
		mov	ah,al
		shr	al,cl
		xor	al,dl
		and	ax,111100000100b ; =0F04h
if FASTER_CODE
		jz	@@LTret			; exit if button 3 not changed
endif
		xor	bx,bx
		xor	cx,cx

@@MSLTupdate:	xor	al,dl			; new buttons state
		mov	[MSLTbuttons],al
		j	swapbuttons
MSLTproc	endp

;========================================================================
;		Process the Mouse Systems packet bytes
;========================================================================

MSMproc		proc
		jcxz	@@MSM_1
		cbw
		dec	cx
		jz	@@MSM_2
		dec	cx
		jz	@@MSM_3
		loop	@@MSM_5

@@MSM_4:	add	ax,[MSM_X]
@@MSM_2:	mov	[MSM_X],ax
		j	@@MSMnext

@@MSM_1:	xor	al,10000111b	; =87h	; sync check: AL should
		test	al,11111000b	; =0F8h	;  be equal to 10000lmr
;	if_ zero
	jnz @@msmnz
		test	al,00000110b	; =6	; check the L and M buttons
;	 if_ odd				; if buttons not same
	jpe @@msmeven
		xor	al,00000110b	; =6	; swap them
;	 end_
@@msmeven:
		mov	[MSM_buttons],al	; bits 2-0: MLR buttons
		;j	@@MSMnext

@@MSM_3:	mov	[MSM_Y],ax
@@MSMnext:	inc	[IOdone]		; request next byte
;	end_
@@msmnz:
		ret

@@MSM_5:	;mov	ch,0
		mov	[IOdone],ch		; request next packet
;	CODE_	ADD_AX	MSM_Y,<dw ?>
		OPCODE_ADD_AX
MSM_Y		dw ?
;	CODE_	MOV_BX	MSM_X,<dw ?>
		OPCODE_MOV_BX
MSM_X		dw ?
		xchg	cx,ax			; OPTIMIZE: instead MOV CX,AX
;	CODE_	MOV_AL	MSM_buttons,<db ?>
		OPCODE_MOV_AL
MSM_buttons	db ?
		;j	reverseY		; reverseY is next line anyway
MSMproc		endp

;========================================================================
;			Update mouse status
;========================================================================
;
; In:	AL		(new buttons state, 2 or 3 LSB used)
;	AH			(wheel movemement)
;	BX			(X mouse movement)
;	CX			(Y mouse movement)
; Out:	none
; Use:	callmask, granpos, mickeys, UIR@
; Modf:	AX, CX, DX, BX, SI, DI, wheel, wheelUIR, UIRunlock
; Call:	updateposition, updatebutton, refreshcursor
;
reverseY	proc
		neg	cx			; reverse Y movement
		;j	swapbuttons
reverseY	endp

swapbuttons	proc
		test	al,00000011b	; =3	; check the L and R buttons
;	if_ odd					; if buttons not same
	jpe @@swapeven
;	CODE_	XOR_AL	swapmask,<db 00000011b>	; 0 if (PS2 xor LEFTHAND)
		OPCODE_XOR_AL
swapmask	db 00000011b
;	end_
@@swapeven:
		;j	mouseupdate
swapbuttons	endp

mouseupdate	proc
;	CODE_	AND_AX
		OPCODE_AND_AX
buttonsmask	db	00000111b	; =8
wheelmask	db	0
		xchg	di,ax			; keep btn, wheel state in DI

;----- update mickey counters and screen position

		xchg	ax,bx			; OPTIMIZE: instead MOV AX,BX
;		MOVREG_	bx,<offset X>
	xor bx,bx	; MOVREG optimizes mov bx,offset POINT.X into this
		call	updateposition

		xchg ax,cx
;		MOVREG_	bl,<offset Y>		; OPTIMIZE: BL instead BX
		mov bl,offset POINT.Y
		call	updateposition
		or	cl,al			; bit 0=mickeys change flag

;----- update wheel movement

		mov	ax,[mickeys.Y]
		xchg ax,di			; retrieve button, wheel info
		xchg dx,ax			; OPTIMIZE: instead MOV DX,AX
		mov	al,dh			; wheel: signed 4 bit value
		xor	al,00001000b	; =8
		sub	al,00001000b	; =8	; sign extension AL[0:3]->AL
;	if_ nz					; if wheel moved
	jz @@wheelz
		cbw
		mov	si,TSRdref:wheel
		add	[si + offset BUTTLASTSTATE.counter],ax	; wheel counter
		add	[wheelUIR],al	; same, but used for the UIR
		mov	al,10000000b	; =80h	; bit 7=wheel movement flag
		xor	bx,bx
		call	@lastpos
;	end_
@@wheelz:

;----- update buttons state

		mov	dh,dl			; DH=buttons new state
		xchg	dl,[buttstatus]
		xor	dl,dh			; DL=buttons change state
if FASTER_CODE
;	if_ nz
	jz @@btnfastz
endif
		xor	bx,bx			; buttpress array index
		mov	al,00000010b		; mask for button 1
		call	updatebutton
		mov	al,00001000b		; mask for button 2
		call	updatebutton
		mov	al,00100000b		; mask for button 3
		call	updatebutton
if FASTER_CODE
;	end_
@@btnfastz:
endif

;----- call User Interrupt Routine (CX=events mask)

		dec	[UIRunlock]
;	if_ zero				; if user proc not running
	jnz @@uirnz
		and	cl,[callmask]
;	 if_ nz					; if there is a user events
	jz @@maskz
;	CODE_	MOV_BX	buttstatus,<db 0,0>	; buttons status
		OPCODE_MOV_BX
buttstatus	db 0,0
		xchg	bh,[wheelUIR]
		mov	ax,[granpos.X]
		mov	dx,[granpos.Y]
		xchg	ax,cx
		mov	si,[mickeys.X]
		;mov	di,[mickeys.Y]
		push	ds
		sti
		call	[UIR@]
		pop	ds
;	 end_
@@maskz:
		call	refreshcursor
;	end_
@@uirnz:

;-----

		inc	[UIRunlock]
		ret
mouseupdate	endp

;========================================================================
; In:	AX			(mouse movement)
;	BX			(offset X/offset Y)
; Out:	AX			(1 - mickey counter changed)
; Use:	mickey8, rangemax, rangemin, granumask, senscoeff
; Modf:	DX, SI, sensround, mickeys, rounderr, pos, granpos
;
updateposition	proc
		test	ax,ax
		jz	@@uposret
		mov	si,ax
;	if_ sign
	jns @@upns
		neg	ax
;	end_
@@upns:

;----- apply sensitivity (SI=movement, AX=abs(SI))

		mov	dx,word ptr senscoeff[bx] ; ~[1/3..3.0]*256
;		cmp	ax,4	; JWASM uses "signed byte" variant
	db 03dh, 4, 0	; "cmp ax, word 4"
;	if_ be
	ja @@upa
		 mov	ax,si
		 cmp	dh,1		; skip [-4..4] movements
		 jae	@@newmickeys	;  when sensitivity >= 1.0
		 imul	dx		; =mickeys*sensitivity
;	else_
	jmp short @@upbe
@@upa:
;		cmp	ax,12	; JWASM uses "signed byte" variant
	db 03dh, 12, 0	; "cmp ax, word 12"
;	 if_ ae
if FASTER_CODE
	jb @@up2b
		 mov	ax,dx
		 shl	ax,1
		 add	ax,dx		; =sensitivity*3
;	 else_
	jmp short @@up2ae
@@up2b:
		mul	dx
if USE_286		
		shr	ax,2		; =sensitivity*min(12,abs(mickeys))/4
else
		shr	ax,1		; =sensitivity*min(12,abs(mickeys))/4
		shr	ax,1
endif
;	 end_
@@up2ae:
else
	jb @@up2ae
		 mov	ax,12
;	 end_
@@up2ae:
		mul	dx
if USE_286		
		shr	ax,2		; =sensitivity*min(12,abs(mickeys))/4
else
		shr	ax,1		; =sensitivity*min(12,abs(mickeys))/4
		shr	ax,1
endif
endif
		imul	si		; DX:AX=mickeys*newsensitivity
;	end_
@@upbe:
		add	al,byte ptr sensround[bx]
		mov	byte ptr sensround[bx],al
		mov	al,ah		; remove 256 multiplier from
		mov	ah,dl		;  sensitivity: AX=DX:AX/256
;		adc	ax,0		; add carry from previous adding
	db 15h, 0, 0	; JWASM uses signed byte variant, same length

;----- apply mickeys per 8 pixels ratio to calculate cursor position

@@newmickeys:	add	word ptr mickeys[bx],ax
if FASTER_CODE
		mov	si,word ptr mickey8[bx]
		cmp	si,8
;	if_ ne
	je @@upeq
if USE_286
		shl	ax,3
else
		shl	ax,1
		shl	ax,1
		shl	ax,1
endif
		dec	si
;	andif_ gt
	jle @@upeq
		add	ax,word ptr rounderr[bx]
		inc	si
		cwd
		idiv	si
		mov	word ptr rounderr[bx],dx
		test	ax,ax
		jz	@@uposdone
;	end_
@@upeq:
else
if USE_286
		shl	ax,3
else
		shl	ax,1
		shl	ax,1
		shl	ax,1
endif
		add	ax,word ptr rounderr[bx]
		cwd
		idiv	word ptr mickey8[bx]
		mov	word ptr rounderr[bx],dx
endif
		add	ax,word ptr pos[bx]

;----- cut new position by virtual ranges and save

@savecutpos::	mov	dx,word ptr rangemax[bx]
		cmp	ax,dx
		jge	@@cutpos
		mov	dx,word ptr rangemin[bx]
		cmp	ax,dx
;	if_ le
	jg @@cpg
@@cutpos:	xchg	ax,dx			; OPTIMIZE: instead MOV AX,DX
;	end_
@@cpg:
		mov	word ptr pos[bx],ax	; new position
		and	al,byte ptr granumask[bx]
		mov	word ptr granpos[bx],ax	; new granulated position
@@uposdone:	mov	ax,1
@@uposret:	ret
updateposition	endp

;========================================================================
; In:	AL			(unrolled press bit mask)
;	CL			(unrolled buttons change state)
;	DL			(buttons change state)
;	DH			(buttons new state)
;	BX			(buttpress array index)
; Out:	CL
;	DX			(shifted state)
;	BX			(next index)
; Use:	granpos
; Modf:	AX, SI, buttpress, buttrelease
;
updatebutton	proc
		shr	dx,1
;	if_ carry				; if button changed
	jnc @@ubnc
		mov	si,TSRdref:buttpress
		test	dl,dl
;	 if_ ns					; if button not pressed
	js @@ubs
		add	al,al			; indicate that it released
		mov	si,TSRdref:buttrelease
;	 end_
@@ubs:
		inc	word ptr [si + bx + offset BUTTLASTSTATE.counter]
@lastpos::	or	cl,al
		mov	ax,[granpos.Y]
		mov	[si + bx + offset BUTTLASTSTATE.lastrow],ax
		mov	ax,[granpos.X]
		mov	[si + bx + offset BUTTLASTSTATE.lastcol],ax
;	end_
@@ubnc:
		add	bx,size BUTTLASTSTATE	; next button
		ret
updatebutton	endp

;========================= END OF IRQ HANDLERS ==========================


;============================ INT 10 HANDLER ============================

if USERIL					; -X-
		even
RILtable	dw TSRcref:RIL_F0	; RIL functions
		dw TSRcref:RIL_F1
		dw TSRcref:RIL_F2
		dw TSRcref:RIL_F3
		dw TSRcref:RIL_F4
		dw TSRcref:RIL_F5
		dw TSRcref:RIL_F6
		dw TSRcref:RIL_F7
endif						; -X- USERIL

int10handler	proc
		assume	ds:nothing,es:nothing
		cld
		test	ah,ah			; set video mode?
		jz	@@setmode
		cmp	ah,11h			; font manipulation function
		je	@@setnewfont
		cmp	ax,4F02h		; VESA set video mode?
		je	@@setmode
if USERIL					; -X-
		cmp	ah,0F0h			; RIL func requested?
		jb	@@jmpold10
		cmp	ah,0F7h
		jbe	@@RIL
		cmp	ah,0FAh
		je	@@RIL_FA
endif						; -X- USERIL
@@jmpold10:	jmp_far	oldint10

@@setnewfont:	cmp	al,10h
		jb	@@jmpold10
		cmp	al,20h
		jae	@@jmpold10
		;j	@@setmode

;===== set video mode or activate font

@@setmode:	push	ax
		mov	ax,2
		pushf				;!!! Logitech MouseWare
		push	cs			;  Windows driver workaround
		call	handler33		; hide mouse cursor
		pop	ax
		pushf
		call	[oldint10]
		push	ds
		push	es
		PUSHALL
		MOVSEG	ds,cs,,@TSRdata
		mov	[nocursorcnt],1		; normalize hide counter
		call	setupvideo
@@exitINT10:	jmp	rethandler

if USERIL					; -X-
;===== RIL

@@RIL:		push	ds es
		PUSHALL
		MOVSEG	ds,cs,,@TSRdata
		mov	bp,sp
		mov	al,ah
		and	ax,0Fh			;!!! AH must be 0 for RIL_*
		mov	si,ax
		add	si,si
		call	RILtable[si]
		j	@@exitINT10

;-----

@@RIL_FA:	MOVSEG	es,cs,,@TSRcode		; RIL FA - Interrogate driver
		mov	bx,TSRcref:RILversion
else						; -X-
		assume	ds:nothing		; -X-
		assume	es:nothing		; -X-
endif						; -X- USERIL
		iret

int10handler	endp

if USERIL					; -X-
		assume	ds:@TSRdata
;========================================================================
; RIL F0 - Read one register
;========================================================================
;
; In:	DX			(group index)
;	BX			(register #)
; Out:	BL			(value)
; Use:	videoregs@
; Modf:	AL, SI
; Call:	none
;
RIL_F0		proc
		mov	si,dx
		mov	si,videoregs@[si].regs@
		cmp	dx,20h
;	if_ below				; if not single register
	jae @@rilf0ae
		add	si,bx
;	end_
@@rilf0ae:
		lodsb
		mov	byte ptr [_ARG_BX_],al
		ret
RIL_F0		endp

;========================================================================
; RIL F1 - Write one register
;========================================================================
;
; In:	DX			(group index)
;	BL			(value for single reg)
;	BL			(register # otherwise)
;	BH			(value otherwise)
; Out:	BL			(value)
; Use:	none
; Modf:	AX
; Call:	RILwrite
;
RIL_F1		proc
		mov	ah,bl
		cmp	dx,20h
		jae	RILwrite		; jump if single registers
		xchg	ax,bx			; OPTIMIZE: instead MOV AX,BX
		mov	byte ptr [_ARG_BX_],ah
		;j	RILwrite
RIL_F1		endp

;========================================================================
; In:	DX			(group index)
;	AL			(register # for regs group)
;	AH			(value to write)
; Out:	none
; Use:	videoregs@
; Modf:	AL, DX, BX, DI
; Call:	RILoutAH, RILgroupwrite
;
RILwrite	proc
		xor	bx,bx
		mov	di,dx
		cmp	dx,20h
		mov	dx,videoregs@[di].port@
		mov	videoregs@[di].rmodify?,dl ; OPTIMIZE: DL instead 1
		mov	di,videoregs@[di].regs@
;	if_ below				; if not single register
	jae @@rilwae
		mov	bl,al
;	end_
@@rilwae:
		mov	[di+bx],ah
		jae	RILoutAH
		;j	RILgroupwrite
RILwrite	endp

;========================================================================
; In:	DX			(IO port)
;	AL			(register #)
;	AH			(value to write)
; Out:	none
; Use:	videoregs@
; Modf:	none
; Call:	none
;
RILgroupwrite	proc
		cmp	dl,0C0h
;	if_ ne					; if not ATTR controller
	jz @@rilgwz
		out	dx,ax	; <---
		ret
;	end_
@@rilgwz:
		push	ax dx
		mov	dx,videoregs@[(size RGROUPDEF)*5].port@
		in	al,dx	; <---		; {3DAh} force address mode
		pop	dx ax
		out	dx,al	; <---		; {3C0h} select ATC register
RILoutAH:	xchg	al,ah
		out	dx,al	; <---		; {3C0h} modify ATC register
		xchg	al,ah
		ret
RILgroupwrite	endp

;========================================================================
; RIL F2 - Read register range
;========================================================================
;
; In:	CH			(starting register #)
;	CL			(# of registers)
;	DX			(group index: 0,8,10h,18h)
;	ES:BX			(buffer, CL bytes size)
; Out:	none
; Use:	videoregs@
; Modf:	AX, CX, SI, DI
; Call:	none
;
RIL_F2		proc
		assume	es:nothing
		mov	di,bx
		mov	si,dx
		mov	si,videoregs@[si].regs@
		mov	al,ch
		;mov	ah,0
		add	si,ax
RILmemcopy:	; -X- sti
		mov	ch,0
		shr	cx,1
		rep	movsw
		adc	cx,cx
		rep	movsb
		ret
RIL_F2		endp

;========================================================================
; RIL F3 - Write register range
;========================================================================
;
; In:	CH			(starting register #)
;	CL			(# of registers, >0)
;	DX			(group index: 0,8,10h,18h)
;	ES:BX			(buffer, CL bytes size)
; Out:	none
; Use:	videoregs@
; Modf:	AX, CX, DX, BX, DI
; Call:	RILgroupwrite
;
RIL_F3		proc
		assume	es:nothing
		mov	di,dx
		mov	dx,videoregs@[di].port@
		mov	videoregs@[di].rmodify?,dl ; OPTIMIZE: DL instead 1
		mov	di,videoregs@[di].regs@
RILgrouploop:	xor	ax,ax
		xchg	al,ch
		add	di,ax
;	countloop_
@@rilf3loop:
		mov	ah,es:[bx]
		mov	[di],ah
		inc	bx
		inc	di
		call	RILgroupwrite
		inc	ax			; OPTIMIZE: AX instead AL
;	end_
		loop @@rilf3loop
		ret
RIL_F3		endp

;========================================================================
; RIL F4 - Read register set
;========================================================================
;
; In:	CX			(# of registers, >0)
;	ES:BX			(table of registers records)
; Out:	none
; Use:	videoregs@
; Modf:	AL, CX, BX, DI
; Call:	none
;
RIL_F4		proc
		assume	es:nothing
		; -X- sti
		mov	di,bx
;	countloop_
@@rilf4loop:
		mov	bx,es:[di]
		movadd	di,,2
		mov	bx,videoregs@[bx].regs@
		mov	al,es:[di]
		inc	di
		xlat
		stosb
;	end_
		loop @@rilf4loop
		ret
RIL_F4		endp

;========================================================================
; RIL F5 - Write register set
;========================================================================
;
; In:	CX			(# of registers, >0)
;	ES:BX			(table of registers records)
; Out:	none
; Use:	none
; Modf:	AX, CX, DX, SI
; Call:	RILwrite
;
RIL_F5		proc
		assume	es:nothing
		mov	si,bx
;	countloop_
@@rilf5loop:
		lods	word ptr es:[si]
		xchg	dx,ax			; OPTIMIZE: instead MOV DX,AX
		lods	word ptr es:[si]
		call	RILwrite
;	end_
		loop @@rilf5loop
		ret
RIL_F5		endp

;========================================================================
; RIL F7 - Define registers default
;========================================================================
;
; In:	DX			(group index)
;	ES:BX			(table of one-byte entries)
; Out:	none
; Use:	videoregs@
; Modf:	CL, SI, DI, ES, DS
; Call:	RILmemcopy
;
RIL_F7		proc
		assume	es:nothing
		mov	si,bx
		mov	di,dx
		mov	cl,videoregs@[di].regscnt
		mov	videoregs@[di].rmodify?,cl ; OPTIMIZE: CL instead 1
		mov	di,videoregs@[di].def@
		push	es ds
		pop	es ds
		j	RILmemcopy
RIL_F7		endp

;========================================================================
; RIL F6 - Revert registers to default
;========================================================================
;
; In:	none
; Out:	none
; Use:	videoregs@
; Modf:	AX, CX, DX, BX, SI, DI, ES
; Call:	RILgrouploop
;
RIL_F6		proc
		MOVSEG	es,ds,,@TSRdata
		mov	si,TSRdref:videoregs@+(size RGROUPDEF)*8

@@R6loop:	sub	si,size RGROUPDEF
		xor	cx,cx
		xchg	cl,[si].rmodify?
		jcxz	@@R6next

		mov	bx,[si].def@
		mov	di,[si].regs@
		mov	dx,[si].port@
		mov	cl,[si].regscnt
		;mov	ch,0
		loop	@@R6group
		mov	al,[bx]
		stosb
		out	dx,al	; <---
		;j	@@R6next		; OPTIMIZE: single regs
		j	@@R6loop		;  handled first

@@R6group:	inc	cx			; OPTIMIZE: CX instead CL
		;mov	ch,0
		call	RILgrouploop

@@R6next:	cmp	si,TSRdref:videoregs@
		ja	@@R6loop
		ret
RIL_F6		endp

endif						; -X- USERIL

;======================== END OF INT 10 HANDLER =========================


;========================================================================
;			Draw mouse cursor
;========================================================================

drawcursor	proc
		mov	cx,[videoseg]
		jcxz	@@drawret		; exit if nonstandard mode

		xor	cx,cx
		cmp	[nocursorcnt],cl	; OPTIMIZE: CL instead 0
		jnz	restorescreen		; jump if cursor disabled

		mov	ax,[granumask.Y]
		xchg	cl,[newcursor]		; remove redraw request
						; CX=force cursor request
		inc	ax
		mov	bx,[granpos.Y]		; cursor position Y
		mov	ax,[granpos.X]		; cursor position X
		jz	graphcursor		; jump if graphics mode

;===== text mode cursor

		mov	si,8			; OPTIMIZE: instead -[granumask.Y]
		call	checkifseen
		jc	restorescreen		; jump if not in seen area

		call	gettxtoffset
		cmp	di,[cursor@]
;	if_ eq					; exit if position not changed
	jnz @@drnz
		jcxz	@@drawret		;  and cursor not forced
;	end_
@@drnz:
		push	di
		call	restorescreen
		;MOVSEG	es,[videoseg],,nothing
		pop	di

		cmp	[cursortype],ch		; OPTIMIZE: CH instead 0
;	if_ nz
	jz @@drz

;----- position hardware text mode cursor

		shr	di,1
if USERIL					; -X-
		mov	dx,videoregs@[0].port@	; CRTC port
else
		mov	dx,[crtc]		; 3d4 or 3b4
endif						; -X- USERIL
		mov	ax,di
;		out_	dx,0Fh,al		; cursor position lo
		mov	ah,al
		mov	al,0fh
		out	dx,ax
		xchg	ax,di			; OPTIMIZE: instead MOV AX,DI
;		out_	dx,0Eh,ah		; cursor position hi
		mov	al,0eh
		out	dx,ax
		ret
;	end_
@@drz:

;----- draw software text mode cursor

		mov	[cursor@],di
		mov	ax,es:[di]		; save char under cursor
		mov	textbuf[2],ax
		and	ax,[startscan]
		xor	ax,[endscan]
		stosw				; draw to new position
		mov	textbuf[0],ax
@@drawret:	ret
drawcursor	endp

;========================================================================
;			Restore old screen contents
;========================================================================

restorescreen	proc
		les	di,dword ptr [cursor@]
		assume	es:nothing
		inc	di
;	if_ nz					; if cursor drawn
	jz @@rsz
		sub	[cursor@],di		; OPTIMIZE: instead MOV -1
		mov	ax,[granumask.Y]
		dec	di
		inc	ax

;	 if_ zero
	jnz @@rsnz

;----- graphics mode

		call	restoresprite
		jmp	restorevregs
;	 end_
@@rsnz:

;----- text mode

		mov	si,TSRdref:textbuf
		lodsw
		cmp	ax,es:[di]
;	 if_ eq					; if screen not changed
	jnz @@rsnz2
		movsw				; restore old text char/attrib
;	 end_
@@rsnz2:
;	end_
@@rsz:
@drawret::	ret
restorescreen	endp

;========================================================================
;		Draw graphics mode mouse cursor
;========================================================================

graphcursor	proc
		sub	ax,[hotspot.X]		; virtual X
		sub	bx,[hotspot.Y]		; virtual Y
		mov	si,16			; cursor height
		push	ax
		call	checkifseen
		pop	ax
		jc	restorescreen		; jump if not in seen area

		xchg	ax,bx
		xor	dx,dx
		neg	ax
;	if_ lt
	jge @@gcge
		neg	ax
		xchg	ax,dx
;	end_
@@gcge:
		mov	[spritetop],ax
		mov	ax,[screenheight]
		cmp	si,ax
;	if_ ge
	jl @@gclt
		xchg	si,ax			; OPTIMIZE: instead MOV SI,AX
;	end_
@@gclt:
		sub	si,dx			; =spriteheight
		push	si			;  =min(16-ax,screenheight-dx)
		call	getgroffset
		pop	dx

; cx=force request, bx=X, di=line offset, si=nextrow, dx=spriteheight

		add	di,bx
		les	ax,dword ptr [cursor@]
		assume	es:nothing
		inc	ax
;	if_ nz					; if cursor drawn
	jz @@cpz
;	CODE_	CMP_DI	cursorpos,<dw ?>
		OPCODE_CMP_DI
cursorpos	dw ?
;	 if_ eq
	jnz @@cpnz2
		cmp	dx,[spriteheight]
;	 andif_ eq				; exit if position not changed
	jnz @@cpnz2
		jcxz	@drawret		;  and cursor not forced
;	 end_
@@cpnz2:
		push	bx
		push	dx
		push	di
		dec	ax
		xchg	di,ax			; OPTIMIZE: instead MOV DI,AX
		call	restoresprite
		;MOVSEG	es,[videoseg],,nothing
		pop	di
		pop	dx
;	else_
	jmp short @@cpnz
@@cpz:
		push	bx
		call	updatevregs
;	end_
@@cpnz:
		pop	bx

; bx=X, di=line offset+bx, si=nextrow, dx=spriteheight, es=videoseg

		mov	[cursorpos],di
		mov	[spriteheight],dx
		mov	[nextrow],si
		sub	di,bx
		push	dx

;----- precompute sprite parameters

		push	bx
		mov	cl,[bitmapshift]
		mov	dx,[cursorwidth]
		sar	bx,cl			; left sprite offset (signed)
		mov	ax,[scanline]
		add	dx,bx			; right sprite offset
		cmp	dx,ax
;	if_ ae
	jb @@pspb
		xchg	dx,ax			; DX=min(DX,scanline)
;	end_
@@pspb:

		pop	ax			; =cursorX
		sub	cl,3			; mode 0Dh=1, other=0
		and	ax,[granumask.X]	; fix for mode 4/5
		sar	ax,cl			; sprite shift for non 13h modes
		neg	bx			; sprite shift for 13h mode
;	if_ lt					; if left sprite offset>0
	jge @@pspge
		add	dx,bx
		sub	di,bx
		mov	bl,0
		and	al,7			; shift in byte (X%8)
;	end_
@@pspge:

		inc	bx			; OPTIMIZE: BX instead BL
		sub	al,8			; if cursorX>0
		mov	bh,al			; ...then BH=-(8-X%8)
		push	bx			; ...else BH=-(8-X)=-(8+|X|)

;----- save screen sprite and draw cursor at new cursor position

		mov	[spritewidth],dx
		mov	[cursor@],di
		mov	al,0D6h			; screen source
		call	copysprite		; save new sprite

;	CODE_	MOV_BX	spritetop,<dw ?>
		OPCODE_MOV_BX
spritetop	dw ?
		pop	cx
		pop	ax			; CL/CH=sprite shift
						; AX=[spriteheight]
						; SI=[nextrow]
		add	bx,bx			; mask offset
;	countloop_ ,ax
@@spriteloop:
		push	ax
		push	cx
		push	bx
		push	si
		push	di
		mov	si,[spritewidth]
		mov	dx,word ptr screenmask[bx]
		mov	bx,word ptr cursormask[bx]
		call	makerow
		pop	di
		pop	si
		pop	bx
		pop	cx
		pop	ax
		add	di,si
		xor	si,[nextxor]		; for interlaced mode?
		movadd	bx,,2
;	end_
	dec ax
	jnz @@spriteloop

;-----

		;j	restorevregs
graphcursor	endp

;========================================================================
;		Restore graphics card video registers
;========================================================================

restorevregs	proc
		mov	bx,TSRdref:vdata1
		j	@writevregs
restorevregs	endp

;========================================================================
;		Save & update graphics card video registers
;========================================================================

updatevregs	proc
		mov	bx,TSRdref:vdata1
		mov	ah,0F4h			; read register set
		call	@registerset

		mov	bx,TSRdref:vdata2
@writevregs::	mov	ah,0F5h			; write register set

@registerset::	; if planar videomode [0Dh-12h] then "push es" else "ret"
		db	?
		MOVSEG	es,ds,,@TSRdata
		mov	cx,[bx-2]
		int	10h
		pop	es
		ret
updatevregs	endp

;========================================================================

restoresprite	proc
		call	updatevregs
		mov	al,0D7h			; screen destination
		;j	copysprite		; restore old sprite
restoresprite	endp

;========================================================================
;		Copy screen sprite back and forth
;========================================================================
;
; In:	AL			(0D6h/0D7h-screen source/dest.)
;	ES:DI			(pointer to video memory)
; Out:	CX = 0
;	BX = 0
; Use:	buffer@ (small in DS for CGA, larger for MCGA, in a000 for EGA...)
; Modf:	AX, DX
; Call:	none
;
copysprite	proc	C uses si di ds es	; TASM inserts PUSH here!
		assume	es:nothing
		cmp	al,0D6h
		pushf				; whether to swap pointers
		mov	NEXTOFFSCODE[1],al
;	CODE_	MOV_AX	nextrow,<dw ?>		; ax = next row offset
		OPCODE_MOV_AX
nextrow		dw ?
;	CODE_	MOV_BX	spriteheight,<dw ?>	; bx = sprite height in lines
		OPCODE_MOV_BX
spriteheight	dw ?

		push	ax
		mov	al,0BEh			; 1 for CGA / MCGA: MOV SI
		cmp	[bitmapshift],1		; 1 for MCGA mode 13h
		jz	@blitmode
		cmp	byte ptr videoseg[1],0A0h
		jne	@blitmode		; jump if CGA
		mov	al,0E8h			; 0 for EGA / VGA: CALL
@blitmode:	mov	BLITCODE[1],al
		cmp	al,0E8h
		jnz	@blitram		; if not EGA / VGA, skip GRC
		call	backup3ce		; backup 3ce/f regs
		push	dx
		mov	dx,3ceh
		mov	ax,word ptr SEQ5[1]	; mode reg, ax=nn05
		and	ah,0f4h			; read mode 0/1 plane/match...
		or	ah,1			; write mode 1: from latch :-)
		out	dx,ax
		mov	dx,3ceh
		mov	ax,0003h		; operator: COPY all planes :-)
		out	dx,ax
		pop	dx
@blitram:	pop	ax

		lds	si,[buffer@]
		assume	ds:nothing
		popf				; whether to swap pointers
;	if_ eq
	jnz @@blitnz
		push	ds
		push	es
		pop	ds
		pop	es			; DS:SI=screen
;		xchg	si,di			; ES:DI=buffer
		xchg	di,si	; JWASM and TASM use opposite encoding
;	end_
@@blitnz:

;	countloop_ ,bx
@@spritewloop:
;	CODE_	MOV_CX	spritewidth,<dw ?>	; seen part of sprite in bytes
		OPCODE_MOV_CX
spritewidth	dw ?
		movsub	dx,ax,cx		; mov dx,ax sub dx,cx
		; instead of movsb inside a000:x, one could possibly do
		; out 3ce,4 read/stosb out 3ce,104 read/stosb out 3ce,204
		; read/stosb out 3ce,304 movsb -> copy planewise on EGA/VGA
		; ... but that would require a suitable target buffer!
		rep	movsb			; -X- now for CGA EGA VGA MCGA

NEXTOFFSCODE	db	01h,0d6h		; ADD SI,DX/ADD DI,DX
;	CODE_	XOR_AX	nextxor,<dw ?>		; for interlace modes?
		OPCODE_XOR_AX
nextxor		dw ?
;	end_	; of outer countloop_
	dec bx
	jnz @@spritewloop

BLITCODE	db 90h				; for the label ;-)
		call restore3ce			; CALL or nop-ish MOV SI
		ret	; TASM inserts the right POP here!

copysprite	endp
		assume	ds:@TSRdata

;========================================================================
;		Transform the cursor mask row to screen
;========================================================================
;
; In:	DX = screenmask[row]	(which pixels will combine with screen color)
;	BX = cursormask[row]	(which pixels will be white/inverted, rather than black/transparent)
;	SI = [spritewidth]
;	CL			(sprite shift when mode 13h)
;	CH			(sprite shift when non 13h modes) (??)
;	ES:DI			(video memory pointer)
; Out:	none
; Use:	bitmapshift		(1 if mode 13h MCGA)
; Modf:	AX, CX, DX, BX, SI, DI
; Call:	none
;
makerow		proc
		assume	es:nothing
		cmp	[bitmapshift],1		; =1 for 13h mode
;	if_ eq
	jnz @@mrnz

;-----

;	 countloop_ ,si				; loop over x pixels
@@pixloop:
		shl	bx,cl			; if MSB=0
;		sbb	al,al			; ...then AL=0
	db 1ah, 0c0h	; JWASM and TASM use opposite encoding
		and	al,0Fh			; ...else AL=0Fh (WHITE color)
		shl	dx,cl
;	  if_ carry				; if most sign bit nonzero
	jnc @@mrnc
		xor	al,es:[di]
;	  end_
@@mrnc:
		stosb
		mov	cl,1
;	 end_ countloop
		dec si
		jnz @@pixloop
		ret
;	end_ if
@@mrnz:

;----- display cursor row in modes other than 13h

makerowno13:	call 	backup3ce

		mov	ax,0FFh
;	loop_					; shift masks left until ch++ is 0
@@m13zloop:
		add	dx,dx
		adc	al,al
		inc	dx			; al:dh:dl shifted screenmask
		add	bx,bx
		adc	ah,ah			; ah:bh:bl shifted cursormask
		inc	ch
;	until_ zero
	jnz @@m13zloop
;		xchg	dh,bl			; al:bl:dl - ah:bh:dh
		xchg	bl,dh	; JWASM and TASM use opposite encoding

;	countloop_ ,si
@@m13loop:
		push	dx
; ***		push	bx			; must be omitted, but why?
		mov	dx,es
		cmp	dh,0A0h
;	 if_ ne					; if not planar mode 0Dh-12h
	jz @@m13z
		and	al,es:[di]
		xor	al,ah
		stosb
;	 else_
	jmp short @@m13nz
@@m13z:
		xchg	cx,ax			; OPTIMIZE: instead MOV CX,AX
if 1	; OLD BUT WORKING
;		out_	3CEh,5,0		; set write mode 0: "color: reg2 mask: reg8"
		mov	dx,3ceh
		mov	ax,5
		out	dx,ax
;		out_	,3,8h			; data ANDed with latched data
		mov	ax,803h
		out	dx,ax
		xchg	es:[di],cl
;		out_	,3,18h			; data XORed with latched data
		mov	ax,1803h
		out	dx,ax
		xchg	es:[di],ch
else	; NEW BUT NO TRANSPARENCY
		mov	dx,3ceh			; graphics controller
		mov	ax,word ptr SEQ5[1]	; mode reg, ax=nn05
		and	ah,0f4h			; zap mode bits
		or	ah,2			; set write mode 2: "color: ram, mask: reg8"
		out	dx,ax
		mov	dx,3ceh			; graphics controller
		mov	ax, 803h		; 8 pan 0, mode AND (TODO: preserve pan)
		out	dx,ax			; set operation
		mov	al,8			; mask reg
		mov	ah,cl			; AND mask
		out	dx,ax			; set mask
		mov	byte ptr es:[di],12	; color
		mov	ax,1803h		; 18 pan 0, mode XOR (TODO: preserve pan)
		out	dx,ax			; set operation
		mov	al,8
		mov	ah,ch			; XOR mask
		out	dx,ax			; set mask
		mov	byte ptr es:[di],15	; color
endif
		inc	di
;	 end_
@@m13nz:
		xchg	ax,bx			; OPTIMIZE: instead MOV AX,BX
		pop	bx			; why was this push dx - pop bx?
; ***		pop	dx			; must be omitted, but why?
;	end_ countloop
	dec si
	jnz @@m13loop

restore3ce::	push	dx
		push	ax
		mov	dx,3ceh			; graphics controller
SEQ3		db	0b8h,3,0		; mov ax,0003
		out	dx,ax
SEQ4		db	0b8h,4,0		; mov ax,0004
		out	dx,ax
SEQ5		db	0b8h,5,0		; mov ax,0005
		out	dx,ax
SEQ8		db	0b8h,8,0		; mov ax,0008
		out	dx,ax
		pop	ax
		pop	dx
		ret

;----- backup 4 EGA+ graphics controller registers, can only read VGA, not from EGA

backup3ce::	push	dx
		push	ax
		mov	dx,3ceh			; graphics controller
		mov	al,3			; operator / pan, pan is 3 LSB
		out	dx,al
		inc	dx
		in	al,dx
		mov	cs:SEQ3[2],al
		dec	dx
		mov	al,4			; plane: 2 LSM, only for read
		out	dx,al
		inc	dx
		in	al,dx
		mov	cs:SEQ4[2],al
		dec	dx
		mov	al,5			; mode: read, write, memmode/depth
		out	dx,al
		inc	dx
		in	al,dx
		mov	cs:SEQ5[2],al
		dec	dx
		mov	al,8			; bit mask (...)
		out	dx,al
		inc	dx
		in	al,dx
		mov	cs:SEQ8[2],al
		pop	ax
		pop	dx
		ret

makerow		endp

;========================================================================
;	Return graphic mode video memory offset to line start
;========================================================================
;
; In:	DX			(Y coordinate in pixels)
; Out:	DI			(video memory offset)
;	SI			(offset to next row)
; Use:	videoseg, scanline
; Modf:	AX, DX, ES
; Call:	@getoffsret
; Info:	4/5 (320x200x4)   byte offset = (y/2)*80 + (y%2)*2000h + (x*2)/8
;	  6 (640x200x2)   byte offset = (y/2)*80 + (y%2)*2000h + x/8
;	0Dh (320x200x16)  byte offset = y*40 + x/8, bit offset = 7 - (x % 8)
;	0Eh (640x200x16)  byte offset = y*80 + x/8, bit offset = 7 - (x % 8)
;	0Fh (640x350x4)   byte offset = y*80 + x/8, bit offset = 7 - (x % 8)
;	10h (640x350x16)  byte offset = y*80 + x/8, bit offset = 7 - (x % 8)
;	11h (640x480x2)   byte offset = y*80 + x/8, bit offset = 7 - (x % 8)
;	12h (640x480x16)  byte offset = y*80 + x/8, bit offset = 7 - (x % 8)
;	13h (320x200x256) byte offset = y*320 + x
;	HGC (720x348x2)   byte offset = (y%4)*2000h + (y/4)*90 + x/8
;						    bit offset = 7 - (x % 8)
;
getgroffset	proc
		xor	di,di
		mov	ax,[scanline]
		MOVSEG	es,di,,BIOS
		mov	si,ax			; [nextrow]
		cmp	byte ptr videoseg[1],0A0h
		je	@getoffsret		; jump if not videomode 4-6
		mov	si,2000h
		sar	dx,1			; DX=Y/2
		jnc	@getoffsret
		mov	di,si			; DI=(Y%2)*2000h
		mov	si,-(2000h-80)
		j	@getoffsret
getgroffset	endp

;========================================================================
;		Return text mode video memory offset
;========================================================================
;
; In:	AX/BX			(cursor position X/Y)
; Out:	DI			(video memory offset=row*[0:44Ah]*2+column*2)
; Use:	0:44Ah, 0:44Eh, bitmapshift
; Modf:	AX, DX, BX, ES
; Call:	getpageoffset
;
gettxtoffset	proc
		MOVSEG	es,0,dx,BIOS
		xchg	di,ax			; OPTIMIZE: instead MOV DI,AX
		mov	al,[bitmapshift]
		dec	ax			; OPTIMIZE: AX instead AL
		xchg	cx,ax
		sar	di,cl			; DI=column*2
		xchg	cx,ax			; OPTIMIZE: instead MOV CX,AX
		xchg	ax,bx			; OPTIMIZE: instead MOV AX,BX
if USE_286		
		sar	ax,2			; AX=row*2=Y/4
else
		sar	ax,1			; AX=row*2=Y/4
		sar	ax,1
endif
		mov	dx,[VIDEO_width]	; screen width

@getoffsret::	imul	dx			; AX=row*screen width
		add	ax,[VIDEO_pageoff]	; add video page offset
		add	di,ax
		ret
gettxtoffset	endp

;========================================================================
;		Check if cursor seen and not in update region
;========================================================================
;
; In:	AX/BX			(cursor position X/Y)
;	SI			(cursor height)
; Out:	Carry flag		(cursor not seen or in update region)
;	BX			(cursor X aligned at byte in video memory)
;	SI			(cursor Y+height)
; Use:	scanline, screenheight, upleft, lowright
; Modf:	DX
; Call:	none
;
checkifseen	proc	C uses cx

;----- check if cursor shape seen on the screen

		add	si,bx
		jle	@@retunseen		; fail if Y+height<=0
		cmp	bx,[screenheight]
		jge	@@retunseen		; fail if Y>maxY

;	CODE_	MOV_CL	bitmapshift,<db ?>	; mode 13h=1, 0Dh=4, other=3
		OPCODE_MOV_CL
bitmapshift	db ?
;	CODE_	MOV_DX	cursorwidth,<dw ?>	; cursor width in bytes
		OPCODE_MOV_DX
cursorwidth	dw ?
		sar	ax,cl
		add	dx,ax
		jle	@@retunseen		; fail if X+width<=0
		cmp	ax,[scanline]
		jge	@@retunseen		; fail if X>maxX

;----- check if cursor shape not intersects with update region

		shl	ax,cl
		cmp	bx,[lowright.Y]
		jg	@@retseen		; ok if Y below
		cmp	si,[upleft.Y]
		jle	@@retseen		; ok if Y+height above

		cmp	ax,[lowright.X]
		jg	@@retseen		; ok if X from the right
		shl	dx,cl
		cmp	dx,[upleft.X]
		jle	@@retseen		; ok if X+width from the left

@@retunseen:	stc
		ret
@@retseen:	clc
		ret
checkifseen	endp


;======================= INT 33 HANDLER SERVICES ========================

;========================================================================

setupvideo	proc
		mov	si,szClearArea2/2	; clear area 2
; ERRIF (szClearArea2 mod 2 ne 0) "szClearArea2 must be even!"
		j	@setvideo
setupvideo	endp

;========================================================================
; 21 - Software reset
;========================================================================
;
; In:	none
; Out:	[AX] = 21h/FFFFh	(not installed/installed)
;	[BX] = 2/3/FFFFh	(number of buttons)
; Use:	0:449h, 0:44Ah, 0:463h, 0:484h, 0:487h, 0:488h, 0:4A8h
; Modf:	RedefArea, screenheight, granumask, buffer@, videoseg, cursorwidth,
;	scanline, nextxor, bitmapshift, @registerset, rangemax, ClearArea
; Call:	hidecursor, @savecutpos
;
softreset_21	proc
		mov	[_ARG_BX_],3
buttonscnt	equ	byte ptr [$-2]		; buttons count (2/3)
		mov	[_ARG_AX_],0FFFFh
						; restore default area
		memcopy	szDefArea,ds,@TSRdata,@TSRdata:RedefArea,,,@TSRdata:DefArea
		call	hidecursor		; restore screen contents
		mov	si,szClearArea3/2	; clear area 3
; ERRIF (szClearArea3 mod 2 ne 0) "szClearArea3 must be even!"

;----- setup video regs values for current video mode

@setvideo::	push	si
		MOVSEG	es,ds,,@TSRdata		; push ds pop es, assume...
		MOVSEG	ds,0,ax,BIOS		; xor ax,ax mov ds,ax
		mov	ax,[CRTC_base]		; base IO address of CRTC
if USERIL					; -X-
		mov	videoregs@[0].port@,ax	; 3D4h/3B4h
		add	ax,6			; Feature Control register
		mov	videoregs@[(size RGROUPDEF)*5].port@,ax
else
		mov	cs:crtc,ax
endif						; -X- USERIL
		mov	al,[VIDEO_mode]		; current video mode
		push	ax

;-----

;	block_
		mov	ah,9
		cmp	al,11h			; VGA videomodes?
;	 breakif_ ae
	jae @@blend

		cbw				; OPTIMIZE: instead MOV AH,0
		cmp	al,0Fh			; 0F-10 videomodes?
;	 if_ ae
	jb @@blb
;		testflag [VIDEO_control],mask VCTRL_RAM_64K
		test [VIDEO_control],60h	; mask VCTRL_RAM_64K
		; value ?nn????? where n+1 = 64kBy blocks of RAM installed
;	  breakif_ zero				; break if only 64K of VRAM
	jz @@blend
		mov	ah,2
;	 else_
	jmp short @@blend
@@blb:
		cmp	al,4			; not color text modes?
;	 andif_ below
	jnb @@blend
		xchg	cx,ax			; OPTIMIZE: instead MOV CX,AX
		mov	al,[VIDEO_switches]	; get display combination
;		maskflag al,mask VIDSW_feature0+mask VIDSW_display
		and al,mask VIDSW_feature0+mask VIDSW_display
		cmp	al,9			; EGA+ECD/MDA?
		je	@@lines350
		cmp	al,3			; MDA/EGA+ECD?
;	  if_ eq
	jnz @@blnz
@@lines350:	mov	ch,13h
;	  end_
@@blnz:
		xchg	ax,cx			; OPTIMIZE: instead MOV AX,CX
;	 end_ if
;	end_ block
@@blend:

;-----

if USERIL					; -X-
		lds	si,[VIDEO_ptrtable@]	; 40:a8
		assume	ds:nothing
		lds	si,[si].VIDEO_paramtbl@	; first pointer there...
		add	ah,al
		mov	al,(offset VPARAM_SEQC) shl 2
		shr	ax,2
		add	si,ax			; SI += (AL+AH)*64+5

		mov	di,TSRdref:VRegsArea
		push	di
		mov	al,3
		stosb				; def_SEQ[0]=3
		memcopy	50			; copy default registers value
		mov	al,0
		stosb				; def_ATC[20]=0; VGA only
		memcopy	9			; def_GRC
		;mov	ah,0
		stosw				; def_FC=0, def_GPOS1=0
		inc	ax			; OPTIMIZE: instead MOV AL,1
		stosb				; def_GPOS2=1

		pop	si			; initialize area of defaults
		;mov	di,TSRdref:DefVRegsArea
; ERRIF (DefVRegsArea ne VRegsArea+64) "VRegs area contents corrupted!"
		memcopy	szVRegsArea,,,,es,@TSRdata	; NOTE: changes es assume!

		dec	ax			; OPTIMIZE: instead MOV AL,0
		mov	cx,8
		mov	di,TSRdref:videoregs@[0].rmodify?
;	countloop_
@@setvloop:
		stosb
		add	di,(size RGROUPDEF)-1
;	end_
	loop @@setvloop
else						; -X-
; ***		MOVSEG	es,ds,,@TSRdata		; -X- push ds pop es, assume...
		assume	es:nothing		; -X-
		MOVSEG	ds,cs,,@TSRcode		; -X- push ds pop es, assume...
endif						; -X- USERIL

;----- set parameters for current video mode
; mode	 seg   screen  cell scan planar  VX/
;			    line	byte
;  0	B800h  640x200 16x8   -    -	  -
;  1	B800h  640x200 16x8   -    -	  -
;  2	B800h  640x200	8x8   -    -	  -
;  3	B800h  640x200	8x8   -    -	  -
;  4	B800h  320x200	2x1   80   no	  8
;  5	B800h  320x200	2x1   80   no	  8
;  6	B800h  640x200	1x1   80   no	  8
;  7	B000h  640x200	8x8   -    -	  -
; 0Dh	A000h  320x200	2x1   40  yes	 16
; 0Eh	A000h  640x200	1x1   80  yes	  8
; 0Fh	A000h  640x350	1x1   80  yes	  8
; 10h	A000h  640x350	1x1   80  yes	  8
; 11h	A000h  640x480	1x1   80  yes	  8
; 12h	A000h  640x480	1x1   80  yes	  8
; 13h	A000h  320x200	2x1  320   no	  2
; other     0  640x200	1x1   -    -	  -
;
		pop	ax			; current video mode
; mode 0-3
		mov	dx,0B8FFh		; B800h: [0-3]
		mov	cx,0304h		; 16x8: [0-1]
		mov	di,200			; x200: [4-6,0Dh-0Eh,13h]
		cmp	al,2
;	if_ ae
	jb @@stb
		dec	cx			; 8x8: [2-3,7]
		cmp	al,4
;	andif_ ae
	jb @@stb
; mode 7
		cmp	al,7
		jne	@@checkgraph
		mov	dh,0B0h			; B000h: [7]
;	end_
@@stb:

@@settext:	mov	ch,1
		mov	bh,0F8h
		shl	dl,cl

		MOVSEG	es,0,ax,BIOS		; xor ax,ax mov es,ax assume
		add	al,[VIDEO_lastrow]	; screen height-1
;	if_ nz					; zero on old machines
	jz @@stz
		inc	ax			; OPTIMIZE: AX instead AL
if USE_286
		shl	ax,3
else
		shl	ax,1
		shl	ax,1
		shl	ax,1
endif
		xchg	di,ax			; OPTIMIZE: instead MOV DI,AX
;	end_
@@stz:
		mov	ax,[VIDEO_width]	; screen width
		j	@@setcommon

; mode 4-6
@@checkgraph:	mov	ah,0C3h			; RET opcode for [4-6,13h]
		;mov	cx,0303h		; sprite: 3 bytes/row
		;mov	dx,0B8FFh		; B800h: [4-6]/1x1: [6,0Eh-12h]
		;mov	di,200			; x200: [4-6,0Dh-0Eh,13h]
		mov	si,2000h xor -(2000h-80) ; [nextxor] for [4-6]
		;MOVSEG	es,ds,,@TSRdata
		mov	bx,TSRdref:spritebuf
		cmp	al,6
		je	@@setgraphics
		jb	@@set2x1

; in modes 0Dh-13h screen contents under cursor sprite will be
; saved at free space in video memory (A000h segment)

		mov	dh,0A0h			; A000h: [0Dh-13h]
		MOVSEG	es,0A000h,bx,nothing
		xor	si,si			; [nextxor] for [0Dh-13h]
		cmp	al,13h
		ja	@@nonstandard
		je	@@mode13
; mode 8-0Dh
		cmp	al,0Dh
		jb	@@nonstandard
		mov	ah,06h			; PUSH ES opcode for [0Dh-12h]
		mov	bx,3E82h		; 16002: [0Dh-0Eh]
		je	@@set320
; mode 0Eh-12h
		cmp	al,0Fh
		jb	@@setgraphics
		mov	di,350			; x350: [0Fh-10h]
		mov	bh,7Eh			; 32386: [0Fh-10h]
		cmp	al,11h
		jb	@@setgraphics
		mov	di,480			; x480: [11h-12h]
		mov	bh,9Eh			; 40578: [11h-12h]
		j	@@setgraphics
; mode 13h
@@mode13:	;mov	bl,0
		mov	bh,0FAh			; =320*200
		mov	cx,1000h		; sprite: 16 bytes/row

@@set320:	inc	cx			; OPTIMIZE: instead INC CL
@@set2x1:	dec	dx			; OPTIMIZE: instead MOV DL,-2

@@setgraphics:	nop				; -X-
		saveFAR	[buffer@],es,bx
		mov	[nextxor],si
		mov	byte ptr [@registerset],ah
		j	@@setgcommon

@@nonstandard:	;mov	cl,3
		;mov	dl,0FFh
		;mov	di,200

;;+++++ for text modes: dh := 0B8h, j @@settext

		mov	dh,0			; no video segment

@@setgcommon:	mov	ax,640			; virtual screen width
		mov	bh,0FFh			; Y granularity
		shr	ax,cl

@@setcommon:	mov	[screenheight],di
		mov	[scanline],ax		; screen line width in bytes
		mov	[bitmapshift],cl	; log2(screen/memory ratio)
						;  (mode 13h=1, 0-1/0Dh=4, other=3)
		mov	byte ptr [cursorwidth],ch ; cursor width in bytes
		mov	byte ptr [granumask.X],dl
		mov	byte ptr [granumask.Y],bh
		mov	byte ptr videoseg[1],dh
		shl	ax,cl
		pop	si

;----- set ranges and center cursor (AX=screenwidth, DI=screenheight)

		mov	cx,ax
		dec	ax
		mov	[rangemax.X],ax		; set right X range
		shr	cx,1			; X middle

		mov	dx,di
		dec	di
		mov	[rangemax.Y],di		; set lower Y range
		shr	dx,1			; Y middle

;----- set cursor position (CX=X, DX=Y, SI=area size to clear)

@setpos::	nop	;cli			; -X-
		MOVSEG	es,ds,,@TSRdata
		mov	di,TSRdref:ClearArea
;		xchg	cx,si
		xchg	si,cx	; JWASM and TASM use opposite encoding
		xor	ax,ax
		rep	stosw

		xchg	ax,dx			; OPTIMIZE: instead MOV AX,DX
;		MOVREG_	bx,<offset Y>
		mov	bx,offset POINT.Y
		call	@savecutpos
		xchg	ax,si			; OPTIMIZE: instead MOV AX,SI
;		MOVREG_	bl,<offset X>		; OPTIMIZE: BL instead BX
		mov	bl,offset POINT.X
		jmp	@savecutpos
softreset_21	endp

;========================================================================
; 1F - Disable mouse driver and unhook int 10 and IRQ
;========================================================================
;
; In:	none
; Out:	[AX] = 1Fh/FFFFh	(success/unsuccess)
;	[ES:BX]			(old int33 handler)
; Use:	oldint33, oldint10
; Modf:	AX, CX, DX, BX, DS, ES, disabled?, nocursorcnt
; Call:	INT 21/35, INT 21/25, disablePS2/disableUART, hidecursor
; -X- TODO: call int 15/C201 (reset resolution and rate, disable)
; -X- TODO: check if enabledriver_20 still works after int 15/C201
;
disabledrv_1F	proc
		les	ax,[oldint33]
		assume	es:nothing
		mov	[_ARG_ES_],es
		mov	[_ARG_BX_],ax

		; this can be patched from disablePS2 to disableUART:
		call_	disableproc,disablePS2	; can change ES!

		mov	al,[disabled?]
		test	al,al
;	if_ zero				; if driver not disabled
	jnz @@ddrvnz
		mov	[buttstatus],al
		inc	ax			; OPTIMIZE: instead MOV AL,1
		mov	[nocursorcnt],al	; normalize hide counter
		call	hidecursor		; restore screen contents

;----- check if INT 33 or INT 10 were intercepted
;	(i.e. handlers segment not equal to CS)

		mov	cx,cs
		DOSGetIntr 33h
		mov	dx,es
		cmp	dx,cx
		jne	althandler_18

		; DOSGetIntr 10h
		; mov	ah,35h
		mov	al,10h
		int	21h
		movsub	ax,es,cx
		jne	althandler_18

		inc	ax			; OPTIMIZE: instead MOV AL,1
		mov	[disabled?],al
		lds	dx,[oldint10]
		assume	ds:nothing
		DOSSetIntr 10h			; restore old INT 10 handler
;	end_ if
@@ddrvnz:
		ret
disabledrv_1F	endp
		assume	ds:@TSRdata

;========================================================================
; 18 - Set alternate User Interrupt Routine
;========================================================================
;
; In:	CX			(call mask)
;	ES:DX			(FAR routine)
; Out:	[AX] = 18h/FFFFh	(success/unsuccess)
;
althandler_18	proc
		assume	es:nothing
		mov	[_ARG_AX_],0FFFFh
		ret
althandler_18	endp

;========================================================================
; 19 - Get alternate User Interrupt Routine
;========================================================================
;
; In:	CX			(call mask)
; Out:	[CX]			(0=not found)
;	[BX:DX]			(FAR routine)
;
althandler_19	proc
		mov	[_ARG_CX_],0
		ret
althandler_19	endp

;========================================================================
; 00 - Reset driver and read status
;========================================================================
;
; In:	none
; Out:	[AX] = 0/FFFFh		(not installed/installed)
;	[BX] = 2/3/FFFFh	(number of buttons)
; Use:	none
; Modf:	none
; Call:	softreset_21, enabledriver_20
;
resetdriver_00	proc
		call	softreset_21
		;j	enabledriver_20
resetdriver_00	endp

;========================================================================
; 20 - Enable mouse driver and hook int 10 and IRQ
;========================================================================
;
; In:	none
; Out:	[AX] = 20h/FFFFh	(success/unsuccess)
; Use:	none
; Modf:	AX, CX, DX, BX, ES, disabled?, oldint10
; Call:	INT 21/35, INT 21/25, setupvideo, enablePS2/enableUART
;
enabledriver_20	proc
		xor	cx,cx
		xchg	cl,[disabled?]
;	if_ ncxz
	jcxz @@edcxz

;----- set new INT 10 handler

		DOSGetIntr 10h
		saveFAR	[oldint10],es,bx
		;mov	al,10h
		push	ds	; -X-
		push	cs	; -X-
		pop	ds	; -X-
;		DOSSetIntr ,,,@TSRcode:int10handler
		mov	dx,@TSRcode:int10handler
		mov	ah,25h
		int	21h
		pop	ds	; -X-
;	end_
@@edcxz:

;-----

		call	setupvideo
		; this can be patched from enablePS2 to enableUART:
		jmp_	enableproc,enablePS2	; can change ES!
enabledriver_20	endp

;========================================================================
; 03 - Get cursor position, buttons status and wheel counter
;========================================================================
;
; In:	none
; Out:	[BL]			(buttons status)
;	[BH]			(wheel movement counter)
;	[CX]			(X - column)
;	[DX]			(Y - row)
; Use:	buttstatus, granpos
; Modf:	AX, CX, DX, wheel.counter
; Call:	@retBCDX
;
status_03	proc
		xor	ax,ax
		xchg	ax,[wheel.counter]
		mov	ah,al
		mov	al,[buttstatus]
		mov	cx,[granpos.X]
		mov	dx,[granpos.Y]
		j	@retBCDX
status_03	endp

;========================================================================
; 05 - Get button press data
;========================================================================
;
; In:	BX			(button number; -1 for wheel)
; Out:	[AL]			(buttons status)
;	[AH]			(wheel movement counter)
;	[BX]			(press times or wheel counter, if BX was -1)
;	[CX]			(last press X)
;	[DX]			(last press Y)
; Use:	none
; Modf:	CX, buttpress, wheel
; Call:	@retbuttstat
;
pressdata_05	proc
		mov	cx,TSRdref:buttpress-(size BUTTLASTSTATE)
		j	@retbuttstat
pressdata_05	endp

;========================================================================
; 06 - Get button release data
;========================================================================
;
; In:	BX			(button number; -1 for wheel)
; Out:	[AL]			(buttons status)
;	[AH]			(wheel movement counter)
;	[BX]			(release times or wheel counter, if BX was -1)
;	[CX]			(last release X)
;	[DX]			(last release Y)
; Use:	buttstatus
; Modf:	AX, CX, DX, BX, SI, buttrelease, wheel
; Call:	none
;
releasedata_06	proc
		mov	cx,TSRdref:buttrelease-(size BUTTLASTSTATE)
@retbuttstat::	mov	ah,byte ptr [wheel.counter]
		mov	al,[buttstatus]
		mov	[_ARG_AX_],ax
		xor	ax,ax

		inc	bx
		mov	si,TSRdref:wheel
		jz	@@retlastpos		; jump if BX was -1
		mov	si,cx

		xor	cx,cx
		xor	dx,dx
		cmp	bx,2+1
;	if_ be
	ja @@rlpa
; ERRIF (6 ne size BUTTLASTSTATE) "BUTTLASTSTATE structure size changed!"
		add	bx,bx
		add	si,bx			; SI+BX=buttrelease
		add	bx,bx			;  +button*size BUTTLASTSTATE

@@retlastpos:	xchg	[si + bx + offset BUTTLASTSTATE.counter],ax
		mov	cx,[si+bx + offset BUTTLASTSTATE.lastcol]
		mov	dx,[si+bx + offset BUTTLASTSTATE.lastrow]
;	end_ if
@@rlpa:
@retBCDX::	mov	[_ARG_DX_],dx
@retBCX::	mov	[_ARG_CX_],cx
@retBX::	mov	[_ARG_BX_],ax
		ret
releasedata_06	endp

;========================================================================
; 0B - Get motion counters
;========================================================================
;
; In:	none
; Out:	[CX]			(number of mickeys mouse moved
;	[DX]			 horizontally/vertically since last call)
; Use:	none
; Modf:	mickeys
; Call:	@retBCDX
;
mickeys_0B	proc
		xchg	ax,bx			; OPTIMIZE: instead MOV AX,BX
		xor	cx,cx
		xor	dx,dx
		xchg	[mickeys.X],cx
		xchg	[mickeys.Y],dx
		j	@retBCDX
mickeys_0B	endp

;========================================================================
; 11 - Check wheel support and get capabilities flags
;========================================================================
;
; In:	none
; Out:	[AX] = 574Dh ('WM')
;	[CX]			(capabilities flag, bits 1..15 reserved)
;	[BX]			(capabilities flag, all bits reserved)
; Use:	none
; Modf:	AX
; Call:	@retBCX
;
wheelAPI_11	proc
		mov	[_ARG_AX_],574Dh
;	CODE_	MOV_CX	wheelflags,<db 0,0>
		OPCODE_MOV_CX
wheelflags	db 0,0
		xor	ax,ax
		j	@retBCX
wheelAPI_11	endp

;========================================================================
; 15 - Get driver storage requirements
;========================================================================
;
; In:	none
; Out:	[BX]			(buffer size)
; Use:	szSaveArea
; Modf:	AX
; Call:	@retBX
;
storagereq_15	proc
		mov	ax,szSaveArea
		j	@retBX
storagereq_15	endp

;========================================================================
; 1B - Get mouse sensitivity
;========================================================================
;
; In:	none
; Out:	[BX]			(horizontal sensitivity, 1..100)
;	[CX]			(vertical sensitivity, 1..100)
;	[DX]			(speed threshold in mickeys/second)
; Use:	sensval, /doublespeed/
; Modf:	AX, CX, DX
; Call:	@retBCDX
;
sensitivity_1B	proc
		mov	ax,[sensval.X]
		mov	cx,[sensval.Y]
		inc	ax
		inc	cx
		xor	dx,dx
;;*		mov	dx,[doublespeed]
		j	@retBCDX
sensitivity_1B	endp

;========================================================================
; 1E - Get display page
;========================================================================
;
; In:	none
; Out:	[BX]			(display page number)
; Use:	0:462h
; Modf:	AX, DS
; Call:	@retBX
;
videopage_1E	proc
		MOVSEG	ds,0,ax,BIOS
		mov	al,[VIDEO_pageno]
		j	@retBX
videopage_1E	endp
		assume	ds:@TSRdata

;========================================================================
; 01 - Show mouse cursor
;========================================================================
;
; In:	none
; Out:	none
; Use:	none
; Modf:	AX, lowright.Y
; Call:	cursorstatus
;
showcursor_01	proc
		neg	ax			; AL=AH=-1
		mov	byte ptr lowright.Y[1],al ; place update region
		j	cursorstatus		;  outside seen screen area
showcursor_01	endp

;========================================================================
; 02 - Hide mouse cursor
;========================================================================
;
; In:	none
; Out:	none
; Use:	none
; Modf:	AX
; Call:	cursorstatus
;
hidecursor_02	proc
		dec	ax			; AL=1,AH=0
		;j	cursorstatus
hidecursor_02	endp

;========================================================================
; Hint:	request to cursor redraw (instead refresh) is useful in cases when
;	interrupt handlers try to hide, then show cursor while cursor
;	drawing is in progress
;
cursorstatus	proc
		add	al,[nocursorcnt]
		sub	ah,al			; exit if "already enabled"
		jz	@showret		;   or "counter overflow"
		mov	[nocursorcnt],al
		inc	ah			; jump if cursor changed
		jz	redrawcursor		;  between enabled/disabled
		ret
cursorstatus	endp

;========================================================================
; 07 - Set horizontal cursor range
;========================================================================
;
; In:	CX			(min X)
;	DX			(max X)
; Out:	none
; Use:	none
; Modf:	BX
; Call:	@setnewrange
;
hrange_07	proc
;		MOVREG_	bx,<offset X>
		xor bx,bx	; MOVREG optimizes mov bx,offset POINT.X ...
		j	@setnewrange
hrange_07	endp

;========================================================================
; 08 - Set vertical cursor range
;========================================================================
;
; In:	CX			(min Y)
;	DX			(max Y)
; Out:	none
; Use:	pos
; Modf:	CX, DX, BX, rangemin, rangemax
; Call:	setpos_04
;
vrange_08	proc
;		MOVREG_	bx,<offset Y>
		mov	bx,offset POINT.Y
if FOOLPROOF
@setnewrange::	xchg	ax,cx			; OPTIMIZE: instead MOV AX,CX
		cmp	ax,dx
;	if_ ge
	jl @@snrl
		xchg	ax,dx
;	end_
@@snrl:
		mov	word ptr rangemin[bx],ax
else
@setnewrange::	mov	word ptr rangemin[bx],cx
endif
		mov	word ptr rangemax[bx],dx
		mov	cx,[pos.X]
		mov	dx,[pos.Y]
		;j	setpos_04
vrange_08	endp

;========================================================================
; 04 - Position mouse cursor
;========================================================================
;
; In:	CX			(X - column)
;	DX			(Y - row)
; Out:	none
; Use:	none
; Modf:	SI
; Call:	@setpos, refreshcursor
;
setpos_04	proc
		mov	si,szClearArea1/2	; clear area 1
; ERRIF (szClearArea1 mod 2 ne 0) "szClearArea1 must be even!"
		call	@setpos
		;j	refreshcursor
setpos_04	endp

;========================================================================

refreshcursor	proc
		sub	[videolock],1
		jc	@showret		; was 0: drawing in progress
		js	@@refreshdone		; was -1: queue already used
		sti

;	loop_
@@rcloop:
		call	drawcursor
@@refreshdone:	inc	[videolock]		; drawing stopped
;	until_ nz				; loop until queue empty
	jz @@rcloop

		cli
@showret::	ret
refreshcursor	endp

;========================================================================
; 09 - Define graphics cursor
;========================================================================
;
; In:	BX			(hot spot X)
;	CX			(hot spot Y)
;	ES:DX			(pointer to bitmaps)
; Out:	none
; Use:	none
; Modf:	AX, CX, BX, SI, DI, ES, hotspot, screenmask, cursormask
; Call:	@showret, redrawcursor
;
graphcursor_09	proc
		assume	es:nothing

;----- compare user shape with internal area

		mov	si,TSRdref:hotspot
		lodsw
		cmp	ax,bx
;	if_ eq
	jnz @@gc9nz
		lodsw
		xor	ax,cx
;	andif_ eq
	jnz @@gc9nz
		mov	di,dx
		;mov	ah,0
		mov	al,16+16
		xchg	ax,cx
		repe	cmpsw
		je	@showret		; exit if cursor not changed
		xchg	cx,ax			; OPTIMIZE: instead MOV CX,AX
;	end_
@@gc9nz:

;----- copy user shape to internal area

		push	ds
		push	ds
		push	es
		pop	ds
		pop	es
		mov	di,TSRdref:hotspot
		xchg	ax,bx			; OPTIMIZE: instead MOV AX,BX
		stosw
		xchg	ax,cx			; OPTIMIZE: instead MOV AX,CX
		stosw
;		memcopy	2*(16+16),,,,,,dx
		mov si,dx
		mov cx, 16+16
		rep	movsw
		pop	ds
		;j	redrawcursor
graphcursor_09	endp

;========================================================================

redrawcursor	proc
hidecursor::	mov	[newcursor],1		; force cursor redraw
		j	refreshcursor
redrawcursor	endp

;========================================================================
; 0A - Define text cursor
;========================================================================
;
; In:	BX			(0 - SW, else HW text cursor)
;	CX			(screen mask/start scanline)
;	DX			(cursor mask/end scanline)
; Out:	none
; Use:	none
; Modf:	AX, CX, BX, cursortype, startscan, endscan
; Call:	INT 10/01, @showret, redrawcursor
;
textcursor_0A	proc
;		xchg	cx,bx
		xchg	bx,cx	; TASM and JWASM use opposite encodings
;	if_ ncxz				; if hardware cursor
	jcxz @@tcacxz
		mov	ch,bl
		mov	cl,dl
		mov	ah,1
		int	10h			; set cursor shape & size
		mov	cl,1
;	end_
@@tcacxz:
		cmp	cl,[cursortype]
;	if_ eq
	jnz @@tcanz
		cmp	bx,[startscan]
;	andif_ eq
	jnz @@tcanz
		cmp	dx,[endscan]
		je	@showret		; exit if cursor not changed
;	end_
@@tcanz:

;-----

		mov	[cursortype],cl
		mov	[startscan],bx
		mov	[endscan],dx
		j	redrawcursor
textcursor_0A	endp

;========================================================================
; 10 - Define screen region for updating
;========================================================================
;
; In:	CX, DX			(X/Y of upper left corner)
;	SI, DI			(X/Y of lower right corner)
; Out:	none
; Use:	none
; Modf:	AX, CX, DX, DI, upleft, lowright
; Call:	redrawcursor
;
updateregion_10	proc
		mov	ax,[_ARG_SI_]
if FOOLPROOF
		cmp	cx,ax
;	if_ ge
	jl @@ur10l
		xchg	cx,ax
;	end_
@@ur10l:
		mov	[upleft.X],cx
		mov	[lowright.X],ax
		xchg	ax,di			; OPTIMIZE: instead MOV AX,DI
		cmp	dx,ax
;	if_ ge
	jl @@ur10l2
		xchg	dx,ax
;	end_
@@ur10l2:
		mov	[upleft.Y],dx
		mov	[lowright.Y],ax
else
		mov	[upleft.X],cx
		mov	[upleft.Y],dx
		mov	[lowright.X],ax
		mov	[lowright.Y],di
endif
		j	redrawcursor
updateregion_10	endp

;========================================================================
; 16 - Save driver state
;========================================================================
;
; In:	BX			(buffer size)
;	ES:DX			(buffer)
; Out:	none
; Use:	SaveArea
; Modf:	CX, SI, DI
; Call:	none
;
savestate_16	proc
		assume	es:nothing
if FOOLPROOF
;;-		cmp	bx,szSaveArea		;!!! TurboPascal IDE
;;-		jb	@stateret		;  workaround: garbage in BX
endif
;		memcopy	szSaveArea,,,dx,,,@TSRdata:SaveArea
		mov di,dx
		mov si,@TSRdata:SaveArea
		mov cx,szSaveArea/2	; happens to be even
		rep movsw
@stateret:	ret
savestate_16	endp

;========================================================================
; 17 - Restore driver state
;========================================================================
;
; In:	BX			(buffer size)
;	ES:DX			(saved state buffer)
; Out:	none
; Use:	none
; Modf:	SI, DI, DS, ES, SaveArea
; Call:	@stateret, redrawcursor
;
restorestate_17	proc
		assume	es:nothing
if FOOLPROOF
;;-		cmp	bx,szSaveArea		;!!! TurboPascal IDE
;;-		jb	@stateret		;  workaround: garbage in BX
endif

;----- do nothing if SaveArea is not changed

;;*		mov	si,TSRdref:SaveArea
;;*		mov	di,dx
;;*		mov	cx,szSaveArea/2
;;*ERRIF (szSaveArea mod 2 ne 0) "szSaveArea must be even!"
;;*		repe	cmpsw
;;*		je	@stateret

;----- change SaveArea

		push	es
		push	dx
		MOVSEG	es,ds,,@TSRdata
		pop	si
		pop	ds
		assume	ds:nothing
		memcopy	szSaveArea,,,@TSRdata:SaveArea
		MOVSEG	ds,es,,@TSRdata
		j	redrawcursor
restorestate_17	endp

;========================================================================
; 0D - Light pen emulation ON
;========================================================================
;
; In:	none
; Out:	none
; Use:	none
; Modf:	none
; Call:	lightpenoff_0E
;
;;*lightpenon_0D	proc
;;*		mov	al,0
;;*		;j	lightpenoff_0E
;;*lightpenon_0D	endp

;========================================================================
; 0E - Light pen emulation OFF
;========================================================================
;
; In:	none
; Out:	none
; Use:	none
; Modf:	nolightpen?
; Call:	none
;
;;*lightpenoff_0E	proc
;;*		mov	[nolightpen?],al	; OPTIMIZE: AL instead nonzero
;;*		ret
;;*lightpenoff_0E	endp

;========================================================================
; 14 - Exchange User Interrupt Routines
;========================================================================
;
; In:	CX			(new call mask)
;	ES:DX			(new FAR routine)
; Out:	[CX]			(old call mask)
;	[ES:DX]			(old FAR routine)
; Use:	callmask, UIR@
; Modf:	AX
; Call:	UIR_0C
;
exchangeUIR_14	proc
		assume	es:nothing
		;mov	ah,0
		mov	al,[callmask]
		mov	[_ARG_CX_],ax
		mov	ax,word ptr UIR@[0]
		mov	[_ARG_DX_],ax
		mov	ax,word ptr UIR@[2]
		mov	[_ARG_ES_],ax
		;j	UIR_0C
exchangeUIR_14	endp

;========================================================================
; 0C - Define User Interrupt Routine
;========================================================================
;
; In:	CX			(call mask)
;	ES:DX			(FAR routine)
; Out:	none
; Use:	none
; Modf:	UIR@, callmask
; Call:	none
;
UIR_0C		proc
		assume	es:nothing
		saveFAR [UIR@],es,dx
		mov	[callmask],cl
		ret
UIR_0C		endp

;========================================================================
; 0F - Set mickeys/pixels ratios
;========================================================================
;
; In:	CX			(number of mickeys per 8 pix
;	DX			 horizontally/vertically)
; Out:	none
; Use:	none
; Modf:	mickey8
; Call:	none
;
sensitivity_0F	proc
if FOOLPROOF
		test	dx,dx
;	if_ nz					; ignore wrong ratio
	jz @@sensfp
;	andif_ ncxz				; ignore wrong ratio
	jcxz @@sensfp
endif
		mov	[mickey8.X],cx
		mov	[mickey8.Y],dx
if FOOLPROOF
;	end_
@@sensfp:
endif
		ret
sensitivity_0F	endp

;========================================================================
; 1A - Set mouse sensitivity
;========================================================================
;
; In:	BX			(horizontal sensitivity, 1..100)
;	CX			(vertical sensitivity, 1..100)
;	DX			(speed threshold in mickeys/second, ignored)
; Out:	none
; Use:	none
; Modf:	AX, CX, BX
; Call:	senscalc
;
sensitivity_1A	proc
		xchg	ax,bx			; OPTIMIZE: instead MOV AX,BX
;		MOVREG_	bx,<offset X>
	xor bx,bx	; MOVREG optimizes	mov bx,offset POINT.X
		call	senscalc
		xchg	ax,cx			; OPTIMIZE: instead MOV AX,CX
;		MOVREG_	bl,<offset Y>		; OPTIMIZE: BL instead BX
	mov bl,offset POINT.Y
		;j	senscalc
sensitivity_1A	endp

;========================================================================
; In:	AX			(001A value)
;	BX			(offset X/offset Y)
; Out:	none
; Modf:	AX, DX, DI, sensval, senscoeff
; Call:	none
; Hint:	sensval=AX-1, senscoeff=[(sensval^2/3600+1/3)*256]
;
senscalc	proc
		dec	ax
;		cmp	ax,100			; ignore original values
	db 3dh, 100, 0	; "cmp ax, word 100"
		; JWASM encodes this in the "byte 100" syntax, same size
;	if_ below				;  outside [1..100]
	jnc @@scnc
		mov	word ptr sensval[bx],ax
		mul	ax			; DX:AX=V^2 (0<=X^2<10000)
		add	ax,3600/3
		;adc	dx,0			; DX:AX=V^2+1200
		;mov	dh,0
		;xchg	dh,dl
;		xchg	dl,al
		xchg	al,dl	; JWASM and TASM use opposite encoding
;		xchg	dl,ah			; DX:AX=(V^2+1200)*256
		xchg	ah,dl	; JWASM and TASM use opposite encoding
		mov	di,3600
		div	di			; AX=(V^2+1200)*256/3600
		mov	word ptr senscoeff[bx],ax
;	end_
@@scnc:
		ret
senscalc	endp

;========================================================================
; 13 - Define double-speed threshold
;========================================================================
;
; In:	DX			(speed threshold in mickeys/second)
; Out:	none
; Use:	none
; Modf:	/DX/, /doublespeed/
; Call:	none
;
doublespeed_13	proc
;;*		test	dx,dx
;;*	if_ zero
;;*		mov	dl,64
;;*	end_
;;*		mov	[doublespeed],dx
		;ret
doublespeed_13	endp

;========================================================================
; 0D 0E 11 12 18 19 1C 1D - Null function for not implemented calls
;========================================================================

nullfunc	proc
		ret
nullfunc	endp

;========================================================================
;				INT 33 handler
;========================================================================

		even
handler33table	dw TSRcref:resetdriver_00
		dw TSRcref:showcursor_01
		dw TSRcref:hidecursor_02
		dw TSRcref:status_03
		dw TSRcref:setpos_04
		dw TSRcref:pressdata_05
		dw TSRcref:releasedata_06
		dw TSRcref:hrange_07
		dw TSRcref:vrange_08
		dw TSRcref:graphcursor_09
		dw TSRcref:textcursor_0A
		dw TSRcref:mickeys_0B
		dw TSRcref:UIR_0C
		dw TSRcref:nullfunc		;lightpenon_0D
		dw TSRcref:nullfunc		;lightpenoff_0E
		dw TSRcref:sensitivity_0F
		dw TSRcref:updateregion_10
		dw TSRcref:wheelAPI_11
		dw TSRcref:nullfunc		;12 - large graphics cursor
		dw TSRcref:doublespeed_13
		dw TSRcref:exchangeUIR_14
		dw TSRcref:storagereq_15
		dw TSRcref:savestate_16
		dw TSRcref:restorestate_17
		dw TSRcref:althandler_18
		dw TSRcref:althandler_19
		dw TSRcref:sensitivity_1A
		dw TSRcref:sensitivity_1B
		dw TSRcref:nullfunc		;1C - InPort mouse only
		dw TSRcref:nullfunc		;1D - define display page #
		dw TSRcref:videopage_1E
		dw TSRcref:disabledrv_1F
		dw TSRcref:enabledriver_20
		dw TSRcref:softreset_21

handler33	proc
		assume	ds:nothing,es:nothing
		cld
		test	ah,ah
;	if_ zero
	jnz @@h33nz
		push	ds
		MOVSEG	ds,cs,,@TSRdata
if PicoMEM
		cmp al,60h
		je updatemouse_60
endif
		cmp	al,21h
		ja	language_23
		push	es
		PUSHALL
		mov	si,ax			;!!! AX must be unchanged
		mov	bp,sp
		add	si,si
		call	handler33table[si]	; call by calculated offset
rethandler::	POPALL
		pop	es
		pop	ds
;	end_
@@h33nz:
		iret
		assume	ds:@TSRdata

if PicoMEM
;========================================================================
; Added for the PicoMEM, but it is generic
; -60h : Call UpdateMouse
updatemouse_60:
		mov ax,dx	; Put Back button in ax
		call mouseupdate
		pop ds
		iret
endif
		
;========================================================================
; 23 - Get language for messages
; Out:	[BX]			(language code: 0 - English)
;
language_23:	cmp	al,23h
		je	@iretBX0

;========================================================================
; 24 - Get software version, mouse type and IRQ
; Out:	[AX] = 24h/FFFFh	(installed/error)
;	[BX]			(version)
;	[CL]			(IRQ #/0=PS/2)
;	[CH] = 1=bus/2=serial/3=InPort/4=PS2/5=HP (mouse type)
; Use:	driverversion
;
version_24:	cmp	al,24h
;	if_ eq
	jnz @@v24nz
		mov	bx,driverversion
;	CODE_	MOV_CX	mouseinfo,<db ?,4>
		OPCODE_MOV_CX
mouseinfo	db ?,4
;	end_
@@v24nz:

;========================================================================
; 26 - Get maximum virtual screen coordinates
; Out:	[BX]			(mouse disabled flag)
;	[CX]			(max virtual screen X)
;	[DX]			(max virtual screen Y)
; Use:	bitmapshift
;
maxscreen_26:	cmp	al,26h
;	if_ eq
	jnz @@m26nz
		mov	cl,[bitmapshift]
;	CODE_	MOV_BX	scanline,<dw ?>
		OPCODE_MOV_BX
scanline	dw ?
;	CODE_	MOV_DX	screenheight,<dw ?>
		OPCODE_MOV_DX
screenheight	dw ?
		shl	bx,cl
		dec	dx
		mov	cx,bx
		dec	cx
;	CODE_	MOV_BX	disabled?,<db 1,0>	; 1=driver disabled
		OPCODE_MOV_BX
disabled?	db 1,0
;	end_
@@m26nz:

;========================================================================
; 27 - Get screen/cursor masks and mickey counters
; Out:	[AX]			(screen mask/start scanline)
;	[BX]			(cursor mask/end scanline)
;	[CX]			(number of mickeys mouse moved
;	[DX]			 horizontally/vertically since last call)
; Use:	startscan, endscan
; Modf:	mickeys
;
cursor_27:	cmp	al,27h
;	if_ eq
	jnz @@c27nz
		mov	ax,[startscan]
		mov	bx,[endscan]
		xor	cx,cx
		xor	dx,dx
		xchg	cx,[mickeys.X]
		xchg	dx,[mickeys.Y]
		pop	ds
		iret
;	end_
@@c27nz:

;========================================================================
; 31 - Get current virtual cursor coordinates
; Out:	[AX]			(min virtual cursor X)
;	[BX]			(min virtual cursor Y)
;	[CX]			(max virtual cursor X)
;	[DX]			(max virtual cursor Y)
; Use:	rangemin, rangemax
;
cursrange_31:	cmp	al,31h
;	if_ eq
	jnz @@c31nz
		mov	ax,[rangemin.X]
		mov	bx,[rangemin.Y]
		lds	cx,[rangemax]
		mov	dx,ds
		pop	ds
		iret
;	end_
@@c31nz:

;========================================================================
; 32 - Get supported advanced functions flag
; Out:	[AX]			(bits 15-0=function 25h-34h supported)
;	[CX] = 0
;	[DX] = 0
;	[BX] = 0
;
active_32:	cmp	al,32h
;	if_ eq
	jnz @@a32nz
if USE28
		mov	ax,0111010000001100b	; active: 26 27 28 2A 31 32
else
		mov	ax,0110010000001100b	; active: 26 27 2A 31 32
endif
		xor	cx,cx
		xor	dx,dx
@iretBX0:	xor	bx,bx
		pop	ds
		iret
;	end_
@@a32nz:

;========================================================================
; 4D - Get pointer to copyright string
; Out:	[ES:DI]			(copyright string)
; Use:	IDstring
;
copyright_4D:	cmp	al,4Dh
;	if_ eq
	jnz @@c4dnz
		MOVSEG	es,cs,,@TSRcode
		mov	di,TSRcref:IDstring
;	end_
@@c4dnz:

;========================================================================
; 6D - Get pointer to version
; Out:	[ES:DI]			(version string)
; Use:	msversion
;
version_6D:	cmp	al,6Dh
;	if_ eq
	jnz @@v6dnz
		MOVSEG	es,cs,,@TSRcode
		mov	di,TSRcref:msversion
;	end_
@@v6dnz:

if USE28
;========================================================================
; 28 - Set video mode
; In:	CX			(video mode)
;	DX			(font size, ignored)
; Out:	[CX]			(=0 if successful)
; Modf:	none
; Call:	none
;
setvidmode_28:	cmp	al,28h
;	if_ eq
	jnz @@v28nz
		push	ax bx bp		;!!! some BIOSes trash BP
		test	ch,ch			; VESA mode >= 100h
;	 if_ zero
	jnz @@v28nz2
		mov	ax,cx
		;mov	ah,0
		int	10h			; set the video mode in AL
		mov	ah,0Fh
		int	10h			; get current video mode
		cmp	al,cl			; CL=requested video mode
		je	@@setmoderet0		; return if successful
;	 end_
@@v28nz2:
		mov	bx,cx
		mov	ax,4F02h
		int	10h			; set VESA video mode in BX
		cmp	ax,004Fh		; CX=requested video mode
		jne	@@setmoderet

@@setmoderet0:	xor	cx,cx			; CX=0 if successful
@@setmoderet:	pop	bp bx ax		; CX=requested mode or 0
;	end_
@@v28nz:
endif

;========================================================================
; 2A - Get cursor hot spot
; Out:	[AX]			(cursor visibility counter)
;	[BX]			(hot spot X)
;	[CX]			(hot spot Y)
;	[DX] = 1=bus/2=serial/3=InPort/4=PS2/5=HP (mouse type)
; Use:	nocursorcnt, hotspot
;
hotspot_2A:	cmp	al,2Ah
;	if_ eq
	jnz @@h2anz
		;mov	ah,0
		mov	al,[nocursorcnt]
		lds	bx,[hotspot]
		mov	cx,ds
;	CODE_	MOV_DX	mouseinfo1,<db 4,0>
		OPCODE_MOV_DX
mouseinfo1	db 4,0
;	end_
@@h2anz:

		pop	ds
		iret
handler33	endp

;======================== END OF INT 33 SERVICES ========================


RILversion	label byte
msversion	db driverversion / 100h,driverversion mod 100h
IDstring	db 'CuteMouse ',CTMVER,0
szIDstring = $ - IDstring

TSRend		label byte


;======================= INITIALIZATION PART DATA =======================

.const

; messages segment virtual ; place at the end of current bsegment
%include ?LANG.msg
; messages ends

S_mousetype	dw dataref:S_atPS2
		dw dataref:S_inMSYS
		dw dataref:S_inLT
		dw dataref:S_inMS
		dw dataref:S_inPM

.data

options		dw 0
OPT_PS2		equ	    1b
OPT_serial	equ	   10b
OPT_COMforced	equ	  100b
OPT_PS2after	equ	 1000b
OPT_3button	equ	10000b
OPT_MSYS	equ    100000b
OPT_lefthand	equ   1000000b
OPT_noUMB	equ  10000000b

OPT_newTSR	equ 100000000b
OPT_Wheel	equ 1000000000b


;============================== REAL START ==============================

.code

say		macro	stroff ; :vararg
;		MOVOFF_	di,<stroff>
		mov	di,offset stroff
		call	sayASCIIZ
endm

real_start:	cld
		DOSGetIntr 33h
		saveFAR [oldint33],es,bx	; save old INT 33h handler

;----- parse command line and find mouse

		say	@data:Copyright		; 'Cute Mouse Driver'
		mov	si,offset PSP:cmdline_len
		lodsb
		cbw				; OPTIMIZE: instead MOV AH,0
		mov	bx,ax
		mov	[si+bx],ah		; OPTIMIZE: AH instead 0
		call	commandline		; examine command line

		mov	al,1Fh			; disable old driver
		call	mousedrv

if PicoMEM
		mov ax,6000h	; PicoMEM Function 0 (Detect)
		mov dx,1234h	; To be added to be sure
		int 13h
		cmp dx,0AA55h
		jne @@NoPicomem
		say	@data:S_PM_Det

		mov ax,6010h	; PicoMEM Function 10h (Enable Mouse)
		mov dx,1234h	; To be added to be sure
		int 13h
		
		jmp @@PMDetectEnd
@@NoPicomem:
		say	@data:S_PM_NotDet
@@PMDetectEnd:

endif

;-----

		mov	ax,[options]
;		testflag ax,OPT_PS2+OPT_serial
		test	al, OPT_PS2+OPT_serial	; value is 3, TASM optimizes
;	if_ zero				; if no /S and /P then
	jnz @@sonz
;		setflag	ax,OPT_PS2+OPT_serial	;  both PS2 and serial assumed
		or al, OPT_PS2+OPT_serial	; value is 3, TASM optimizes
;	end_
@@sonz:
;---
;		testflag ax,OPT_PS2after
		test al, OPT_PS2after		; 8, TASM optimizes this
;	if_ nz
	jz @@paz
		call	searchCOM		; call if /V
		jnc	@@serialfound
;	end_
@@paz:
;---
;		testflag ax,OPT_PS2+OPT_PS2after
		test al, OPT_PS2+OPT_PS2after	; 9, TASM optimizes this
;	if_ nz
	jz @@p2z
		push	ax
		call	checkPS2		; call if /V or PS2
		pop	ax
;	andif_ nc
	jc @@p2z
		mov	mouseinfo[0],bh
		j	@@mousefound
;	end_
@@p2z:
;---
;		testflag ax,OPT_PS2after
 		test al, OPT_PS2after	; 8, TASM optimizes this
;	if_ zero
	jnz @@panz
;		testflag ax,OPT_serial+OPT_MSYS	; 2008: better than +nomsys?
		test al, OPT_serial+OPT_MSYS	; 22h, TASM optimizes this
;	andif_ nz
	jz @@panz
		call	searchCOM		; call if no /V and serial
		jnc	@@serialfound
;	end_
@@panz:
if PICOMEM
        mov	bl,4 				; 4 Will Display PicoMEM
		jmp @@mousefound
endif
		mov	di,dataref:E_notfound	; 'Error: device not found'
		jmp	EXITENABLE

;-----

@@serialfound:	;push	ax			; preserve OPT_newTSR value
		mov	al,2
		mov	mouseinfo[1],al
		mov	[mouseinfo1],al
		;pop	ax
@@mousefound:	mov	[mousetype],bl

;----- check if CuteMouse driver already installed

;		testflag ax,OPT_newTSR
		test ah, OPT_newTSR shr 8	; ax,100h, TASM optimizes
		jnz	@@newTSR
		call	getCuteMouse
		mov	di,dataref:S_reset	; 'Resident part reset to'
		mov	cx,4C02h		; terminate, al=return code

;	if_ ne
	jz @@mfz

;----- allocate UMB memory, if possible, and set INT 33 handler

@@newTSR:	mov	bx,(TSRend-TSRstart+15)/16
		push	bx
		call	prepareTSR		; new memory segment in ES
		memcopy	<size oldint33>,es,,@TSRdata:oldint33,ds,,@TSRdata:oldint33
		push	ds
		MOVSEG	ds,es,,@TSRcode
		mov	[disabled?],1		; copied back in setupdriver
		DOSSetIntr 33h,,,@TSRcode:handler33
		POPSEG	ds,@data
		pop	ax
		mov	di,dataref:S_installed	; 'Installed at'
		mov	cl,0			; errorlevel
;	end_ if
@@mfz:

;-----

		push	ax			; size of TSR for INT 21/31
;		say	di
		call    sayASCIIZ
		mov	al,[mousetype]

		mov	bx,dataref:S_CRLF
		add	al,al
;	if_ carry				; if wheel (=8xh)
	jnc @@mfnc
		mov	bx,dataref:S_wheel
;	end_
@@mfnc:

		cbw				; OPTIMIZE: instead MOV AH,0
		cmp	al,1 shl 1
		xchg	si,ax			; OPTIMIZE: instead MOV SI,AX
;	if_ ae					; if not PS/2 mode (=0)
	jb @@mfb
;	 if_ eq					; if Mouse Systems (=1)
	 jnz @@mfnz
		inc	cx			; OPTIMIZE: CX instead CL
;	 end_
@@mfnz:
		say	@data:S_atCOM
;	end_
@@mfb:
		push	cx			; exit function and errorlevel
;		say	S_mousetype[si]
		mov	di, S_mousetype[si]
		call	sayASCIIZ
;		say	bx
		mov	di,bx
		call    sayASCIIZ
		call	setupdriver

;----- close all handles (20 pieces) to prevent decreasing system
;	pool of handles if INT 21/31 used

		mov	bx,19
;	loop_
@@mfns:
;		DOSCloseFile
		mov	ah,3eh
		int	21h
		dec	bx
;	until_ sign
	jns @@mfns

;-----

		pop	ax			; AH=31h (TSR) or 4Ch (EXIT)
		pop	dx
		int	21h

;========================================================================
;		Setup resident driver code and parameters
;========================================================================

setupdriver	proc

;----- detect VGA card (VGA ATC have one more register)

		mov	ax,1A00h
		int	10h			; get display type in BX
		cmp	al,1Ah
; if USERIL					; -X-
;	if_ eq
	jnz @@sdnz
		xchg	ax,bx			; OPTIMIZE: instead MOV AL,BL
		sub	al,7
		cmp	al,8-7
;	andif_ be					; if monochrome or color VGA
	ja @@sdnz
if USERIL
		inc	videoregs@[(size RGROUPDEF)*3].regscnt
endif
;	end_
@@sdnz:
; else
if 1	; -X-
		; *** SAY error if AL not 1ah: need VGA as we have no RIL ***
		jz @havevga
		say E_needvga
@havevga:
endif						; -X-
; endif						; -X- USERIL

;----- setup left hand mode handling

;	CODE_	MOV_CX	mousetype,<db ?,0>	; 0=PS/2,1=MSys,2=LT,3=MS,
		OPCODE_MOV_CX
mousetype	db ?,0
						; 80h=PS/2+wheel,83h=MS+wheel
		test	cl,7Fh
		mov	al,00000000b	; =0
;	if_ nz					; if not PS/2 mode (=x0h)
	jz @@mtz
		mov	al,00000011b	; =3
;	end_
@@mtz:
;		testflag [options],OPT_lefthand
		test byte ptr [options], OPT_lefthand	; 40h, TASM optimizes
;	if_ nz
	jz @@mtz2
		xor	al,00000011b	; =3
;	end_
@@mtz2:
		mov	[swapmask],al

;----- setup buttons count, mask and wheel flags

		mov	al,3
;		testflag [options],OPT_3button
		test byte ptr [options], OPT_3button	; 10h, TASM optimizes
;	if_ zero
	jnz @@mtnz
		jcxz	@@setbuttons		; jump if PS/2 mode (=0)
		cmp	cl,al			; OPTIMIZE: AL instead 3
;	andif_ eq				; if MS mode (=3)
	jnz @@mtnz
@@setbuttons:	mov	[buttonsmask],al	; OPTIMIZE: AL instead 0011b
		dec	ax
		mov	[buttonscnt],al		; OPTIMIZE: AL instead 2
;	end_
@@mtnz:

		cmp	cl,80h			; if some wheel protocol
;	if_ ae
	jb @@mtb
		inc	wheelflags[0]		; report "wheel present"
		mov	[wheelmask],00001111b	; =0Fh
;	end_
@@mtb:

;----- setup mouse handlers code (Modify the code)
;	block_
		test	cl,7Fh
;	 breakif_ zero				; break if PS/2 mode (=x0h)
	jz @@mtblock

; -X-		fixcode	IRQproc,0B0h,%OCW2<OCW2_EOI> ; MOV AL,OCW2<OCW2_EOI>
		fixnear	enableproc,enableUART
		fixnear	disableproc,disableUART
		dec	cx
;	 breakif_ zero				; break if Mouse Systems mode (=1)
	jz @@mtblock

		fixnear	mouseproc,MSLTproc
		dec	cx
;	 breakif_ zero				; break if Logitech mode (=2)
	jz @@mtblock

		fixcode	MSLTCODE3,,2
		loop	@@setother		; break if wheel mode (=83h)

		cmp	al,2			; OPTIMIZE: AL instead [buttonscnt]
;	 if_ ne					; if not MS2
	jz @@mtbz
		fixcode	MSLTCODE2,075h		; JNZ
;	 end_
@@mtbz:
		mov	al,0C3h			; RET
		fixcode	MSLTCODE1,al
		fixcode	MSLTCODE3,al
;	end_ block
@@mtblock:

;----- setup, if required, other parameters

@@setother:	push	es
		push	ds
		push	es
		push	ds
		pop	es
		pop	ds			; get back [oldint10]...
		memcopy	<size oldint10>,es,,@TSRdata:oldint10,ds,,@TSRdata:oldint10
		mov	al,[disabled?]
		pop	ds
		mov	[disabled?],al		; ...and [disabled?]

		DOSGetIntr [IRQintnum]		; -X- can be 0, none
		mov	ax,es
		pop	es
		mov	di,TSRdref:oldIRQaddr
		xchg	ax,bx
		stosw				; save old IRQ handler
		xchg	ax,bx			; OPTIMIZE: instead MOV AX,BX
		stosw

;----- copy TSR image (even if ES=DS - this is admissible)

		memcopy	((TSRend-TSRdata+1)/2)*2,es,,@TSRdata:TSRdata,ds,,@TSRdata:TSRdata

;----- call INT 33/0000 (Reset driver)

		pop	ax
		pushf				;!!! Logitech MouseWare
		push	cs
		push	ax			;  Windows driver workaround
		mov	ax,TSRcref:handler33
		push	es
		push	ax
		xor	ax,ax			; reset driver
		nop				; -X-
		retf				; jump to relocated INT33
setupdriver	endp

;========================================================================
;		Check given or all COM-ports for mouse connection
;========================================================================

searchCOM	proc
		;mov	[LCRset],LCR<0,,LCR_noparity,0,2>
		mov	di,coderef:detectmouse
		call	COMloop
		jnc	@searchret

;		testflag [options],OPT_MSYS
		test byte ptr [options], OPT_MSYS	; 20h, TASM optimizes
		stc
		jz	@searchret

;		mov	[LCRset],LCR<0,,LCR_noparity,0,3>
		mov	[LCRset],3
		mov	bl,1			; =Mouse Systems mode
		mov	di,coderef:checkUART
		;j	COMloop
searchCOM	endp

;========================================================================

COMloop		proc
		push	ax
		xor	ax,ax			; scan only current COM port
;		testflag [options],OPT_COMforced
		test byte ptr [options], OPT_COMforced	; 4, TASM optimizes
		jnz	@@checkCOM
		mov	ah,3			; scan all COM ports

;	loop_
@@ccns:
		inc	ax			; OPTIMIZE: AX instead AL
		push	ax
		call	setCOMport
		pop	ax
@@checkCOM:	push	ax
		mov	si,[IO_address]
		call	di
		pop	ax
		jnc	@@searchbreak
		dec	ah
;	until_ sign
	    jns @@ccns
		;stc				; preserved from prev call

@@searchbreak:	pop	ax
@searchret::	ret
COMloop		endp

;========================================================================
;			Check if UART available
;========================================================================
;
; In:	SI = [IO_address]
; Out:	Carry flag		(no UART detected)
; Use:	none
; Modf:	AX, DX
; Call:	none
;
checkUART	proc
		test	si,si
		 jz	@@noUART		; no UART if base=0

;----- check UART registers for reserved bits

		movidx	dx,MCR_index,si		; {3FCh} MCR (modem ctrl reg)
		 in	ax,dx			; {3FDh} LSR (line status reg)
;		testflag al,mask MCR_reserved+mask MCR_AFE
		test al, mask MCR_reserved+mask MCR_AFE
		 jnz	@@noUART
		movidx	dx,LSR_index,si,MCR_index
		 in	al,dx			; {3FDh} LSR (line status reg)
		inc	ax
		 jz	@@noUART		; no UART if AX was 0FFFFh

;----- check LCR function

		cli
		movidx	dx,LCR_index,si,LSR_index	; "dec dx dec dx"
		 in	al,dx			; {3FBh} LCR (line ctrl reg)
		 push	ax
;		out_	dx,%LCR<1,0,-1,-1,3>	; {3FBh} LCR: DLAB on, 8S2
;		mov	al,%LCR<1,0,-1,-1,3>
		mov	al,10111111b
		out	dx,al
		 inb	ah,dx
;		out_	dx,%LCR<0,0,0,0,2>	; {3FBh} LCR: DLAB off, 7N1
;		mov	al,%LCR<0,0,0,0,2>
		mov	al,00000010b
		out	dx,al
		 in	al,dx
		sti
;		sub	ax,(LCR<1,0,-1,-1,3> shl 8)+LCR<0,0,0,0,2>
		sub	ax, (10111111b shl 8) + 00000010b

;	if_ zero				; zero if LCR conforms
	jnz @@lcrnz

;----- check IER for reserved bits

		movidx	dx,IER_index,si,LCR_index
		 in	al,dx			; {3F9h} IER (int enable reg)
		movidx	dx,LCR_index,si,IER_index
		;mov	ah,0
		and	al,mask IER_reserved	; reserved bits should be clear
;	end_ if
@@lcrnz:

		neg	ax			; nonzero makes carry flag
		pop	ax
		 out	dx,al			; {3FBh} LCR: restore contents
		ret

@@noUART:	stc
		ret
checkUART	endp

;========================================================================
;			Detect mouse type if present
;========================================================================
;
; In:	SI = [IO_address]
; Out:	Carry flag		(no UART or mouse found)
;	BX			(mouse type: 2=Logitech,3=MS,83h=MS+wheel)
; Use:	0:46Ch, LCRset
; Modf:	AX, CX, DX, ES
; Call:	checkUART
;
detectmouse	proc
		call checkUART
		jnc	@@detmokay
		jmp	@@detmret
@@detmokay:

;----- save current LCR/MCR

		movidx	dx,LCR_index,si		; {3FBh} LCR (line ctrl reg)
		 in	ax,dx			; {3FCh} MCR (modem ctrl reg)
		 push	ax			; keep old LCR and MCR values

;----- reset UART: drop RTS line, interrupts and disable FIFO

		;movidx	dx,LCR_index,si		; {3FBh} LCR: DLAB off
;		 out_	dx,%LCR<>,%MCR<>	; {3FCh} MCR: DTR/RTS/OUT2 off
;		mov	al,0	; %LCR<>
;		mov	ah,0	; %MCR<>
	xor ax,ax	; the out_ macro optimizes this
		out	dx,ax
		movidx	dx,IER_index,si,LCR_index
		 ;mov	ax,(FCR<> shl 8)+IER<>	; {3F9h} IER: interrupts off
		 out	dx,ax			; {3FAh} FCR: disable FIFO

;----- set communication parameters and flush receive buffer

		movidx	dx,LCR_index,si,IER_index
;		 out_	dx,%LCR{LCR_DLAB=1}	; {3FBh} LCR: DLAB on
;		mov	al,%LCR{LCR_DLAB=1} 
		mov	al,80h
		out	dx,al
;		xchg	dx,si
		xchg	si,dx	; TASM and JWASM use opposite encodings
		 ;mov	ah,0			; 1200 baud rate
;		 out_	dx,96,ah		; {3F8h},{3F9h} divisor latch
		mov	al,96
		out	dx,ax
;		xchg	dx,si
		xchg	si,dx	; TASM and JWASM use opposite encodings
;		 out_	dx,[LCRset]		; {3FBh} LCR: DLAB off, 7/8N1
		mov	al,[LCRset]
		out	dx,al
		movidx	dx,RBR_index,si,LCR_index
		 in	al,dx			; {3F8h} flush receive buffer

;----- wait current+next timer tick and then raise RTS line

		MOVSEG	es,0,ax,BIOS
;	loop_
@@tmrnz:
		mov	ah,byte ptr [BIOS_timer]
;	 loop_
@@tmrz:
		cmp	ah,byte ptr [BIOS_timer]
;	 until_ ne				; loop until next timer tick
	jz @@tmrz
		xor	al,1
;	until_ zero				; loop until end of 2nd tick
	jnz @@tmrnz

		movidx	dx,MCR_index,si,RBR_index
;		 out_	dx,%MCR<,,,0,,1,1>	; {3FCh} MCR: DTR/RTS on, OUT2 off
;		mov	al,%MCR<,,,0,,1,1>
		mov al,00000011b
		out	dx,al

;----- detect if Microsoft or Logitech mouse present

		mov	bx,0103h		; bl=mouse type, bh=no `M'
;	countloop_ 4,cl				; scan 4 first bytes
	mov cl,4
@@clloop:
;	 countloop_ 2+1,ch			; length of silence in ticks
	mov ch,2+1
@@chloop:
						; (include rest of curr tick)
		mov	ah,byte ptr [BIOS_timer]
;	  loop_
@@zloop:
		movidx	dx,LSR_index,si
		 in	al,dx			; {3FDh} LSR (line status reg)
;		testflag al,mask LSR_RBF
		test al, mask LSR_RBF
		 jnz	@@parse			; jump if data ready
		cmp	ah,byte ptr [BIOS_timer]
;	  until_ ne				; loop until next timer tick
	jz @@zloop
;	 end_ countloop				; loop until end of 2nd tick
	dec ch
	jnz @@chloop
; 	 break_					; break if no more data
	jmp short @@clloopend

@@parse:	movidx	dx,RBR_index,si
		 in	al,dx			; {3F8h} receive byte
		cmp	al,'('-20h
;	 breakif_ eq				; break if PnP data starts
	jz @@clloopend
		cmp	al,'M'			; PnP: microsoft?
;	 if_ eq
	jnz @@prsnz
		mov	bh,0			; MS compatible mouse found...
;	 end_
@@prsnz:
		cmp	al,'Z'			; PnP: wheel?
;	 if_ eq
	jnz @@prsnz2
; * Only for PS2, wheel detection is a risk, so never disable COM wheel check
; *		testflag [options],OPT_Wheel
; *	if_ nz
		mov	bl,83h			; ...MS mouse+wheel found
; *	end_	; else leave bx = 103 (from above) = MS without wheel
;	 end_
@@prsnz2:
		cmp	al,'3'			; PnP: logitech?
;	 if_ eq
	jnz @@prsnz3
		mov	bl,2			; ...Logitech mouse found
;	 end_
@@prsnz3:
;	end_ countloop
	dec cl
	jnz @@clloop
@@clloopend:

		movidx	dx,LCR_index,si
		 pop	ax			; {3FBh} LCR: restore contents
		 out	dx,ax			; {3FCh} MCR: restore contents

		shr	bh,1			; 1 makes carry flag
@@detmret:	ret
detectmouse	endp

;========================================================================
;				Check for PS/2
;========================================================================
;
; In:	none
; Out:	Carry flag		(no PS/2 device found)
;	BL			(mouse type: 0=PS/2)
;	BH			(interrupt #/0=PS/2)
; Use:	none
; Modf:	AX, CX, BX
; -X- did the old version change ES?
; Call:	INT 11, INT 15/C2xx, setRate
; -X- replaced outKBD, flushKBD by int 15/C2xx calls (KoKo/Eric)
; After reset with C201: "disabled, 100 Hz, 4 c/mm, 1:1", protocol unchanged
;
if PM_KeepALL
checkPS2	proc
	jmp short @@ps2foo
@@noPS2y:	jmp	@@noPS2			; no supported type
@@ps2foo:
		int	11h			; get equipment list
;		testflag al,mask HW_PS2
		test al, mask HW_PS2
		jz	@@noPS2y		; jump if PS/2 not indicated
		mov	bh,3			; standard 3 byte packets
		PS2serv 0C205h,@@noPS2y		; initialize mouse, bh=datasize
		mov	bh,3			; resolution: 2^bh counts/mm
		PS2serv 0C203h,@@noPS2x		; set mouse resolution bh
		PS2serv 0C204h,@@PS2valid1	; get type (KoKo)
		; Dell Inspirion 1501 touchpad fails w/ i/o error on c204
		; but works as a 2 button non-PnP mouse with ctmouse 1.9
@@isPS2pnp:	cmp	bh,0FFh			; KoKo: ?
		jz	@@PS2valid1
		cmp	bh,0AAh			; Dosemu
		jz	@@PS2valid1
		cmp	bh,0			; KoKo: plain
		jz	@@PS2valid1
		cmp	bh,3			; KoKo: wheel
		jz	@@PS2valid1
		cmp	bh,4			; KoKo: plain? wheel?
		jz	@@PS2valid1
@@noPS2x:	jmp	@@noPS2			; no supported type
@@dummyPS2:	retf				; no-op callback handler
@@PS2valid1:	push	cs
		pop	es
		mov	bx,coderef:@@dummyPS2
		PS2serv	0C207h,@@noPS2x		; can we set a handler?
		MOVSEG	es,0,bx,nothing
		PS2serv	0C207h			; clear mouse handler (ES:BX=0)

;		testflag [options],OPT_Wheel	; dare to try PS2 wheel?
		test byte ptr [options+1], OPT_Wheel shr 8	; TASM optimizes
;	if_ nz
	jz @@ps2wz
;----- select IntelliMouse Z wheel + 3 button mode, via magic rate handshake

		mov	ah,200
		call	setRate
		jc	@@noPS2z
		mov	ah,100
		call	setRate
		jc	@@noPS2z
		mov	ah,80
		call	setRate			; 200->100->80 rate does this
		jc	@@noPS2z
;	end_
@@ps2wz:

;----- check if successful

if 0
		mov	dx,0D464h		; enable mouse functions
		call	outKBD
		mov	dx,0F260h		; get ID
		call	flushKBD		; returns ah
else
		PS2serv 0C204h,@@PS2nonPnP	; get type (KoKo)
		mov	ah,bh
endif

		cmp	ah,0FFh			; ?
		jz	@@PS2valid2
		cmp	ah,0AAh			; Dosemu
		jz	@@PS2valid2
		cmp	ah,0			; plain
		jz	@@PS2valid2
		cmp	ah,3			; wheel
		jz	@@PS2valid2
		cmp	ah,4			; plain? wheel?
		jz	@@PS2valid2
		jmp	@@noPS2			; no supported type
@@noPS2z:	jmp	@@noPS2
@@PS2nonPnP:	; see note about Dell Inspiron 1501 above
		mov	ah,0			; assume plain
@@PS2valid2:
		xor	bx,bx			; =PS/2 mouse found
		cmp	ah,3			; ID=3 -> 3 button+wheel mode
;	if_ eq
	jnz @@ps2w2
;		testflag [options],OPT_Wheel
		test byte ptr [options+1], OPT_Wheel shr 8 ; TASM optimizes
;	if_ nz		; patch a jump short from to PS2PLAIN into to PS2WHEEL:
	jz @@ps2w2
;		mov	PS2WHEELCODE[1], PS2WHEEL - PS2WHEELCODE - 2
; ...		push	bx
; ...		mov	bx,offset PS2WHEELCODE + 2	; enable wheel
; ...		mov	word ptr cs:[bx], -1	; make test return NZ
; ...		pop	bx
		mov	byte ptr [PS2WHEELCODE+1],PS2WHEEL-PS2WHEELCODE-2
		mov	bl,80h			; =PS/2+wheel mouse

		push	ax
		push	bx
		mov	bh,4			; packet size in bytes
		PS2serv 0C205h,@@noPS2w		; -X- KoKo: 4 byte protocol
		mov	ah,200			; -X- KoKo: repeat magic...
		call	setRate
		jc	@@noPS2w
		mov	ah,100
		call	setRate
		jc	@@noPS2w
		mov	ah,80
		call	setRate
		jc	@@noPS2w
		pop	bx
		pop	ax
;	end_
;	end_
@@ps2w2:

;-----

; -X-		mov	byte ptr [IRQintnum],68h+12
		; no handler yet, enabledriver_20 does int 15/C207, 15/C200
		clc
		ret
@@noPS2w:	pop	bx
		pop	ax
@@noPS2:	stc
		ret
checkPS2	endp
endif ;KeepAll

;========================================================================

; Set mouse sampling rate to AH samples per second
; -X- old version changed DX and (!) ES
setRate		proc
if 0
		push	ax
		mov	dx,0D464h		; enable mouse functions
		call	outKBD
		mov	dx,0F360h		; set rate...
		call	flushKBD		; should be 0FAh (ACK) or 0FEh (resend)

		mov	dx,0D464h		; enable mouse functions
		call	outKBD
		pop	dx
		mov	dl,60h			; ...value
		;j	flushKBD		; should be 0FAh (ACK) or 0FEh (resend)
else
		push	ax
		mov	al,6
		cmp 	ah,200
		jz	@@RATEFOUND
		dec	ax			; OPTIMIZE: instead of AL
		cmp	ah,100
		jz	@@RATEFOUND
		dec	ax			; OPTIMIZE: instead of AL
		cmp	ah,80
		jz	@@RATEFOUND
		dec	ax			; OPTIMIZE: instead of AL
		cmp	ah,60
		jz	@@RATEFOUND
		dec	ax			; OPTIMIZE: instead of AL
		cmp	ah,40
		jz	@@RATEFOUND
		dec	ax			; OPTIMIZE: instead of AL
		cmp	ah,20
		jz	@@RATEFOUND
		dec	ax			; OPTIMIZE: instead of AL
		cmp	ah,10
		jz	@@RATEFOUND
		; else: invalid rate selected, using 10 Hz :-p
@@RATEFOUND:	push	bx
		mov	bh,al			; the rate
		PS2serv	0C202h,@@RATEERR	; set rate
		clc
		pop	bx
		pop	ax
		ret
@@RATEERR:	stc
		pop	bx
		pop	ax
		ret
endif
SetRate		endp

;========================================================================
; In:	DL			(port)
;	DH			(value)
; Out:	AH
; Use:	0:46Ch
; Modf:	AL, DX, ES
; Call:	outKBD
;
if 0
flushKBD	proc
		call	outKBD

		mov	ah,0
@@inKBDstart:	MOVSEG	es,0,dx,BIOS
	loop_
		mov	dl,byte ptr [BIOS_timer]
	 loop_
		in	al,64h			; keyboard status register
		test	al,00000001b	; =1
	  if_ nz				; if read buffer full
		 inb	ah,60h
		 j	@@inKBDstart		; wait next byte
	  end_
		cmp	dl,byte ptr [BIOS_timer]
	 until_ ne				; loop until next timer tick
		xor	dh,1
	until_ zero				; loop until end of 2nd tick
		clc
		ret
flushKBD	endp
endif

;========================================================================
;			Send command to auxiliary device
;========================================================================
;
; In:	DL			(port)
;	DH			(value)
; Out:	none
; Use:	0:46Ch
; Modf:	AL, DH, ES
; Call:	none
;
if 0
outKBD		proc
		push	dx
		MOVSEG	es,0,dx,BIOS
	loop_
		mov	dl,byte ptr [BIOS_timer]
	 loop_
		in	al,64h			; keyboard status register
		test	al,00000010b	; =2	; check if we can send data
		 jz	@@outKBD		; jump if write buffer empty
		cmp	dl,byte ptr [BIOS_timer]
	 until_ ne				; loop until next timer tick
		xor	dh,1
	until_ zero				; loop until end of 2nd tick

@@outKBD:	pop	dx
		mov	al,dh
		mov	dh,0
		out	dx,al
		ret
outKBD		endp
endif

;========================================================================
;				Set COM port
;========================================================================
;
; In:	AL			(COM port, 1-4)
; Out:	none
; Use:	0:400h
; Modf:	AL, CL, ES, com_port, IO_address, S_atIO
; Call:	setIRQ
;
setCOMport	proc
		push	ax
		push	di
		add	al,'0'
		mov	[com_port],al

		cbw				; OPTIMIZE: instead MOV AH,0
		xchg	di,ax			; OPTIMIZE: instead MOV DI,AX
		MOVSEG	es,0,ax,BIOS
		add	di,di
		mov	ax,COM_base[di-'1'-'1']
		mov	[IO_address],ax

		mov	di,dataref:S_atIO	; string for 4 digits
		MOVSEG	es,ds,,@data
		_word_hex

		pop	di
		pop	ax
		and	al,1			; 1=COM1/3, 0=COM2/4
		add	al,3			; IRQ4 for COM1/3
		;j	setIRQ			; IRQ3 for COM2/4
setCOMport	endp

;========================================================================
;				Set IRQ number
;========================================================================
;
; In:	AL			(IRQ#, 1-7)
; Out:	none
; Use:	none
; Modf:	AL, CL, IRQno, mouseinfo, IRQintnum, PIC1state, notPIC1state
; Call:	none
;
setIRQ		proc
		add	al,'0'
		mov	[IRQno],al
		sub	al,'0'
		mov	mouseinfo[0],al
		mov	cl,al
		add	al,8			; INT=IRQ+8
		mov	[IRQintnum],al
		mov	al,1
		shl	al,cl			; convert IRQ into bit mask
		mov	[PIC1state],al		; PIC interrupt disabler
		not	al
		mov	[notPIC1state],al	; PIC interrupt enabler
		ret
setIRQ		endp

;========================================================================
;		Check if CuteMouse driver is installed
;========================================================================
;
; In:	none
; Out:	Zero flag		(ZF=1 if installed)
;	ES			(driver segment)
; Use:	IDstring
; Modf:	AX, CX, SI, DI
; Call:	mousedrv
;
getCuteMouse	proc
		xor	di,di
		mov	al,4Dh			; get copyright string
		call	mousedrv
		mov	si,TSRcref:IDstring
		cmp	di,si
;	if_ eq
	jnz @@gcmnz
		mov	cx,szIDstring
		repe	cmpsb
;	end_
@@gcmnz:
		ret
getCuteMouse	endp

;========================================================================
;			Call mouse driver if present
;========================================================================
;
; In:	AL			(function)
; Out:	results of INT 33h, if driver installed
; Use:	oldint33
; Modf:	AH
; Call:	INT 33
;
mousedrv	proc
		mov	cx,word ptr oldint33[2]
;	if_ ncxz
	jcxz @@mdcxz
		mov	ah,0
		pushf				;!!! Logitech MouseWare
		call	[oldint33]		;  Windows driver workaround
;	end_
@@mdcxz:
		ret
mousedrv	endp


;========================= COMMAND LINE PARSING =========================

;========================================================================
;			Parse Serial option
;========================================================================

_serialopt	proc
		mov	bx,(4 shl 8)+1
		call	parsedigit
;	if_ nc					; '/Sc' -> set COM port
	jc @@soc
;		setflag	[options],OPT_COMforced
		or byte ptr [options], OPT_COMforced	; TASM optimizes
		call	setCOMport

		;mov	bl,1
		mov	bh,7
		call	parsedigit
		jnc	setIRQ			; '/Sci' -> set IRQ line
;	end_
@@soc:
		ret
_serialopt	endp

;========================================================================
;			Parse Resolution option
;========================================================================

_resolution	proc
		;mov	ah,0
		mov	bx,(9 shl 8)+0
		call	parsedigit		; first argument
;	if_ nc
	jc @@resc
		mov	ah,al
		;mov	bx,(9 shl 8)+0
		call	parsedigit		; second argument
		jnc	@@setres		; jump if digit present
;	end_
@@resc:
		mov	al,ah			; replicate missing argument

@@setres:	add	ax,0101h
		push	ax			; AL=RY+1, AH=RX+1
		mov	al,10
		mul	ah			; AX=10*(RX+1)
		mov	bx,offset POINT.X+(DefArea-SaveArea)
		call	senscalc
		pop	ax
		mov	ah,10
		mul	ah			; AX=10*(RY+1)
		mov	bx,offset POINT.Y+(DefArea-SaveArea)
		jmp	senscalc
_resolution	endp

;========================================================================
; In:	DS:SI			(string pointer)
;	BL			(low bound)
;	BH			(upper bound)
; Out:	DS:SI			(pointer after digit)
;	Carry flag		(no digit)
;	AL			(digit if no carry)
; Use:	none
; Modf:	CX
; Call:	BADOPTION
;
parsedigit	proc
		lodsb
		;_ch2digit
		sub	al,'0'
		cmp	al,bh
;	if_ be
	ja @@pda
		cmp	al,bl
		jae	@ret			; JAE mean CF=0
;	end_
@@pda:
		cmp	al,10
		mov	cx,dataref:E_argument	; 'Error: Invalid argument'
		jb	BADOPTION		; error if decimal digit
		dec	si
		stc
@ret::		ret
parsedigit	endp

;========================================================================
;		Check if mouse services already present
;========================================================================

_checkdriver	proc
		mov	cx,word ptr oldint33[2]
		jcxz	@ret
		;mov	ah,0
		mov	al,21h			; OPTIMIZE: AL instead AX
		int	33h
		inc	ax
		jnz	@ret
		mov	di,dataref:E_mousepresent ; 'Mouse service already...'
		j	EXITMSG
_checkdriver	endp

;========================================================================
.const

cmOPTION	struc	; OPTION is a reserved word in some MASM / TASM
  optchar	db ?
  optmask	dw 0
  optproc@	dw ?
cmOPTION ends

OPTABLE		cmOPTION <'P',OPT_PS2,			@ret>
		cmOPTION <'O',OPT_Wheel,			@ret>
		cmOPTION <'S',OPT_serial,			_serialopt>
		cmOPTION <'V',OPT_PS2after,		@ret>
		cmOPTION <'3' and not 20h,OPT_3button,	@ret>
		cmOPTION <'R',,				_resolution>
		cmOPTION <'L',OPT_lefthand,		@ret>
		cmOPTION <'B',,				_checkdriver>
		cmOPTION <'N',OPT_newTSR,			@ret>
		cmOPTION <'W',OPT_noUMB,			@ret>
		cmOPTION <'U',,				unloadTSR>
		cmOPTION <'?' and not 20h,,		EXITMSG>
		; ignore the old "disable mouse systems" option
		; default is now to disable old mouse systems, as
		; those can be confused with empty serial ports.
		cmOPTION <'Y',,				@ret>
		; new option to "enable mouse systems"...
		cmOPTION <'M',OPT_MSYS,			@ret>
OPTABLEend	label byte

.code

;========================================================================
; In:	DS:SI			(null terminated command line)
;
commandline	proc
;	loop_
@@clloop:
		lodsb
		test	al,al
		jz	@ret			; exit if end of command line
		cmp	al,' '
;	until_ above			; skips spaces and controls
	jbe @@clloop

		cmp	al,'/'			; option character?
;	if_ eq
	jnz @@clnz
		lodsb
		and	al,not 20h		; uppercase
		mov	di,dataref:Syntax	; 'Options:'
		mov	bx,dataref:OPTABLE
;	 loop_
@@cloloop:
		cmp	al,[bx + offset cmOPTION.optchar]
;	  if_ eq
	jnz @@clonz
		mov	ax,[bx + offset cmOPTION.optmask]
		or	[options],ax
;		call	[bx + offset cmOPTION.optproc@]
		call	[bx + cmOPTION.optproc@]
		j	commandline
;	  end_
@@clonz:
		add	bx,size cmOPTION
		cmp	bx,dataref:OPTABLEend
;	 until_ ae
	jb @@cloloop
;	end_ if
@@clnz:

		mov	cx,dataref:E_option	; 'Error: Invalid option'
BADOPTION::	say	@data:E_error		; 'Error: Invalid '
;		say	cx			; 'option'/'argument'
		mov	di,cx
		call	sayASCIIZ
		mov	di,dataref:E_help	; 'Enter /? on command line'

EXITMSG::	mov	bl,[di]
		inc	di
;		say	di
		call	sayASCIIZ
		say	@data:S_CRLF
		xchg	ax,bx			; OPTIMIZE: instead MOV AL,BL
		.exit				; terminate, al=return code
commandline	endp

;========================================================================
; In:	DS:DI			(null terminated string)
; Out:	none
; Use:	none
; Modf:	AH, DL, DI
; Call:	none
;
sayASCIIZ_	proc
;	loop_
@@sazloop:
		mov	ah,2
		int	21h		; write character in DL to stdout
		inc	di
sayASCIIZ::	mov	dl,[di]
		test	dl,dl
;	until_ zero
	jnz @@sazloop
		ret
sayASCIIZ_	endp


;============================ TSR MANAGEMENT ============================

;========================================================================
;			Unload driver and quit
;========================================================================

unloadTSR	proc
		call	getCuteMouse		; check if CTMOUSE installed
		mov	di,dataref:E_nocute	; 'CuteMouse driver is not installed!'
		jne	EXITMSG

		push	es
		mov	al,1Fh			; disable CuteMouse driver
		call	mousedrv
		mov	cx,es
		pop	es

		cmp	al,1Fh
		mov	di,dataref:E_notunload	; 'Driver unload failed...'
;	if_ eq
	jnz @@unlnz
		saveFAR	[oldint33],cx,bx
		push	ds
;		DOSSetIntr 33h,cx,,bx		; restore old int33 handler
		mov	dx,bx
		mov	ax,2533h
		mov	ds,cx
		int	21h
		pop	ds
		call	FreeMem
		mov	di,dataref:S_unloaded	; 'Driver successfully unloaded...'
;	end_
@@unlnz:

EXITENABLE::	mov	al,20h			; enable old/current driver
		call	mousedrv
		j	EXITMSG
unloadTSR	endp

;========================================================================
; Prepare memory for TSR
;
; In:	BX			(TSR size)
;	DS			(PSP segment)
; Out:	ES			(memory segment to be TSR)
;	CH			(exit code for INT 21)
; Use:	PSP:2Ch, MCB:8
; Modf:	AX, CL, DX, SI, DI
; Call:	INT 21/49, AllocUMB
;
prepareTSR	proc
		assume	ds:PSP
		mov	cx,[env_seg]
;	if_ ncxz				; suggested by Matthias Paul
	jcxz @@prepcxz
		DOSFreeMem cx			; release environment block
;	end_
@@prepcxz:
		assume	ds:@data

		call	AllocUMB
		mov	ax,ds
		mov	ch,31h			; TSR exit, al=return code
		cmp	dx,ax
;	if_ ne					; if TSR not "in place"
	jz @@prepz
		push	ds
		dec	ax			; current MCB
		dec	dx			; target MCB...
						; ...copy process name
		memcopy	8,dx,MCB,MCB:ownername,ax,MCB,MCB:ownername
		POPSEG	ds,@data
		inc	ax			; current PSP
		inc	dx			; target PSP
		mov	[MCB:ownerID],dx	; ...set owner to itself
						; ...copy current PSP
						; (this makes PC-DOS MEM happy)
		memcopy	256,dx,PSP,PSP:DOS_exit,ax,PSP,PSP:DOS_exit

		mov	ch,4Ch			; terminate, al=return code
;	end_ if
@@prepz:
		mov	es,dx
;		mov	es:[PSP:DOS_exit],cx	; memory shouldn't be
						;  interpreted as PSP
						;  (CX != 20CDh)
		mov	es:[0],cx	; JWASM complains about es:PSP:...
		ret
prepareTSR	endp


;=========================== MEMORY HANDLING ============================

;========================================================================
; Get XMS handler address
;
; In:	none
; Out:	Carry flag		(set if no XMS support)
; Use:	none
; Modf:	AX, CX, BX, XMSentry
; Call:	INT 2F/4300, INT 2F/4310
;
if 0
getXMSaddr	proc	C uses es
		DOSGetIntr 2Fh			; suggested by Matthias Paul
		mov	cx,es
		stc
;	if_ ncxz				; if INT 2F initialized
	jcxz @@gxacxz
		mov	ax,4300h
		int	2Fh			; XMS: installation check
		cmp	al,80h
		stc
;	andif_ eq				; if XMS service present
	jnz @@gxacxz
		mov	ax,4310h		; XMS: Get Driver Address
		int	2Fh
		saveFAR [XMSentry],es,bx
		clc
;	end_
@@gxacxz:
		ret
getXMSaddr	endp
endif

;========================================================================
; Save allocation strategy
;
; In:	none
; Out:	Carry flag		(no UMB link supported)
; Use:	none
; Modf:	AX, SaveMemStrat, SaveUMBLink
; Call:	INT 21/5800, INT 21/5802
;
SaveStrategy	proc
		DOSGetAlloc			; get DOS alloc strategy
		mov	[SaveMemStrat],ax
		DOSGetUMBlink			; get UMB link state
		mov	[SaveUMBLink],al
		ret
SaveStrategy	endp

;========================================================================
; Restore allocation strategy
;
; In:	none
; Out:	none
; Use:	SaveMemStrat, SaveUMBLink
; Modf:	AX, BX
; Call:	INT 21/5801, INT 21/5803
;
RestoreStrategy	proc
;	CODE_	MOV_BX	SaveMemStrat,<dw ?>
		OPCODE_MOV_BX
SaveMemStrat	dw ?
		DOSSetAlloc			; set DOS alloc strategy
;	CODE_	MOV_BX	SaveUMBLink,<db ?,0>
		OPCODE_MOV_BX
SaveUMBLink	db ?,0
		DOSSetUMBlink			; set UMB link state
		ret
RestoreStrategy	endp

;========================================================================
; Allocate high memory
;
; In:	BX			(required memory size in para)
;	DS			(current memory segment)
; Out:	DX			(seg of new memory or DS)
; -X- Use:	XMSentry
; Modf:	AX, ES
; Call:	INT 21/48, INT 21/49, INT 21/58,
;	SaveStrategy, RestoreStrategy, getXMSaddr
;
AllocUMB	proc
		push	bx
;		testflag [options],OPT_noUMB
		test byte ptr [options], OPT_noUMB	; 80h, TASM optimizes
		jnz	@@allocasis		; jump if UMB prohibited
		mov	ax,ds
		cmp	ah,0A0h
		jae	@@allocasis		; jump if already loaded hi

;----- check if UMB is a DOS type

		call	SaveStrategy
		DOSSetUMBlink UMB_LINK		; add UMB to MCB chain

		mov	bl,HI_BESTFIT		; OPTIMIZE: BL instead BX
		DOSSetAlloc			; try best strategy to
						;  allocate DOS UMBs
;	if_ carry
	jnc @@nhilo
		mov	bl,HILOW_BESTFIT	; OPTIMIZE: BL instead BX
		DOSSetAlloc			; try a worse one then
;	end_
@@nhilo:

		pop	bx
		push	bx
		DOSAlloc			; allocate UMB (size in BX)
		pushf
		xchg	dx,ax			; OPTIMIZE: instead MOV DX,AX
		call	RestoreStrategy		; restore allocation strategy
		popf
;	if_ nc
	jc @@allc
		cmp	dh,0A0h			; exit if allocated mem is
		jae	@@allocret		;  is above 640k (segment
		DOSFreeMem dx			;  0A000h) else free it
;	end_
@@allc:

if 0
;----- try a XMS manager to allocate UMB

		call	getXMSaddr
;	if_ nc
	jc @@xmsc
		pop	dx
		push	dx
		mov	ah,10h			; XMS: Request UMB (size=DX)
		call	[XMSentry]		; ...AX=1 -> BX=seg, DX=size
		dec	ax
;	andif_ zero
	jnz @@xmsc
		pop	ax
		push	ax
		cmp	bx,ax
		mov	dx,bx
		jae	@@allocret
		mov	ah,11h			; XMS: Release UMB (seg=DX)
		call	[XMSentry]
;	end_
@@xmsc:
endif

;----- use current memory segment

@@allocasis:	mov	dx,ds

@@allocret:	pop	bx
		ret
AllocUMB	endp

;========================================================================
; In:	ES			(segment to free)
; Out:	none
; Use:	XMSentry
; Modf:	AH, DX
; Call:	INT 21/49, getXMSaddr
;
FreeMem		proc
		assume	es:nothing
if 0
		call	getXMSaddr
;	if_ nc
	jc @@fmc
		mov	dx,es
		mov	ah,11h			; XMS: Release UMB
		call_far XMSentry
;	end_
@@fmc:
endif
		DOSFreeMem			; free allocated memory
		ret
FreeMem		endp

;========================================================================

end start
