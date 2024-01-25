;   PC/FTP Packet Driver source, conforming to version 1.05 of the spec
;   Updated to version 1.08 Feb. 17, 1989.
;   Copyright 1988-1993 Russell Nelson

;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, version 1.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program; if not, write to the Free Software
;   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	include picomem.inc
	include	defs.asm

code	segment word public
	assume	cs:code, ds:code

	public	is_eisa
is_eisa	db	0			;=0 if ISA, =1 if EISA
	extrn	sys_features: byte	;bitmask of system features.
	extrn	is_186: byte		;=0 if 808[68], =1 if 80[1234]86.
	extrn	is_286: byte		;=0 if 80[1]8[68], =1 if 80[234]86.
	extrn	is_386: byte		;=0 if 80[12]8[68], =1 if 80[34]86.
	extrn	int_no: byte		;the board's interrupt level.
	extrn	hw_int_no: byte		;the 8259 interrupt level.
	extrn	driver_class: byte	;the class of this driver, per the spec.
	extrn	rcv_modes: word		;count of modes followed by mode handles.
if PicoMEM  eq 1
	extrn 	io_addr:word
endif	

;-> last byte of static memory used by driver-dependent code.
	extrn	end_resident: byte
	extrn	end_free_mem: byte

;-> the fixed address of the card.
	extrn	rom_address: byte

;-> the current address of the card.
	extrn	my_address: byte

	extrn	phd_dioa: byte
	extrn	phd_environ: word
	extrn	flagbyte: byte

	include	printnum.asm
	include	decout.asm
	include	digout.asm
	include	crlf.asm
	include	chrout.asm

free_mem	dw	end_resident	;allocate memory from here.
;also see memory_to_keep.

	public	malloc
malloc:
;enter with dx = amount of memory desired.
;exit with nc, dx -> that memory, or cy if there isn't enough memory.
	add	dx,free_mem		;make a pointer after that much memory.
	cmp	dx,offset end_free_mem	;is it still in the free area?
	ja	malloc_1		;no, we're in trouble.
	xchg	dx,free_mem		;get the pointer back, store ptr->end.
	clc
	ret
malloc_1:
	stc
	ret


end_tail_1	label	byte		; end of the delayed init driver

;usage_msg is of the form "usage: driver [options] <packet_int_no> <args>"
	extrn	usage_msg: byte

options_i_msg	label	byte
db"   -i -- Force driver to report itself as IEEE 802.3 instead of Ethernet II.",CR,LF
options_msg	label	byte
db"   -d -- Delayed initialization.  Used for diskless booting",CR,LF
db"   -n -- NetWare conversion.  Converts 802.3 packets into 8137 packets",CR,LF
db"   -w -- Windows hack, obsoleted by winpkt",CR,LF
db"   -p -- Promiscuous mode disable",CR,LF
db"   -u -- Uninstall",CR,LF
db '$'

;copyright_msg is of the form:
;"Packet driver for the foobar",CR,LF
;"Portions Copyright 19xx, J. Random Hacker".
	extrn	copyright_msg: byte

copyleft_msg	label	byte
 db "Packet driver skeleton copyright 1988-93, Crynwr Software.",CR,LF
 db "This program is freely copyable; source must be available; NO WARRANTY.",CR,LF
 db "See the file COPYING.DOC for details; send FAX to +1-315-268-9201 for a copy.",CR,LF
 db CR,LF,'$'

no_resident_msg	label	byte
 db CR,LF,"*** Packet driver failed to initialize the board ***",CR,LF,'$'

;parse_args should parse the arguments.
;called with ds:si -> immediately after the entry_point.
	extrn	parse_args: near

;print_parameters should print the arguments.
	extrn	print_parameters: near

	extrn	our_isr: near, their_isr: dword
	extrn	entry_point: byte

eisa_signature	db	"EISA"

system_msg	db	"System: ",'$'
i386_msg	db	"[345]86 processor",'$'
i286_msg	db	"286 processor",'$'
i186_msg	db	"186 processor",'$'
i8088_msg	db	"8088/8086 processor",'$'
mca_msg		db	", Microchannel bus",'$'
eisa_msg	db	", EISA bus",'$'
isa_msg		db	", ISA bus",'$'
two_8259_msg	db	", Two 8259s",'$'
entry_point_name	db	"Packet driver software interrupt is ",'$'
eaddr_msg	db	"My Ethernet address is ",'$'
aaddr_msg	db	"My ARCnet address is ",'$'

