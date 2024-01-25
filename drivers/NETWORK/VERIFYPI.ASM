signature	db	'PKT DRVR',0
signature_len	equ	$-signature

packet_int_msg	db	CR,LF
		db	"Error: <packet_int_no> should be 0x60->0x66, 0x68->0x6f, or 0x78->0x7e",CR,LF
		db	"       0x67 is the EMS interrupt, and 0x70 through 0x77 are used by second 8259"
		db	'$'

verify_packet_int:
;enter with no special registers.
;exit with cy,dx-> error message if the packet int was bad,
;  or nc,zr,es:bx -> current interrupt if there is a packet driver there.
;  or nc,nz,es:bx -> current interrupt if there is no packet driver there.
	cmp	entry_point,60h		;make sure that the packet interrupt
	jb	verify_packet_int_bad	;  number is in range.
	cmp	entry_point,67h		;make sure that the packet interrupt
	je	verify_packet_int_bad	;  number is in range.
	cmp	entry_point,70h		;make sure that the packet interrupt
	jb	verify_packet_int_ok	;  number is in range.
	cmp	entry_point,78h		;make sure that the packet interrupt
	jb	verify_packet_int_bad	;  number is in range.
	cmp	entry_point,7eh
	jbe	verify_packet_int_ok
verify_packet_int_bad:
	mov	dx,offset packet_int_msg
	stc
	ret
verify_packet_int_ok:

	mov	ah,35h			;get their packet interrupt.
	mov	al,entry_point
	int	21h

	lea	di,3[bx]		;see if there is already a signature
	mov	si,offset signature	;  there.
	mov	cx,signature_len
	repe	cmpsb
	clc
	ret
