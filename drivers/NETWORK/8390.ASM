;History:538,1
; Ian Brabham	28 Apr 1993	Fix problems related to SMC version of 8390

dp8390_version	equ	3	;version number of the generic 8390 driver.

;  Copyright, 1988-1992, Russell Nelson, Crynwr Software

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

; This driver is the work of several people: Bob Clements, Eric Henderson,
; Dave Horne, Glenn Talbott, Russell Nelson, Jan Engvald, Paul Kranenburg,
; and Ian Brabham.

  ife SM_RSTART_PG
	%err	SM_RSTART_PG cannot be zero because of a decrement/unsigned jump.
  endif

;
; The longpause macro was originally written as this:
;
;longpause macro
;	push	cx
;	mov	cx,0
;	loop	$
;	pop	cx
;endm
;
; It was only used to stall while hard resetting the card. On my
; 25Mhz 486 longpause was taking more than 18ms, and almost forever
; on slower machines, much longer than necessary and not predictable.
;
; To be able to utilize longpause elsewhere and make it machine independent and
; predictable, I have re-written it to be a fixed time of 1.6ms, which just
; happens to be the time National recommends waiting for the NIC chip to
; stop sending or receiving after being commanded to stop.
;
; Based on the assumption that ISA specs mandate a 1.0 uS minimum I/O cycle
; Microchannel a 0.5uS minimum I/O cycle, and the NMI Status register (location
; 61h) is readable via I/O cycle on all machines, the longpause macro is now
; defined below. - gft - 901604
; (I realize that on slow machines this may take much longer, but the point
; is that on FAST machines it should never be faster than 1.6ms)

longpause macro
	local lp_not_mc
	push cx
	push ax
	mov  cx,1600	; 1.6ms = 1600*1.0us
	test sys_features,SYS_MCA
	je   lp_not_mc
	shl  cx,1	; twice as many loops for Microchannel
lp_not_mc:
	in al,61h
	loop lp_not_mc
	pop  ax
	pop  cx
endm

	extrn	sys_features: byte

sm_rstop_ptr	db	SM_RSTOP_PG

rxcr_bits       db      ENRXCR_BCST     ; Default to ours plus multicast
  ifdef board_features
is_overrun_690	db	0
  endif


;-> the assigned Ethernet address of the card.
	extrn	rom_address: byte

;-> current address
	extrn	my_address: byte

	public	mcast_list_bits, mcast_all_flag
mcast_list_bits db      0,0,0,0,0,0,0,0 ;Bit mask from last set_multicast_list
mcast_all_flag  db      0               ;Non-zero if hware should have all
					; ones in mask rather than this list.

	public	rcv_modes
rcv_modes	dw	7		;number of receive modes in our table.
		dw	0               ;There is no mode zero
		dw	rcv_mode_1
		dw	rcv_mode_2
		dw	rcv_mode_3
		dw	rcv_mode_4
		dw	rcv_mode_5
		dw	rcv_mode_6

;
;	a temp buffer for the received header
;
RCV_HDR_SIZE	equ	26		; 2 ids @6 + protocol @2+8, + header @4
rcv_hdr		db	RCV_HDR_SIZE dup(0)

;
;	The board data
;
		public	board_data
BOARD_DATA_SIZE equ	32
board_data	db 	BOARD_DATA_SIZE dup(0)