already_msg	db	CR,LF,"Error: there is already a packet driver (you may uninstall it using -u) at ",'$'
no_pkint_msg	db	CR,LF,"Error: there is no packet driver at ",'$'
no_pkt_msg	db	CR,LF,"Error: no packet driver found between 0x60 and 0x80",CR,LF,'$'
two_pkts_msg	db	CR,LF,"Error: there are two packets drivers (specify the desired one after -u).",CR,LF,'$'
int_msg		db	CR,LF
		db	"Error: <hardware_irq> should be between 0 and "
int_msg_num	label	word
		db	"15 inclusive", '$'
xt_hd_warn_msg	db	CR,LF,"Warning: the hard disk on an XT usually uses IRQ 5.  Use a different interrupt",CR,LF,'$'
no_ieee_msg	db	CR,LF,"Error: this driver doesn't implement both IEEE 802.3 and Ethernet II",CR,LF,'$'
terminated_msg	db	"Uninstall completed",'$'

if PicoMEM  eq 1
detected_msg     db 'PicoMEM Detected',CR,LF,'$'
notdetected_msg  db	'Error : PicoMEM Not Detected','$'
endif

handle		dw	?

entry_point_fnd	db	0
bogus_type	db	0,0		;totally bogus type code.
our_recv:
	xor	di,di
	mov	es,di
	retf

	public	etopen_diagn
etopen_diagn	db	0		; errorlevel from etopen if set

;etopen should initialize the device.  If it needs to give an error, it
;can issue the error message and quit to dos.
	extrn	etopen: near

memory_to_keep	dw	end_resident	;keep at least this much memory.
;also see free_mem.

already_error:
	mov	dx,offset already_msg
already_error_1:
	mov	di,offset entry_point
	call	print_number
	mov	ax,4c05h		; give errorlevel 5
	int	21h

usage_error:
	mov	dx,offset usage_msg
	mov	ah,9
	int	21h
	mov	dx,offset options_msg
	cmp	word ptr driver_class,BLUEBOOK + IEEE8023*256	;both present?
	jne	error
	mov	dx,offset options_i_msg
	public	error
error:
	mov	ah,9
	int	21h
	mov	ax,4c0ah		; give errorlevel 10
	int	21h

;;;	include	timeout.asm

	public	start_1
start_1:
	cld

	mov	dx,offset copyright_msg
	mov	ah,9
	int	21h

	mov	dx,offset copyleft_msg
	mov	ah,9
	int	21h

	mov	dx,offset branding_msg
	mov	ah,9
	int	21h

	mov	dx,0f000h		;ROM segment
	mov	es,dx
	mov	di,0ffd9h
	mov	si,offset eisa_signature
	mov	cx,2
	repe	cmpsw
	jne	not_eisa
	inc	is_eisa
not_eisa:

;
; Get the feature byte (if reliable) so we can know if it is a microchannel
; computer and how many interrupts there are.
;
	mov	ah,0c0h
	int	15h			; es:bx <- sys features block
	jc	look_in_ROM		; error, must use rom.
	or	ah,ah
	jnz	look_in_ROM
	mov	dx,es:[bx]		; # of feature bytes
	cmp	dx,4			; do we have the feature byte we want?
	jae	got_features		;yes.
look_in_ROM:
	cmp	byte ptr es:[0fffeh],0fch;is this an AT?
	jne	identified		;no.
	or	sys_features,TWO_8259	; ATs have 2nd 8259
	jmp	identified		; assume no microchannel
got_features:
	mov	ah,es:[bx+2]		; model byte
	cmp	ah,0fch
	je	at_ps2
	ja	identified		; FD, FE and FF are not ATs
	cmp	ah,0f8h
	je	at_ps2
	ja	identified		; F9, FA and FB are not ATs
	cmp	ah,09ah
	jbe	identified		; old non-AT Compacs go here
at_ps2:					; 9B - F8 and FC are assumed to
	mov	ah,es:[bx+5]		;   have reliable feature byte
	mov	sys_features,ah
identified:

