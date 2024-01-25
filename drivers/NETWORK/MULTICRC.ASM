	public	set_multicast_list
set_multicast_list:
;enter with ds:si ->list of multicast addresses, ax = number of addresses,
;  cx = number of bytes.
;return nc if we set all of them, or cy,dh=error if we didn't.
	assume ds:code

	mov	cx,ax			;keep a count of addresses in cx.
	mov	di,offset mcast_list_bits
	xor	ax,ax
	mov	[di+0],ax
	mov	[di+2],ax
	mov	[di+4],ax
	mov	[di+6],ax
	jcxz	set_mcl_2
set_mcl_1:
	call	add_mc_bits
	loop	set_mcl_1
set_mcl_2:
	call    set_hw_multi	; Set the multicast mask bits in chip
	clc
	ret

add_mc_bits:
;entry:	ds:si -> multicast address, di-> sixty-four bit multicast filter.
;preserve cx, di, increment si by EADDR_LEN
	push	cx
	mov	cx,EADDR_LEN
	mov	dx,0ffffh			; this is msw.
	mov	bx,0ffffh			; set 32 bit number
add_mcb_1:
	lodsb
	call	upd_crc			; update crc
	loop	add_mcb_1		; and loop.

  ifdef MULTICRC_REVERSE
	mov	cl,8
add_mcb_2:
	shl	dh,1
	rcr	dl,1
	loop	add_mcb_2
	mov	dh,dl
  endif

	mov	al,dh			; get ms 8 bits,
	rol	al,1
	rol	al,1
	rol	al,1			; put 3 bits at bottom
	and	al,7
	mov	bl,al			; save in bl
	xor	bh,bh			; make bx into an index to the byte.

	mov	al,dh			; get ms 8 bits,
	ror	al,1
	ror	al,1			; but at bottom
	and	al,7
	mov	cl,al			; save in cl
	mov	al,1
	shl	al,cl			; set the correct bit,

	or	[bx+di],al
	pop	cx
	ret

;
;	dx is high,
;	bx is low.
;	al is data

upd_crc:
	push	cx
	mov	cx,8		; do 8 bits
	mov	ah,0
upd_crc1:
	shl	bx,1		; shift bx
	rcl	dx,1		; through dx
	rcl	ah,1		; carry is at bottom of ah
	xor	ah,al		; xor with lsb of data
	rcr	ah,1		; and put in carry bit
	jnc	upd_crc2
;
;	autodin is x^32+x^26+x^23x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x^1
;
	xor	dx,0000010011000001b
	xor	bx,0001110110110110b + 1	;plus one for end-around carry.
upd_crc2:
	shr	al,1		; shift the data
	loop	upd_crc1
	pop	cx
	ret