; add public for soft errors (how were these extracted before? - gft - 910604

		public soft_errors
		public soft_tx_errors,soft_tx_err_bits,soft_tx_collisions
		public soft_rx_errors,soft_rx_err_bits
		public soft_rx_overruns,soft_rx_over_nd
;
; Re-arranged the order of these soft_xx_err things so that they can be
; accessed as a data structure (like the statistics structure defined
; in the packet driver spec. I don't know if it's necessary but I've always
; found data structures to be more portable if elements are size aligned.
;  - gft - 910607
; Don't rearrange or insert things between these soft error words because
; they are accessed as a data structure (I don't think I violated my own
; admonition since they wern't public and I could find no references to 
; them and only the NEx000 drivers reference the next previous thing,
; board_data, and they only us 16 bytes of that.)

soft_errors 		label   dword
soft_tx_errors		dw	0,0
soft_tx_collisions	dw	0,0 ; added - gft - 910607 cause who ever heard
				    ; of a CSMA/CD driver not counting
				    ; collisions.
soft_rx_errors		dw	0,0
soft_rx_overruns	dw	0,0 ; added - gft - 910604 cause I just
				    ; gotta track these so I can findout
				    ; just when I'm pushing a driver or
				    ; application to it's limits
soft_rx_over_nd		dw	0,0 ; Also count when theres no data.
hard_tx_errors		dw	0,0
soft_tx_err_bits	db	0
soft_rx_err_bits	db	0

;
; Next Packet Pointer (added  - gft - 910603)
;
;   Initialize to the same value as the current page pointer (start page 1).
;   Update after each reception to be the value of the next packet pointer
;   read from the NIC Header.
;   Copy value -1 to boundry register after each update.
;   Compare value with contents of current page pointer to verify that a
;   packet has been received (don't trust ISR RXE/PRX bits). If !=, one
;   or more packets have been received.

next_packet	db	0
save_curr	db	0

; Added flags and temp storage for new receive overrun processing
;  - gft - 910604

rcv_ovr_resend	db	0,0	; flag to indicate resend needed
defer_upcall	db	0,0	; flag to indicate deferred upcall needed
defer_ds	dw	?	;   deferred upcall parameters
defer_si	dw	?
defer_cx	dw	?


ifdef	deb2screen
; Event to screen debugger. Destroys no registers and requires just 3 bytes at
; each place called. Produces a one-line summary of event types that has ever
; occured and then some trace lines with the sequence of the last events./Jan E LDC

SHOWMIN		equ	'a'
SHOWMAX		equ	'l'
EVENTCOLOR	equ	31h
EVENTLINE	equ	17
TRACECOLOR	equ	2eh

ShowEvent	proc	near
x		=	0
		rept	(SHOWMAX-SHOWMIN+1)
		push	ax
		mov	al,x
		jmp	short ShowEventNum
x		=	x+1
		endm

  ShowEventNum:
		pushf
		push	di
		push	es
		mov	ah,EVENTCOLOR
		mov	di,ax
		shl	di,1
		add	al,SHOWMIN
		mov	es,cs:EventPar
		cld
		stosw

		mov	ah,TRACECOLOR
		mov	es,cs:TracePar
		cli
		mov	di,cs:TraceInd
		stosw
		and	di,01ffh		; (1ff+1)/2 = 256 log entries
		mov	cs:TraceInd,di
		mov	al,01bh
		not	ah
		stosw

		pop	es
		pop	di
		popf
		pop	ax
		ret
ShowEvent	endp

EventPar	dw	0b800h+(EVENTLINE-1)*10-2*EVENTCOLOR*16
TracePar	dw	0b800h+EVENTLINE*10
TraceInd	dw	0

SHOW_EVENT	macro	id
if id gt SHOWMAX or id lt SHOWMIN
		.err
endif
		call	ShowEvent+((id-SHOWMIN)*(ShowEventNum-ShowEvent)/(SHOWMAX-SHOWMIN+1))
		endm

else

SHOW_EVENT	macro	num
		endm

endif ; deb2screen


ifdef	debug			; Include a very useful logging mechanism.  

; The log entry structure.  Log entries include useful data such as
; a type (each place a log entry is made uses a different type), various
; chip status, ring buffer status, log entry dependent data, and optionally
; 8259 interrupt controller status.
logentry	struc
le_type		db	0	; Log entry type
le_ccmd		db	?	; Value of CCMD register
le_isr		db	?	; Value of ISR register
le_tsr		db	?	; Value of TSR register
le_tcur		dw	?	; Value of sm_tcur
le_tboundary	dw	?	; Value of sm_tboundary
le_tnum		dw	?	; Value of sm_tnum
le_dw		dw	?	; Log type specific dw data
ifndef	mkle8259		; Log 8259 status?
le_dd		dd	?	; Log type specific dd data
else
le_irr1		db	?	; Value of 8259-1 IRR register
le_isr1		db	?	; Value of 8259-1 ISR register
le_irr2		db	?	; Value of 8259-2 IRR register
le_isr2		db	?	; Value of 8259-2 ISR register
endif
logentry	ends

; The types of log entries.
LE_SP_E		equ	0	; send_pkt entry
LE_SP_X		equ	1	; send_pkt exit
LE_ASP_E	equ	2	; as_send_pkt entry
LE_ASP_X	equ	3	; as_send_pkt exit
LE_RBALLOC_E	equ	4	; tx_rballoc entry
LE_RBALLOC_X	equ	5	; tx_rballoc exit
LE_COPY_E	equ	6	; sm_copy entry
LE_COPY_X	equ	7	; sm_copy exit
LE_START_E	equ	8	; tx_start entry
LE_START_X	equ	9	; tx_start exit
LE_XMIT_E	equ	0ah	; xmit entry
LE_XMIT_X	equ	0bh	; xmit exit
LE_TXISR_E	equ	0ch	; txisr entry
LE_TXISR_X	equ	0dh	; txisr exit
LE_RECV_E	equ	0eh	; recv entry
LE_RECV_X	equ	0fh	; recv exit
LE_RCVFRM_E	equ	10h	; rcv_frm entry
LE_RCVFRM_X	equ	11h	; rcv_frm exit
LE_COPY_L	equ	12h	; sm_copy loop
LE_TIMER_E	equ	13h	; timer entry
LE_TIMER_X	equ	14h	; timer exit

	public	log, log_index
log		logentry 256 dup (<>) ; The log itself
log_index	db	0	; Index to current log entry

; The macro used to create log entries.
mkle	macro	letype, ledw, ledd, ledd2 ; Make an entry in the log
	pushf			; Save interrupt enable state
	cli			; Disable interrupts
	push	dx		; Save registers
	push	bx
	push	ax
	mov bl,	log_index	; Get current log_index
	xor bh,	bh		; Clear high byte
	shl bx,	1		; Multiply by sixteen
	shl bx,	1
	shl bx,	1
	shl bx,	1
	mov log[bx].le_type, letype ; Store log entry type
	loadport		; Base of device
	setport EN_CCMD	; Point at chip command register
	in al,	dx		; Get chip command state
	mov log[bx].le_ccmd, al	; Store CCMD value
	setport EN0_ISR		; Point at chip command register
	in al,	dx		; Get chip command state
	mov log[bx].le_isr, al	; Store ISR value
	setport EN0_TSR		; Point at chip command register
	in al,	dx		; Get chip command state
	mov log[bx].le_tsr, al	; Store TSR value
	mov ax,	sm_tcur		; Get current sm_tcur
	mov log[bx].le_tcur, ax	; Store sm_tcur value
	mov ax,	sm_tboundary	; Get current sm_tboundary
	mov log[bx].le_tboundary, ax ; Store sm_tboundary value
	mov ax,	sm_tnum		; Get current sm_tnum
	mov log[bx].le_tnum, ax	; Store sm_tnum value
	mov log[bx].le_dw, ledw	; Store log entry dw
ifndef	mkle8259		; Include extra per-type data
	mov word ptr log[bx].le_dd, ledd ; Store low word of log entry dd
	mov word ptr log[bx].le_dd+2, ledd2 ; Store high word of log entry dd
else				; Include 8259 status
	mov	al,0ah		; read request register from
	out	0a0h,al		; secondary 8259
	pause_
	in	al,0a0h		; get it
	mov log[bx].le_irr2, al
	mov	al,0bh		; read in-service register from
	out	0a0h,al		; secondary 8259
	pause_
	in	al,0a0h		; get it
	mov log[bx].le_isr2, al
	mov	al,0ah		; read request register from
	out	020h,al		; primary 8259
	pause_
	in	al,020h		; get it
	mov log[bx].le_irr1, al
	mov	al,0bh		; read in-service register from
	out	020h,al		; primary 8259
	pause_
	in	al,020h		; get it
	mov log[bx].le_isr1, al
endif
ifdef	screenlog		; Log the entry type to the screen too
	push	es
	mov ax,	0b800h		; Color screen only...
	mov es,	ax
	mov bl,	log_index	; Get current log_index
	xor bh,	bh		; Clear high byte
	shl bx,	1		; Multiply by sixteen
	add bx,	3360
	mov byte ptr es:[bx-1], 07h
	mov byte ptr es:[bx], letype+30h
	mov byte ptr es:[bx+1], 70h
	pop	es
endif
	inc	log_index	;
	pop	ax		; Restore registers
	pop	bx
	pop	dx
	popf			; Restore interrupt enable state
	endm

else
mkle	macro	letype, ledw, ledd, ledd2 ; Define an empty macro
	endm
endif

	public bad_command_intercept
bad_command_intercept:
;called with ah=command, unknown to the skeleton.
;exit with nc if okay, cy, dh=error if not.
	mov	dh,BAD_COMMAND
	stc
	ret

	public	as_send_pkt
; The Asynchronous Transmit Packet routine.
; Enter with es:di -> i/o control block, ds:si -> packet, cx = packet length,
;   interrupts possibly enabled.
; Exit with nc if ok, or else cy if error, dh set to error number.
;   es:di and interrupt enable flag preserved on exit.
as_send_pkt:
	ret

	public	drop_pkt
; Drop a packet from the queue.
; Enter with es:di -> iocb.
drop_pkt:
	assume	ds:nothing
	ret

	public	xmit
; Process a transmit interrupt with the least possible latency to achieve
;   back-to-back packet transmissions.
; May only use ax and dx.
xmit:
	assume	ds:nothing
	ret


; The tx_wait loop had three problems that affected high load throughput.
; Most seriously, while we are waiting for the previous SEND to finnish,
; the chip can actually be RECEIVING one or even many packets! But because
; we were waiting with interrupts disabled, these packets were not emptied
; from the receive ring and we could get an overrun. We could put in code to
; test for pending receive interrupts, but that would not help for the
; third problem, see below. Instead interrupts are now on while waiting.
; Secondly, the wait loop was not long enough to allow for up to 16 collisions.
; Thirdly, for a router there are two or more drivers and the busy waiting
; in one of them prevented interrupt handling for the other(s), giving
; unnecessary low throughput. /Jan E LDC

tx_wait:
	mov	bx,1024*7	; max coll time in Ethernet slot units
tx_wait_l1:
	mov	ah,51		; assume 1 us IO
	test	sys_features,SYS_MCA
	jz	tx_wait_l2
	shl	ah,1		; MCA IO is just 0.5 us
tx_wait_l2:
	sti			; allow receive interrupts while waiting
	loadport		; Point at chip command register
	setport EN_CCMD		; ..
	in al,	dx		; Get chip command state
	test al,ENC_TRANS	; Is transmitter still running?
	cli			; the rest of the code may not work with EI (?)
	jz	tx_idle_0	; Go if free

	dec	ah
	jnz	tx_wait_l2	; wait 51.2 us (one ethernet slot time)

	dec	bx		; waited more than max collision time?
	jnz	tx_wait_l1	; -no, probably not stuck yet

	SHOW_EVENT	'b'
	add2	hard_tx_errors,1	;count hard errors.
	call	count_out_err	; Should count these error timeouts
				; Maybe need to add recovery logic here
	jmp	short tx_idle_0


	public	send_pkt
send_pkt:
;enter with ds:si -> packet, cx = packet length.
;exit with nc if ok, or else cy if error, dh set to error number.
	assume	ds:nothing
	mkle LE_SP_E, cx, si, ds

	cli

;ne1000 checks the packet size at this point, which is probably more sensible.
	loadport		; Point at chip command register
	setport EN_CCMD		; ..
	pause_
;ne1000 fails to check to see if the transmitter is still busy.
	in al,	dx		; Get chip command state
	test al,ENC_TRANS	; Is transmitter still running?
ifdef debug
	mov log_ccmd,al		; added - gft - 910607
endif
;
; Used to just go to tx_idle here if the transmitter was not running, however
; it is possible to get here with the transmission complete, but since
; interrupts are off when we get here it is also possible that a transmission
; JUST DID complete and has not yet had its interrupt acknowledge and errors
; recorded. Proceding here without checking will work, it just looses the 
; error status from the last transmission if that transmission has not been
; acknowledged by the isr_tx code.
;
; Changed the jz tx_idle below to the following code - gft - 910607
;
; 	jz	tx_idle		; Go if free

	jnz	tx_wait

; Check for recent TX completions in the interrupt status register

	setport EN0_ISR		; Point at isr
	pause_
	in	al,dx		; get state
ifdef debug
	mov	log_isr,al
endif
	test al,ENISR_TX+ENISR_TX_ERR ; pending TX interupts?
	jz	tx_idle		; No, Go on with new TX

tx_idle_0:	; Added to capture TSR if tx not done on entry
		; AND added all the below TX error detection down to tx_idle
		; - gft - 910603

	loadport
 	setport	EN0_TSR		; point to TSR
	pause_
	in al,	dx		; get state from prior TX
ifdef debug
	mov log_tsr,al		; added - gft - 910603
endif

; Acknowledge the TX interrupt and take care of any TX errors from the 
; previous transmission

	mov	ah,al		; save the TSR state

	setport EN0_ISR		; Point at the interrupt status register
	pause_
	mov	al,ENISR_TX+ENISR_TX_ERR ; clear either possible TX int bit
	out	dx,al

	test ah,ENTSR_COLL+ENTSR_COLL16+ENTSR_FU+ENTSR_OWC+ENTSR_CRS
	jz	tx_idle		; No usefull errors, skip. See the called
				; routine for explanation of the selection
				; of TSR bits for consideration as errors.
	call	count_tx_err

tx_idle:
; If IPX + NETX ver 3.26 receives a CANT_SEND it will put a query on the
; screen if one should abort or retry. This is annoying, causes delay and
; destroys formatted screens. Even worse, the current NETX, ver 3.32, will
; hang for any of the replies.

	cmp	word ptr [si+2*EADDR_LEN+2],0ffffh ; Novell packet?
	je	tx_nov_noerr	; -yes, avoid CANT_SEND, causes hang :-(

	mov	ax,soft_tx_errors	;return an error if the previous
	or	ax,hard_tx_errors	;  packet failed.
	jne	send_pkt_err
tx_nov_noerr:

	cmp	cx,GIANT	; Is this packet too large?
	ja	send_pkt_toobig

	cmp cx,	RUNT		; Is the frame long enough?
	jnb	tx_oklen	; Go if OK
	mov cx,	RUNT		; Stretch frame to minimum allowed
tx_oklen:
	push	cx		; Hold count for later
	loadport		; Set up for address
	setport EN0_ISR
	pause_
	mov	al,ENISR_RDC	; clear remote interrupt int.
	out	dx,al
	setport	EN0_TCNTLO	; Low byte of TX count
	pause_
	mov al,	cl		; Get the count
	out dx,	al		; Tell card the count
	setport	EN0_TCNTHI	; High byte of TX count
	pause_
	mov al,	ch		; Get the count
	out dx,	al		; Tell card the count
	xor ax,	ax		; Set up ax at base of tx buffer
	mov ah,	SM_TSTART_PG	; Where to put tx frame
	pop	cx		; Get back count to give to board
	call	block_output
	jc	tx_no_rdc
	loadport
	setport	EN0_TPSR	; Transmit Page Start Register
	pause_
	mov al,	SM_TSTART_PG
	out dx,	al		; Start the transmitter
	setport	EN_CCMD		; Chip command reg
	pause_
	mov al,	ENC_TRANS+ENC_NODMA+ENC_START
	out dx,	al		; Start the transmitter
	mkle LE_SP_X, cx, 1, 0

	mov	ax,soft_tx_errors	;return an error if the previous
	or	ax,hard_tx_errors	;  packet failed.
	jne	send_pkt_err
	clc			; Successfully started
	ret			; End of transmit-start routine
send_pkt_toobig:
	mov	dh,NO_SPACE
	stc
	ret
send_pkt_err:
	SHOW_EVENT	'e'
	mov	soft_tx_errors,0
	mov	soft_tx_collisions,0
	mov	hard_tx_errors,0
	stc
	mov	dh,CANT_SEND
	ret
tx_no_rdc:
	mov	dh,CANT_SEND
	mkle LE_SP_X, cx, 0, 0
	stc
	ret

count_tx_err:
;
; Function to count hard and soft TX completion errors and collisions.
; Entered with ah containing the transmit status register state.
;  - gft - 910607

	test ah,ENTSR_PTX	; 
	jnz	tx_err_02	; NO hard tx errors go look for soft ones

; Handle hard tx errors, fifo underrun and abort

	add2	hard_tx_errors,1	;count hard errors.

	test ah,ENTSR_COLL16	; 16 collision abort?
	jz	tx_err_01 	; no skip
	SHOW_EVENT	'c'	; this almost always occurs in pairs, is this
				; routine called twice WHILE one collision
				; error lasts??? /Jan E LDC
	add2	soft_tx_collisions,16
tx_err_01:
	call	count_out_err  ; only possible other hard error is FU, just
	                       ; count the hard error and skip the soft error
	jmp 	tx_err_done

; Handle the tx soft errors and collisions
;
;  National DP8390 Datasheet Addendum June 1990 says that Carrier Sense Lost 
;  (CRS) and Non Deferred TX (DFR) are useless in some or all 8390s, that
;  leaves only out of window collision (OWC) and CD Heartbeat (CDH) as 
;  soft errors (I would hesitate to call collision an error of any kind).
;  With who knows how may MAUs out there with SQE either disabled or not
;  functional, I don't count CDs as errors either (If your MAU doesn't SQE
;  you would count CD errors on EVERY transmission.) That only leaves OWC
;  as a soft error.

tx_err_02:
	test ah,ENTSR_OWC+ENTSR_COLL ; No soft errors or collisions?
	jz	tx_err_done	     ; return

	test ah,ENTSR_OWC	; Out of window collision?
	jz	tx_err_03	; No, skip
	SHOW_EVENT	'f'	; this is not uncommon on a real net using
                                ; some WD cards, other WD cards on same net
				; don't report any OWC!?? /Jan E LDC
	add2	soft_tx_errors,1

; Collison Counter
	
tx_err_03:
	test ah,ENTSR_COLL	; Enumerated Collisions?
	jz	tx_err_done		; No, return

	setport EN0_NCR		; point at the collision counter
	pause_
	in	al,dx		; get the count
	and	al,0fh		; clear the unused bits
	xor	ah,ah		; clear the high byte
	add2	soft_tx_collisions,ax

tx_err_done:
	mkle LE_TX_ERR, ax, 0, 0
  	ret


	public	set_address
set_address:
	assume	ds:code
;enter with my_address,si -> Ethernet address, CX = length of address.
;exit with nc if okay, or cy, dh=error if any errors.
	loadport
	setport	EN_CCMD		; Chip command register
	pushf
	cli			; Protect from irq changing page bits
	mov al,	ENC_NODMA+ENC_PAGE1	;+ENC_STOP
	out dx,	al		; Switch to page one for writing eaddr
	pause_
	setport	EN1_PHYS	; Where it goes in 8390
set_8390_1:
	lodsb
	out	dx,al
	pause_
	inc	dx
	loop	set_8390_1
	loadport
	setport	EN_CCMD		; Chip command register
	mov al,	ENC_NODMA+ENC_PAGE0	;+ENC_STOP
	out dx,	al		; Restore to page zero
	pause_
	popf
	clc
	ret

; Routines to set address filtering modes in the DS8390
rcv_mode_1:     ; Turn off receiver
	mov al,	ENRXCR_MON      ; Set to monitor for counts but accept none
	jmp short rcv_mode_set
rcv_mode_2:     ; Receive only packets to this interface
	mov al, 0               ; Set for only our packets
	jmp short rcv_mode_set
rcv_mode_3:     ; Mode 2 plus broadcast packets (This is the default)
	mov al,	ENRXCR_BCST     ; Set four ours plus broadcasts
	jmp short rcv_mode_set
rcv_mode_4:     ; Mode 3 plus selected multicast packets
	mov al,	ENRXCR_BCST+ENRXCR_MULTI ; Ours, bcst, and filtered multicasts
	mov     mcast_all_flag,0	; need to do sw filter.
	jmp short rcv_mode_set
rcv_mode_5:     ; Mode 3 plus ALL multicast packets
	mov al,	ENRXCR_BCST+ENRXCR_MULTI ; Ours, bcst, and filtered multicasts
	mov     mcast_all_flag,1
	jmp short rcv_mode_set
rcv_mode_6:     ; Receive all packets (Promiscuous physical plus all multi)
	mov al,	ENRXCR_BCST+ENRXCR_MULTI+ENRXCR_PROMP
	mov     mcast_all_flag,1
rcv_mode_set:
	push    ax			; Hold mode until masks are right
	call    set_hw_multi		; Set the multicast mask bits in chip
	pop     ax
	loadport
	setport	EN0_RXCR		; Set receiver to selected mode
	pause_
	out dx,	al
	mov     rxcr_bits,al		; Save a copy of what we set it to
	ret


; Set the multicast filter mask bits in case promiscuous rcv wanted
set_hw_multi:
	push    cs
	pop     ds
	assume	ds:code
	loadport
	setport	EN_CCMD		; Chip command register
	pause_
	mov cx,	8		; Eight bytes of multicast filter
	mov si, offset mcast_list_bits  ; Where bits are, if not all ones
	cli			; Protect from irq changing page bits
	mov al,	ENC_NODMA+ENC_PAGE1+ENC_STOP
	out dx,	al		; Switch to page one for writing eaddr
	setport	EN1_MULT	; Where it goes in 8390
	pause_
	mov al, mcast_all_flag  ; Want all ones or just selected bits?
	or al,  al
	je      set_mcast_2     ; Just selected ones
	mov al,	0ffh		; Ones for filter
set_mcast_all:
	out dx,	al		; Write a mask byte
	inc	dl		; Step to next one
	loop	set_mcast_all	; ..
	jmp short set_mcast_x

set_mcast_2:
	lodsb                   ; Get a byte of mask bits
	out dx,	al		; Write a mask byte
	inc	dl		; Step to next I/O register
	loop	set_mcast_2 	; ..
set_mcast_x:
	loadport
	setport	EN_CCMD		; Chip command register
	pause_
	mov al,	ENC_NODMA+ENC_PAGE0+ENC_START
	out dx,	al		; Restore to page zero
	ret


	public	reset_board
reset_board:
	assume ds:nothing
	reset_8390
	setport	EN_CCMD		; Chip command reg
	pause_
	mov al,	ENC_STOP+ENC_NODMA
	out dx,	al		; Stop the DS8390

; Wait 1.6ms for the NIC to stop transmitting or receiving a packet. National
; says monitoring the ISR RST bit is not reliable, so a wait of the maximum
; packet time (1.2ms) plus some padding is required.

	longpause
	ret

	public	terminate
terminate:
	terminate_board
	ret

	public	reset_interface
reset_interface:
	assume ds:code
	call	reset_board
	loadport		; Base of I/O regs
	setport	EN0_ISR		; Interrupt status reg
	pause_
	mov al,	0ffh		; Clear all pending interrupts
	out dx,	al		; ..
	setport	EN0_IMR		; Interrupt mask reg
	pause_
	xor al,	al		; Turn off all enables
	out dx,	al		; ..
	ret

; Linkages to non-device-specific routines
;called when we want to determine what to do with a received packet.
;enter with cx = packet length, es:di -> packet type, dl = packet class.
;It returns with es:di = 0 if don't want this type or if no buffer available.
	extrn	recv_find: near

;called after we have copied the packet into the buffer.
;enter with ds:si ->the packet, cx = length of the packet.
	extrn	recv_copy: near

;call this routine to schedule a subroutine that gets run after the
;recv_isr.  This is done by stuffing routine's address in place
;of the recv_isr iret's address.  This routine should push the flags when it
;is entered, and should jump to recv_exiting_exit to leave.
;enter with ax = address of routine to run.
	extrn	schedule_exiting: near

;recv_exiting jumps here to exit, after pushing the flags.
	extrn	recv_exiting_exit: near

	extrn	count_in_err: near
	extrn	count_out_err: near

	public	recv
recv:
;called from the recv isr.  All registers have been saved, and ds=cs.
;Actually, not just receive, but all interrupts come here.
;Upon exit, the interrupt will be acknowledged.
;ne1000 and ne2000 routines are identical to this point (except that ne2000
;  masks off interrupts).
	assume	ds:code
	mkle LE_RECV_E, 0, 0, 0

check_isr:			; Was there an interrupt from this card?
	loadport		; Point at card's I/O port base
	setport EN0_IMR		; point at interrupt masks
	pause_			; switch off, this way we can
	mov	al,0		; leave the chip running.
	out	dx,al		; no interrupts please.
	setport	EN0_ISR		; Point at interrupt status register
	pause_
	in al,	dx		; Get pending interrupts
	and al,	ENISR_ALL	; Any?
	jnz	isr_test_overrun
	mkle LE_RECV_X, 0, 0, 0
	jmp	interrupt_done	; Go if none

;
; Revised receive overrun code which corresponds to the National DP8390
; Datasheet Addendum, June 1990.
;
; - gft - 910604
;

; Test for receive overrun in value from NIC ISR register

isr_test_overrun:
	test al,ENISR_OVER	; Was there an overrun?
	jnz	recv_overrun	; Go if so.
	jmp	recv_no_overrun	; Go if not.

recv_overrun:
  ifdef board_features
	test	board_features, BF_NIC_690
	jz	recv_overrun_2
	mov	is_overrun_690, 1
	jmp	recv_no_overrun

recv_overrun_2:
  endif

; Count these things

	add2	soft_rx_overruns,1

; and log them
	SHOW_EVENT	'd'
	mkle LE_RX_OVR_E, 0, 0, 0

; Get the command register TXP bit to test for incomplete transmission later

	loadport		; Point at Chip's Command Reg
 	setport	EN_CCMD		; ..
	pause_
	in 	al, dx
	mov 	ah, al		; Save CR contents in ah for now

; Stop the NIC

	pause_
	mov 	al, ENC_STOP+ENC_NODMA
	out 	dx, al		; Write "stop" to command register

; Wait 1.6ms for the NIC to stop transmitting or receiving a packet. National
; says monitoring the ISR RST bit is not reliable, so a wait of the maximum
; packet time (1.2ms) plus some padding is required.

	longpause

; Clear the remote byte count registers

	xor	al,al
	setport	EN0_RCNTLO	; Point at byte count regs
	out	dx, al
	setport EN0_RCNTHI
	pause_
	out	dx, al

; check the saved state of the TXP bit in the command register

	mov	rcv_ovr_resend,al	; clear the resend flag
	test ah,ENC_TRANS	; Was transmitter still running?
	jz	rcv_ovr_loopback	; Go if not running

; Transmitter was running, see if it finished or died

	setport EN0_ISR		; point at the NIC ISR
	pause_
	in	al,dx		; and get it
	test al,ENISR_TX+ENISR_TX_ERR	; Did the transmitter finish?
	jnz	rcv_ovr_loopback	; one will be set if TX finished

; Transmitter did not complete, remember to resend the packet later.

	mov	rcv_ovr_resend,ah	; ah has at least ENC_TRANS set

; Put the NIC chip into loopback so It won't keep trying to receive into
; a full ring

rcv_ovr_loopback:
	loadport	; resync setport
 	setport	EN0_TXCR	; ..
	mov al,	ENTXCR_LOOP	; Put transmitter in loopback mode
	pause_
	out dx,	al		; ..
	setport	EN_CCMD		; Point at Chip command reg
	mov al,	ENC_START+ENC_NODMA
	pause_
	out dx,	al		; Start the chip running again

; Verify that there is really a packet to receive by fetching the current
; page pointer and comparing it to the next packet pointer.

	mov al, ENC_NODMA+ENC_PAGE1	; Set page one
	pause_
	out dx,al
	setport EN1_CURPAG	; Get current page
	pause_
	in al,dx
	pause_			; Rewrite current page to fix SMC bug.
	out dx,al
	mov bl,al		; save it
	mov save_curr,al
	setport	EN_CCMD		;
	mov al, ENC_NODMA+ENC_PAGE0
	pause_
	out dx,al		; Back to page 0

	mov al, next_packet	; get saved location for next packet

	cmp	al,bl		; Check if buffer emptry
	jne	rcv_ovr_rx_one  ; OK go get the NIC header

; NO PACKET IN THE RING AFTER AN OVW INTERRUPT??? Can this ever happen?
; YES! if overrun happend between a receive interrupt and the when the
; current page register is read at the start of recv_frame.
; Count these things too.

	add2	soft_rx_over_nd,1

	jmp	rcv_ovr_empty


; Get the NIC header for the received packet, and check it

rcv_ovr_rx_one:
	mov	ah,al		; make a byte address. e.g. page
	mov	bl,ah		; and save in bl
	mov	al,0		; 46h becomes 4600h into buffer
	mov	cx,RCV_HDR_SIZE	; size of rcv_hdr
	mov	di,offset rcv_hdr ;point to header
	movseg	es,ds
	call	block_input
	mov al,	rcv_hdr+EN_RBUF_STAT	; Get the buffer status byte
	test al,ENRSR_RXOK	; Is this frame any good?
	jz	rcv_ovr_ng	; Skip if not

;
; EVEN if the NIC header status is OK, I have seen garbaged NIC headers, so
; it doesn't hurt to range check the next packet pointer here.
;
	mov al,	rcv_hdr+EN_RBUF_NXT_PG	; Start of next frame
	mov next_packet, al	; Save next packet pointer
	cmp 	al,SM_RSTART_PG	; Below around the bottom?
	jb	rcv_ovr_ng	; YES - out of range
	cmp	al,sm_rstop_ptr ; Above the top?
	jae	rcv_ovr_ng	; YES - out of range

; ok to call rcv_frm

; This gets real tricky here. Because some wise user (like me) may attempt
; to call send_pkt from his receive interrupt service routine, you can't just
; blindly call rcv_frm from here because the NIC is now in LOOPBACK mode!
; to get around this problem I added a global flag that causes rcv_frm to
; make the first call to the users interrupt service routine, get a buffer,
; fill the buffer, BUT defer the second call until later. If a second call
; is required the flag will still be set after return from rcv_frm and you
; can make the upcall after the NIC has been taken out of loopback and has
; resent any required packet.

	mov	defer_upcall, 1	; Defer upcalls until later. (may be cleared
				;  by rcv_frm)
 	call	rcv_frm		; Yes, go accept it
	jmp	rcv_ovr_ok

rcv_ovr_ng:
	or	byte ptr soft_rx_err_bits,al
	add2	soft_rx_errors,1
	mkle LE_RX_ERR, 0, 0, 0 ; Log error packet - gft - 910521
;
; HAD TO ADD ERROR RECOVERY HERE. TO BLINDLY PROCEED AND ASSUME THE NEXT
; PACKET POINTR FROM THE NIC HEADER IS VALID IS INVITING DISASTER.
;
; Error recovery consists of killing and restarting the NIC. This drops all
; the packets in the ring, but thats better than winding up in the weeds!
;
; - gft - 910611

ifdef err_stop
	call	rcv_mode_1	; for debugging, stop on error
	jmp	check_isr	; so we can dump the log
endif

; NO! no longer in memory
;       call    etopen_0        ; go back to the initial state
;
; Instead copy the last known current page pointer into the next packet pointer
; which will result in skipping all the packets from the errored one to where
; the NIC was storing them when we entered this ISR, but prevents us from
; trying to follow totally bogus next packet pointers through the card RAM
; space.
;
        mov     al, save_curr   ; get the last known current page pointer
        mov     next_packet, al ; and use it as the next packet pointer
        jmp     check_isr       ; then go handle more interrupts

rcv_ovr_ok:
; moved the next two instructions up to where I range check the
; next packet pointer above - gft - 910611
;	mov	al,rcv_hdr+EN_RBUF_NXT_PG ; Get pointer to next frame
;	mov	next_packet, al ; save it in next_packet - gft - 910603
	mov	al,next_packet	; Grap the next packet pointer
	dec	al		; Back up one page
	cmp	al,SM_RSTART_PG	; Did it wrap?
	jae	rcv_ovr_nwr2
	mov	al,sm_rstop_ptr	; Yes, back to end of ring
rcv_ovr_empty:
	dec	al
rcv_ovr_nwr2:
	loadport		; Point at boundary reg
	setport	EN0_BOUNDARY	; ..
	pause_
	out dx,	al		; Set the boundary
; When we get here we have either removed one packet from the ring and updated
; the boundary register, or determined that there really were no new packets
; in the ring.

; Clear the OVW bit in the ISR register.

	loadport		; resync the setport macro
	setport EN0_ISR		; point at the ISR 
	mov al,	ENISR_OVER	; Clear the overrun interrupt bit
	pause_
	out dx,	al

; Take the NIC out of loopback

	setport	EN0_TXCR	; Back to TX control reg
	xor al,	al		; Clear the loopback bit
	pause_
	out dx,	al		; ..

; Any incomplete transmission to resend?

	cmp rcv_ovr_resend,0
	jz  rcv_ovr_deferred	; no, go check for deferred upcall

; Yes, restart the transmission

	setport EN_CCMD	; point at command register
	mov al,	ENC_TRANS+ENC_NODMA+ENC_START
	pause_
	out dx,	al		; Start the transmitter

; If an upcall was deferred, make it now

rcv_ovr_deferred:
	cmp	defer_upcall,0
	jz	rcv_ovr_done
	mov	si,defer_si		; Recover pointer to destination
	mov	cx,defer_cx		; And it's this long
	mov	ds,defer_ds		; Tell client it's his source
	assume	ds:nothing
	call	recv_copy	; Give it to him
	movseg	ds,cs
	assume	ds:code

; log the end of overrun processing, with which flags were set

rcv_ovr_done:
ifdef debug
	mov 	ax, WORD PTR rcv_ovr_resend
	mov 	bx, WORD PTR defer_upcall
	mkle	LE_RX_OVR_X, ax, bx, 0
endif

	mov	defer_upcall,0	; clear the defer upcall flag

; finally go back and check for more interrupts

 	jmp	check_isr	; Done with the overrun case

; End of new receive overrun code

recv_no_overrun:
; Handle receive flags, normal and with error (but not overrun).
	test al,ENISR_RX+ENISR_RX_ERR	; Frame received without overrun?
	jnz	recv_frame_0	; Go if so.

  ifdef board_features
	cmp	is_overrun_690, 0
	jne	recv_overrun_11
	jmp	recv_no_frame	; Go if not.
recv_overrun_11:
	jmp	recv_690_overrun
  else
	jmp	recv_no_frame	; Go if not.
  endif

;
; Move the label recv_frame down to below where the interrupts are cleared.
; This will cause the interrupts to be cleared only after being read in 
; check_isr, instead of every time a packet is read from the ring. The way
; it used to work was find a RX or RX_ERR interrupt, clear both, check for
; packet really in ring (compare next packet pointer with current page reg)
; read packet and loop back to clear both ints. When all packets read from
; ring, loop back to check_isr. IF packets keep arriving as fast as or faster
; than we can read them, we never get back to check_isr to see if any higher
; priority interrupts occur (like OVW).
;
; The way it works now is find a RX or RX_ERR interrupt, clear both, check
; for packet really in ring and remember the current page register. Read
; packets from the ring until all packets received at the time the current
; page register was last read have been received, (comparing next packet
; pointer to the once read value of current page register). This eliminates
; both the (possibly unnecessary) resetting of the interrupts and the 
; page switch and current page register read on a per packet basis. This should
; also eliminate possible problems with not doing the ring overflow processing
; in heavy traffic referred to in my comments below.
;
;  - gft - 910611
;
recv_frame_0:

	loadport		; Point at Chip's Command Reg
	setport	EN0_ISR		; Point at Interrupt status register
	pause_
	mov al,	ENISR_RX+ENISR_RX_ERR
	out dx,	al		; Clear those requests
 	setport	EN_CCMD		; ..
	pause_
	cli
	mov al,	ENC_NODMA+ENC_PAGE1+ENC_START
	out dx,	al		; Switch to page 1 registers
	setport	EN1_CURPAG	;Get current page of rcv ring
	pause_
	in al,	dx		; ..
;	mov ah,	al		; Hold current page in AH
	mov save_curr,al	; Hold last read current page register in 
				; memory instead - gft - 910611
 	setport	EN_CCMD		; Back to page zero registers
	pause_
	mov al,	ENC_NODMA+ENC_PAGE0+ENC_START
	out dx,	al		; Switch back to page 0 registers

; This becomes the loop back point to read packets from the ring.
; now only loop back and read until those packets received at the time 
; the current page register is read above have been read.
; - gft - 910611

recv_frame:
	mov al, next_packet	; Get saved pointer to next packet in ring
;
; change the next instruction to compare against the saved copy of the current
; page register and only read from the ring what was received up until the 
; last read from the current page register - gft - 910611
;
;	cmp al,	ah		; Read all the frames?
	cmp al,	save_curr	; Read all the frames?
	jne	recv_more_frames; hacked jump code for addition of mkle
				; macro below - gft -910521
;	jmp	recv_frame_break	; Finished them all
;
; changed jmp recv_frame_break to jmp check_isr after recv_frame_break was
; determined to be superfluous. See comments at recv_frame_break below.
; - gft - 910531
;
  ifdef board_features
	cmp	is_overrun_690, 0
	je	recv_not_690_overrun

recv_690_overrun:
	mov	is_overrun_690, 0	; clear overrun indicator
	loadport
	setport	EN0_BOUNDARY		; rewrite bndry with itself
	in	al, dx
	pause_
	out	dx, al

	setport	EN0_ISR			; Point at Interrupt status register
	pause_
	mov	al, ENISR_OVER		; Clear overrun interrupt bit
	out	dx, al
	call	count_in_err		; Count the anomaly

recv_not_690_overrun:
  endif
	jmp	check_isr	; Finished all receives, check for more
				; interrupts.

recv_more_frames:

	mov	ah,al		; make a byte address. E.G. page
	mov	al,0		; 46h becomes 4600h into buffer
	mov	bl,ah
	mov	cx,RCV_HDR_SIZE
	mov	di,offset rcv_hdr
	movseg	es,ds
	call	block_input
	mov al,	rcv_hdr+EN_RBUF_STAT	; Get the buffer status byte
	test al,ENRSR_RXOK	; Good frame?
	jz	recv_err_no_rcv
;
; EVEN if the NIC header status is OK, I have seen garbaged NIC headers, so
; it doesn't hurt to range check the next packet pointer here.
;
	mov al,	rcv_hdr+EN_RBUF_NXT_PG	; Start of next frame
	mov next_packet, al	; Save next packet pointer
	cmp 	al,SM_RSTART_PG	; Below around the bottom?
	jb	recv_err_no_rcv ; YES - out of range
	cmp	al,sm_rstop_ptr ; Above the top?
	jae	recv_err_no_rcv ; YES - out of range

; ok to call rcv_frm

	call	rcv_frm		; Yes, go accept it
	jmp	recv_no_rcv
recv_err_no_rcv:
	or	byte ptr soft_rx_err_bits,al
	add2	soft_rx_errors,1
;
; The code used to assume that after decoding the NIC header status
; byte as a receive error, the status, and next packet pointer values
; are actually valid. When using the NIC, this assumption may get one
; up that proverbial creek without a paddle. It has been my experience that
; IF bad status is read in the NIC header, the rest of the NIC header is
; not to be trusted. More likely you read a NIC header from the middle of a
; packet than actually receiving a frame, especially if the save error packet
; bit (SEP) in the receive configuration register is NOT set (it's NOT set in
; this driver).
;
; Error recovery consists of killing and restarting the NIC. This drops all
; the packets in the ring, but thats better than winding up in the weeds!
;
; - gft - 910611

ifdef err_stop
	call	rcv_mode_1	; for debugging, stop on error
	jmp	check_isr	; so we can dump the log
endif

; NO! no longer in memory
;       call    etopen_0        ; go back to the initial state
;
; Instead copy the last known current page pointer into the next packet pointer
; which will result in skipping all the packets from the errored one to where
; the NIC was storing them when we entered this ISR, but prevents us from
; trying to follow totally bogus next packet pointers through the card RAM
; space.
;
        mov     al, save_curr   ; get the last known current page pointer
        mov     next_packet, al ; and use it as the next packet pointer
        jmp     check_isr       ; then go handle more interrupts

recv_no_rcv:
; moved the next two instructions up to where I range check the
; next packet pointer above - gft - 910611
;	mov al,	rcv_hdr+EN_RBUF_NXT_PG	; Start of next frame
;	mov next_packet, al	; Save next packet pointer
	mov	al,next_packet	; Grap the next packet pointer
	dec	al		; Make previous page for new boundary

; Here's another little bug, which was exposed when when I expanded the
; recieve packet ring for HP 16 bit cards (from 06h-7fh to 06h-ffh).
; The jge instruction is for SIGNED comparisons and failed when the next
; packet pointer got into the range 81h-0feh. Changed the below jge to
; a jae for unsigned comparsions. - gft - 910610
; (Also scanned hppclan.asm and 8390.asm for improper use of signed 
; comparisons and found no others.)

	cmp al,	SM_RSTART_PG	; Wrap around the bottom?
;	jge	rcv_nwrap4
	jae	rcv_nwrap4		; - gft - 910610
	mov al,	sm_rstop_ptr	; Yes
	dec al
rcv_nwrap4:
	loadport		; Point at the Boundary Reg again
 	setport	EN0_BOUNDARY	; ..
	pause_
	out dx,	al		; Set new boundary
; This is real bizarre. The NIC DP8390 Datasheet Addendum, June 1990, states
; that:
;
;    In heavily loaded networks which cause overflows of the Recieve Buffer
;    Ring, the NIC may disable the local DMA and suspend further receptions
;    even if the Boundary register is advanced beyond the Current register.
;    In the event that the Network Interface Controller (DP8390 NIC) should
;    encounter a receiver buffer overflow, it is necessary to implement the
;    following routine. A receive buffer overflow is indicated by the NIC's
;    assertion of the overflow bit (OVW) in the interrupt status register
;    (ISR).
;
;    If this routine is not adhered to, the NIC may act in an unpredictable
;    manner. It should also be noted that it is not permissible to service
;    an overflow interrupt by continuing to empty packets from the receive
;    buffer without implementing the prescribed overflow routine.
;    ...
;
; The overflow routine is the one implemented at recv_overrun.
;
; The funny thing is that the way this code is written, in a heavily loaded
; network, you could NEVER NOTICE THAT AN OVERRUN OCCURED. If the NIC does
; NOT suspend further receptions even though the Boundary register is advanced
; (like is says the NIC MAY do above), you will simply receive each frame,
; advance the boundary pointer (allowing the NIC to receive another frame),
; and jump from here back to recv_frame to read the next packet, NEVER CHECKING
; ISR for OVW. If the NIC NEVER stops, and the network is heavily loaded
; enough you could go round and round forever.
;
; So what's the problem you ask? If the NIC DOES stop receiving, you will
; process every packet in the ring before you notice that there is an overrun!
; Instead of dropping a few packets here and a few packets there you will drop
; large blocks of packets because you have taken the time to empty the ring
; completely before turning the NIC back on.
;
; The solution is to check here for an OVW after each pass and jump to
; recv_overrun if required.
;
;	setport	EN0_ISR		; Point at interrupt status register
;	pause_
;	in al,	dx		; Get pending interrupts
;ifdef debug
;	mov log_isr,al
;endif
;	test al,ENISR_OVER	; Was there an overrun?
;	jz	recv_frame_loop	; Go if not.
;	jmp	recv_overrun	; Go if so.
;recv_frame_loop:
;
; But that's a performance hit and it may not be necessaary. I have not yet
; been able to tell if the NIC is stopping (like National says it MAY do), or 
; if it keeps on receiving as soon as the boundary pointer is advanced. It 
; SEEMS to keep on working fine.
;
; Therefore I'm not going to put that overrun check in here and I'll
; live with this routine as long as it seems to be working fine.
;
; One more thought, if this code is implemented, it shoud be moved to the
; beginning of recv_frame before the RX interrupts are cleared. Recv_overrun
; only receives ONE packet and jumps back to check_isr, and if you don't move 
; this routine you could exit with packets in the ring and not know it because
; you cleared the interrupt bits.
;
;  - gft - 910606
;
; New thoughts and fixes - gft - 910611
; 
; A compromise fix which also MINIMIZES NIC accesses is to remember the
; contents of the current page register after entry to recv_frame and then
; remove from the ring only those packets received at that point. Then go
; back to check_isr and catch any new packets or receive overruns. See comments
; at the start of recv_frame.
;
	jmp	recv_frame	; See if any more frames

;
; The next bit of code following recv_frame_break: is superfluous. 
;   1. recv_frame_break: is not public, it can't be reached from outside.
;   2. a grep of all the Clarkson packet driver collection reveals only
;      one jump to recv_frame_break.
;   3. Just prior to that jump, the command register was set to 
;      ENC_NODMA+ENC_PAGE0+ENC_START, followed by a setport to EN0_BOUNDARY
;      and a fetch of the boundry register.
; Therefore I am commenting out the following bit of code, and changing the
; jmp recv_frame_break (formerly je recv_frame_break) to a jmp check_isr.
; - gft - 910531
;
;recv_frame_break:
;	loadport		; Point at Command register
; 	setport	EN_CCMD		; ..
;	pause_
;	mov al,	ENC_NODMA+ENC_PAGE0+ENC_START
;	out	dx,al
;	jmp	check_isr	; See if any other interrupts pending
;

recv_no_frame:				; Handle transmit flags.
	test al,ENISR_TX+ENISR_TX_ERR	; Frame transmitted?
	jnz	isr_tx		; Go if so.
	jmp	isr_no_tx	; Go if not.
isr_tx:
	mov ah,	al		; Hold interrupt status bits
	loadport		; Point at Transmit Status Reg
 	setport	EN0_TSR		; ..
	pause_
	in al,	dx		; ..

; New TX interrupt acknowledge code follows:

	mov	ah,al		; save the TSR state

	setport EN0_ISR		; Point at the interrupt status register
	pause_

; Since transmissions happen synchronously to this driver (even if as_send_pkt
; is implemented, their still synchronous to the driver), only one of the two
; possible TX interrupts may occur at any one time. Therefore it is OK to
; zap both TX and TX_ERR bits in the interrupt status register here.

	mov	al,ENISR_TX+ENISR_TX_ERR ; clear either possible TX int bit
	out	dx,al

	test ah,ENTSR_COLL+ENTSR_COLL16+ENTSR_FU+ENTSR_OWC
	jz	isr_tx_done	; No usefull errors, skip. See the called
				; routine for explanation of the selection
				; of TSR bits for consideration as errors.
	call	count_tx_err

isr_tx_done:
; If TX queue and/or TX shared memory ring buffer were being
; used, logic to step through them would go here.  However,
; in this version, we just clear the flags for background to notice.

 	jmp	check_isr	; See if any other interrupts on

isr_no_tx:
; Now check to see if any counters are getting full
	test al,ENISR_COUNTERS	; Interrupt to handle counters?
	jnz	isr_stat	; Go if so.
	jmp	isr_no_stat	; Go if not.
isr_stat:
; We have to read the counters to clear them and to clear the interrupt.
; Version 1 of the PC/FTP driver spec doesn't give us
; anything useful to do with the data, though.
; Fix this up for V2 one of these days.
	loadport		; Point at first counter
 	setport	EN0_COUNTER0	; ..
	pause_
	in al,	dx		; Read the count, ignore it.
	setport	EN0_COUNTER1
	pause_
	in al,	dx		; Read the count, ignore it.
	setport	EN0_COUNTER2
	pause_
	in al,	dx		; Read the count, ignore it.
	setport	EN0_ISR		; Clear the statistics completion flag
	pause_
	mov al,	ENISR_COUNTERS	; ..
	out dx,	al		; ..
isr_no_stat:
 	jmp	check_isr	; Anything else to do?

interrupt_done:
	loadport
	setport	EN0_IMR		; Tell card it can cause these interrupts
	pause_
	mov al,	ENISR_ALL
	out dx,	al
	ret

; Do the work of copying out a receive frame.
; Called with bl/ the page number of the frame header in shared memory

	public	rcv_frm
rcv_frm:
	mkle LE_RCVFRM_E, 0, 0, 0

; Set cx to length of this frame.
	mov ch,	rcv_hdr+EN_RBUF_SIZE_HI	; Extract size of frame
	mov cl,	rcv_hdr+EN_RBUF_SIZE_LO	; Extract size of frame
	sub cx,	EN_RBUF_NHDR		; Less the header stuff
; Set es:di to point to Ethernet type field.
	mov di,	offset rcv_hdr+EN_RBUF_NHDR+EADDR_LEN+EADDR_LEN
	push	bx			; save page.
	push	cx			; Save frame size
	push	es
	mov ax,	cs			; Set ds = code
	mov ds,	ax
	mov es,ax
	assume	ds:code

	mov	dl, BLUEBOOK		;assume bluebook Ethernet.
	mov	ax, es:[di]
	xchg	ah, al
	cmp 	ax, 1500
	ja	BlueBookPacket
	inc	di			;set di to 802.2 header
	inc	di
	mov	dl, IEEE8023
BlueBookPacket:

	call	recv_find		; See if type and size are wanted
	pop	ds			; RX page pointer in ds now
	assume	ds:nothing
	pop	cx
	pop	bx
	cld			; Copies below are forward, please
	mov ax,	es		; Did recv_find give us a null pointer?
	or ax,	di		; ..
	je	rcv_no_copy	; If null, don't copy the data

	push	cx		; We will want the count and pointer
	push	es		;  to hand to client after copying,
	push	di		;  so save them at this point
	mov	ah,bl		; set ax to page to start from
	mov	al,EN_RBUF_NHDR	; skip the header stuff
	call	block_input
;
; Added defer upcall code here in support of new ring overrun code.
; If defer_upcall is set, don't call recv_copy, just save si,ds,cx
; and the receive ring overrun code will call recv_copy later.
;  - gft - 910604
;
	cmp	defer_upcall,0	; is deferral required?
	jz	rcv_copy_ok	; No, finish of the copy.
	pop	ax		; Yes, save the parameters for recv_copy
	mov	defer_si,ax	; to use later.
	pop	ax
	mov	defer_ds,ax
	pop	ax
	mov	defer_cx,ax
	jmp	rcv_copy_deferred

rcv_copy_ok:
	pop	si		; Recover pointer to destination
	pop	ds		; Tell client it's his source
	pop	cx		; And it's this long
	assume	ds:nothing
	call	recv_copy	; Give it to him
rcv_no_copy:
	movseg	ds,cs
	assume	ds:code
	mov	defer_upcall,0	; clear the defer upcall flag - gft - 910604
rcv_copy_deferred:
	mkle LE_RCVFRM_X, 0, 0, 0
	ret			; That's it for rcv_frm

	include	multicrc.asm
	include	timeout.asm

	public	timer_isr
timer_isr:
;if the first instruction is an iret, then the timer is not hooked
	iret

;any code after this will not be kept after initialization. Buffers
;used by the program, if any, are allocated from the memory between
;end_resident and end_free_mem.
	public end_resident,end_free_mem
end_resident	label	byte
end_free_mem	label	byte

;standard EN0_DCFG contents:
endcfg	db	048h			; Set burst mode, 8 deep FIFO

; Called once to initialize the card

	public	etopen
etopen:				; Initialize interface

;Step 1. Reset and stop the 8390.

	call	reset_board

;Step 2. Init the Data Config Reg.

	loadport
	mov	al,endcfg
	setport	EN0_DCFG
	pause_
	out	dx,al

;Step 2a.  Config the Command register to page 0.
	loadport
	mov	al, ENC_PAGE0 + ENC_NODMA + ENC_STOP
	setport	EN_CCMD
	pause_
	out	dx,al

;Step 3. Clear Remote Byte Count Regs.

	mov	al, 0
	setport	EN0_RCNTLO
	pause_
	out	dx,al
	setport	EN0_RCNTHI
	pause_
	out	dx,al

;Step 4. Set receiver to monitor mode

	mov	al, ENRXCR_MON
	setport	EN0_RXCR
	pause_
	out	dx,al

;Step 5. Place NIC into Loopback Mode 1.

	mov	al,ENTXCR_LOOP
	setport	EN0_TXCR
	pause_
	out	dx,al

;Step 6. Do anything special that the card needs.  Read the Ethernet address
;into rom_address.

	call	init_card		;init the card as needed.
	jnc	etopen_1		;go if it worked.
	ret
etopen_1:

;Step 7. Re-init endcfg in case they put it into word mode.

	loadport
	mov	al,endcfg
	setport	EN0_DCFG
	pause_
	out	dx,al

;Step 8. Init EN0_STARTPG to same value as EN0_BOUNDARY

	mov	al,SM_RSTART_PG
	setport	EN0_STARTPG
	pause_
	out	dx,al
  if 1	;Paul Kranenburg suggests that this should be set to zero.
	mov	al,SM_RSTART_PG
  else
	mov	al,sm_rstop_ptr
	dec	al
  endif
	setport	EN0_BOUNDARY
	pause_
	out	dx,al
	mov	al,sm_rstop_ptr
	setport	EN0_STOPPG
	pause_
	out	dx,al

;Step 9. Write 1's to all bits of EN0_ISR to clear pending interrupts.

	mov	al, 0ffh
	setport	EN0_ISR
	pause_
	out	dx,al

;Step 10. Init EN0_IMR as desired.

	mov	al, ENISR_ALL
	setport	EN0_IMR
	pause_
	out	dx,al

;Step 11. Init the Ethernet address and multicast filters.

	mov	si,offset rom_address
	mov	cx,EADDR_LEN
	call	set_address	; Now set the address in the 8390 chip
	call	set_hw_multi  ; Put the right stuff into 8390's multicast masks

;Step 12. Program EN_CCMD for page 1.

	loadport
	mov	al, ENC_PAGE1 + ENC_NODMA + ENC_STOP
	setport	EN_CCMD
	pause_
	out	dx,al

;Step 13. Program the Current Page Register to same value as Boundary Pointer.

; THIS IS WRONG WRONG WRONG inspite of some self-contradicting National
; documentation. If the Current Page Register is initialized to the same
; value as the Boundary Pointer, the first ever packet received will be lost or
; trashed because the driver always expects packets to be received at Boundrary
; pointer PLUS ONE! 

	mov	al,SM_RSTART_PG
	inc	al		; To fix the bug! - gft - 910523
	setport	EN1_CURPAG
	pause_
	out	dx,al
	mov	save_curr,al	; added in conjunction with fixes above
				; - gft - 910611
	mov	next_packet, al ; initialize next_packet to the value in
				; current - gft - 910603

;Step 14. Program EN_CCMD back to page 0, and start it.

	mov	al, ENC_NODMA + ENC_START + ENC_PAGE0
	setport	EN_CCMD
	pause_
	out	dx,al

	mov	al, 0			;set transmitter mode to normal.
	setport	EN0_TXCR
	pause_
	out	dx,al

  if 0
	mov	al, ENRXCR_BCST
	setport	EN0_RXCR
	pause_
	out	dx,al
  endif

	call	set_recv_isr	; Put ourselves in interrupt chain

	mov	al, int_no		; Get board's interrupt vector
	add	al, 8
	cmp	al, 8+8			; Is it a slave 8259 interrupt?
	jb	set_int_num		; No.
	add	al, 70h - 8 - 8		; Map it to the real interrupt.
set_int_num:
	xor	ah, ah			; Clear high byte
	mov	int_num, ax		; Set parameter_list int num.

	clc				; Say no error
	ret				; Back to common code