;Determine the processor type.  The 8088 and 8086 will actually shift ax
;over by 33 bits, while the 80[123]86 use a shift count mod 32.
	mov	cl,33
	mov	ax,0ffffh
	shl	ax,cl			;186 or better?
	jz	processor_identified	;no.
	mov	is_186,1

	push	sp
	pop	ax
	cmp	ax,sp			;286 or better?
	jne	processor_identified	;no.
	mov	is_286,1

	pushf
	pop	ax
	or	ax,7000h		;the 386 lets us set these bits
	push	ax
	popf				;this should be a real popf.
	pushf
	pop	ax
	test	ax,7000h		;did the bits get set?
	je	processor_identified
	mov	is_386,1

processor_identified:

	mov	si,offset phd_dioa+1
	call	skip_blanks		;end of line?
	cmp	al,CR
	je	usage_error_j_1

chk_options:
	call	skip_blanks
	cmp	al,'-'			; any options?
	jne	no_more_opt
	inc	si			; skip past option char
	lodsb				; read next char
	or	al,20h			; convert to lower case
	cmp	al,'d'
	jne	not_d_opt
	or	flagbyte,D_OPTION
	jmp	chk_options
not_d_opt:
	cmp	al,'n'
	jne	not_n_opt
	or	flagbyte,N_OPTION
	jmp	chk_options
not_n_opt:
	cmp	al,'w'
	jne	not_w_opt
	or	flagbyte,W_OPTION
	jmp	chk_options
not_w_opt:
	cmp	al,'p'
	jne	not_p_opt
	cmp	rcv_modes,6		;do they even *have* a promiscuous mode?
	jbe	chk_options		;no.
	mov	rcv_modes+2[6*2],0	;yes, nuke it.
	jmp	chk_options
not_p_opt:
	cmp	al,'u'
	jne	not_u_opt
	or	flagbyte,U_OPTION
	jmp	chk_options
not_u_opt:
	cmp	al,'i'
	jne	not_i_opt
	cmp	word ptr driver_class,BLUEBOOK + IEEE8023*256	;both present?
	mov	dx,offset no_ieee_msg
	jne	usage_error_j_1		;no - give error
	mov	word ptr driver_class,IEEE8023 + BLUEBOOK*256	;yes, swap them.
	jmp	chk_options
not_i_opt:
usage_error_j_1:
	jmp	usage_error
no_more_opt:

	mov	di,offset entry_point	;parse the packet interrupt number
	call	get_number		;  for them.

	test	flagbyte,U_OPTION	;should we terminate the driver?
	jne	terminate
	jmp	not_terminate
terminate:

	cmp	entry_point,0		;did they ask for the default?
	jne	terminate_int_ok	;no, run with it.

	mov	entry_point,60h
terminate_check_int:
	call	verify_packet_int
	jc	terminate_check_int_1
	jne	terminate_check_int_1
	cmp	entry_point_fnd,0	;did we already find one?
	jne	terminate_check_int_2	;yes, it's ambiguous - give error.
	mov	al,entry_point		;no, remember it.
	mov	entry_point_fnd,al
terminate_check_int_1:
	inc	entry_point		;go look at the next one
	cmp	entry_point,80h
	jbe	terminate_check_int	;keep going to the end.

	mov	al,entry_point_fnd	;restore the last one found.
	mov	entry_point,al
	cmp	entry_point_fnd,0	;did we find one?
	jne	terminate_int_ok	;yes.
	mov	dx,offset no_pkt_msg	;no packet drivers installed!
	jmp	error
terminate_check_int_2:
	mov	dx,offset two_pkts_msg	;two packet drivers - which one??
	jmp	error

terminate_int_ok:
	call	verify_packet_int	;is the one they specified acceptable?
	jnc	terminate_1		;no, it's not in range.
	jmp	error
terminate_1:
	je	terminate_2		;go if we found a signature.
	mov	dx,offset no_pkint_msg	;no packet driver there.
	jmp	already_error_1
terminate_2:

	mov	their_isr.offs,bx
	mov	their_isr.segm,es

	mov	ax,1ffh			;driver_info
	pushf
	cli
	call	their_isr
	call	fatal_error
	movseg	ds,cs

	mov	ah,2			;access_type
	mov	al,ch			;their class from driver_info().
	mov	bx,dx			;their type from driver_info().
	mov	dl,cl			;their number from driver_info().
	mov	cx,2			;use type length 2.
	mov	si,offset bogus_type
	movseg	es,cs
	mov	di,offset our_recv
	pushf
	cli
	call	their_isr
	call	fatal_error
	mov	handle,ax

	mov	ah,5			;terminate the driver.
	mov	bx,handle
	pushf
	cli
	call	their_isr
	jnc	now_close
	call	print_error
	mov	ah,3			;release_type
	mov	bx,handle
	pushf
	cli
	call	their_isr
	int	20h
now_close:
	mov	dx,offset terminated_msg
	mov	ah,9
	int	21h
	int	20h

usage_error_j_2:
	jmp	usage_error
not_terminate:

if PicoMEM  eq 1
; All the detection and Command line parsing is done : Detect the PicoMEM and update IRQ/Port

		mov ax,6000h	; PicoMEM Function 0 (Detect)
		mov dx,1234h	; To be added to be sure
		int 13h
		cmp dx,0AA55h
		jne @@NoPicomem

		mov dx,offset detected_msg
		mov	ah,9
		int	21h

		mov ax,6002h	; PicoMEM Function 2 (ne2000 Infos)
		mov dx,1234h	; To be added to be sure
		int 13h

		mov	int_no,ah
		mov	io_addr,bx

		call print_parameters

		jmp @@PMDetectEnd
@@NoPicomem:

		mov dx,offset notdetected_msg
		mov	ah,9
		int	21h

		mov	ah,4ch
		mov al,32
		int	21h			; Return to DOS
		
@@PMDetectEnd:

;		mov	int_no,3
;		mov	io_addr,300h

endif

if PicoMEM  eq 0
	call	parse_args
	jc	usage_error_j_2

	call	skip_blanks		;end of line?
	cmp	al,CR
	jne	usage_error_j_2
endif

	call	verify_packet_int
	jnc	packet_int_ok
	jmp	error
packet_int_ok:
	jne	packet_int_unused
	jmp	already_error		;give an error if there's one there.
packet_int_unused:
;
; Verify that the interrupt number they gave is valid.
;
	cmp	int_no,15		;can't possibly be > 15.
	ja	int_bad
	test	sys_features,TWO_8259	; 2nd 8259 ?
	jnz	int_ok			;yes, no need to check for <= 7.
	mov	int_msg_num,'7'+' '*256	;correct the error message, just in case.
	cmp	int_no,7		;make sure that the packet interrupt
	jbe	int_ok_7		;  number is in range.
int_bad:
	mov	dx,offset int_msg
	jmp	error
int_ok_7:
	cmp	int_no,5		;Are they trying to use irq 5 on an XT?
	jne	int_ok			;no.

	push	ds
	mov	ax,40h
	mov	ds,ax
	mov	al,ds:[75h]		;get the number of hard disks.
	pop	ds

	or	al,al			;do they have one?
	je	int_ok			;unbelievably, no.
	mov	dx,offset xt_hd_warn_msg
	mov	ah,9
	int	21h
int_ok:

;
; Map IRQ 2 to IRQ 9 if needed.
;
	test	sys_features,TWO_8259	; 2nd 8259 ?
	je	no_mapping_needed	;no, no mapping needed
	cmp	int_no,2		;map IRQ 2 to IRQ 9.
	jne	no_mapping_needed
	mov	int_no,9
no_mapping_needed:

; If they chose the -d option, don't call etopen when we are loaded,
; but when we are called for the first time
;
; Save part of the tail, needed by delayed etopen
	test	flagbyte,D_OPTION
	jz	open_now
	mov	memory_to_keep,offset end_tail_1	; save first part of tail
	jmp	delayed_open_1
open_now:
	call	etopen			;init the driver.  If any errors,
					;this routine returns cy.
	jnc	yes_resident
	jmp	no_resident

yes_resident:
	mov	si,offset rom_address	;copy their original address to
	movseg	es,ds
	mov	di,offset my_address	;  their current address.
	mov	cx,MAX_ADDR_LEN/2
	rep	movsw

; tell them what kind of system they have.
	mov	dx,offset system_msg
	mov	ah,9
	int	21h

	mov	dx,offset i386_msg
	cmp	is_386,0
	jne	have_processor
	mov	dx,offset i286_msg
	cmp	is_286,0
	jne	have_processor
	mov	dx,offset i186_msg
	cmp	is_186,0
	jne	have_processor
	mov	dx,offset i8088_msg
have_processor:
	mov	ah,9
	int	21h

	mov	dx,offset mca_msg
	test	sys_features,SYS_MCA
	jne	have_bus
	mov	dx,offset eisa_msg
	cmp	is_eisa,0
	jne	have_bus
	mov	dx,offset isa_msg
have_bus:
	mov	ah,9
	int	21h

	test	sys_features,TWO_8259
	je	only_one_8259
	mov	dx,offset two_8259_msg
	mov	ah,9
	int	21h
only_one_8259:

	call	crlf

	mov	di,offset entry_point
	mov	dx,offset entry_point_name
	call	print_number

	call	print_parameters	;echo our parameters.
	or	flagbyte,CALLED_ETOPEN

	cmp	driver_class,BLUEBOOK	;Blue Book Ethernet?
	je	print_eaddr		;yes.
	cmp	driver_class,IEEE8023	;IEEE 802.3 Ethernet?
	jne	print_addr_2		;no, don't print what we don't have.

print_eaddr:
	mov	dx,offset eaddr_msg
	mov	ah,9
	int	21h

	mov	si,offset rom_address
	call	print_ether_addr

	call	crlf

print_addr_2:

	cmp	driver_class,8		;ARCnet?
	jne	print_addr_3		;no, don't print what we don't have.

	mov	dx,offset aaddr_msg
	mov	ah,9
	int	21h

	mov	al,rom_address
	mov	cl,' '			;Don't eliminate leading zeroes.
	call	byteout

	call	crlf

print_addr_3:
delayed_open_1:
	mov	ah,35h			;remember their packet interrupt.
	mov	al,entry_point
	int	21h
	mov	their_isr.offs,bx
	mov	their_isr.segm,es

	mov	ah,25h			;install our packet interrupt
	mov	dx,offset our_isr
	int	21h

	mov	ah,49h			;free our environment, because
	mov	es,phd_environ		;  we won't need it.
	int	21h

	mov	bx,1			;get the stdout handle.
	mov	ah,3eh			;close it in case they redirected it.
	int	21h

	test	flagbyte,D_OPTION
	jne	f_release_type_1	;no.
	cmp	rcv_modes+2[3*2],0	;does mode 3 exist?
	je	f_release_type_1	;no.
	call	rcv_modes+2[3*2]	;  call it.
f_release_type_1:

	mov	dx,memory_to_keep	;keep the greater of this and
	cmp	dx,free_mem		;  free_mem.
	jae	go_resident
	mov	dx,free_mem
go_resident:
	add	dx,0fh			;round up to next highest paragraph.
	mov	cl,4
	shr	dx,cl
	mov	ah,31h			;terminate, stay resident.
	mov	al,etopen_diagn		; errorlevel (0 - 9, just diagnostics)
	int	21h

no_resident:
	mov	ah,9			;print their error message.
	int	21h

	mov	dx,offset no_resident_msg
	mov	ah,9
	int	21h

	mov	ax,4c00h + 32		; give errorlevel 32
	cmp	al,etopen_diagn
	ja	no_et_diagn		; etopen gave specific reason?
	mov	al,etopen_diagn		; yes, use that for error level
no_et_diagn:
	int	21h

; 			Suggested errorlevels:
;
; _____________________  0 = normal
; 			 1 = unsuitable memory address given; corrected
; In most cases every-	 2 = unsuitable IRQ level given; corrected
; thing should work as	 3 = unsuitable DMA channel given; corrected
; expected for lev 1-5	 4 = unsuitable IO addr given; corrected (only 1 card)
; _____________________	 5 = packet driver for this int # already loaded
; External errors, when	20 = general cable failure (but pkt driver is loaded)
; corrected normal	21 = network cable is open             -"-
; operation starts	22 = network cable is shorted          -"-
; _____________________ 23 = 
; Packet driver not	30 = usage message
; loaded. A new load	31 = arguments out of range
; attempt must be done	32 = unspecified device initialization error
;			33 = 
;			34 = suggested memory already occupied
;			35 = suggested IRQ already occupied
;			36 = suggested DMA channel already occupied
;			37 = could not find the network card at this IO address


	include	verifypi.asm
	include	getnum.asm
	include	getdig.asm
	include	skipblk.asm
	include	printea.asm
	include	pkterr.asm
	include	getenv.asm

	public	branding_msg
branding_msg	db	'$'

code	ends

	end
