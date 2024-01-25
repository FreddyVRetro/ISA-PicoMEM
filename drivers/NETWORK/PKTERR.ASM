;put into the public domain by Russell Nelson, nelson@crynwr.com

err0	db	"No error at all.",'$'
err1	db	"Invalid handle number",'$'
err2	db	"No interfaces of specified class found",'$'
err3	db	"No interfaces of specified type found",'$'
err4	db	"No interfaces of specified number found",'$'
err5	db	"Bad packet type specified",'$'
err6	db	"This interface does not support multicast",'$'
err7	db	"This packet driver cannot terminate",'$'
err8	db	"An invalid receiver mode was specified",'$'
err9	db	"Operation failed because of insufficient space",'$'
err10	db	"The type had previously been accessed, and not released.",'$'
err11	db	"The command was out of range, or not implemented",'$'
err12	db	"The packet couldn't be sent (usually hardware error)",'$'
err13	db	"Hardware address couldn't be changed (more than 1 handle open)",'$'
err14	db	"Hardware address has bad length or format",'$'
err15	db	"Couldn't reset/initialize interface (more than 1 handle open)",'$'
err16	db	"An invalid iocb was specified",'$'
errunk	db	"Unknown error",'$'

errlist	dw	err0, err1, err2, err3, err4, err5, err6, err7, err8
	dw	err9, err10, err11, err12, err13, err14, err15, err16
error_count	equ	($ - errlist)/2

fatal_error:
	call	print_error
	jnc	fatal_error_1		;Only terminate if there really
	int	20h			;  was an error.
fatal_error_1:
	ret

print_error:
;enter with cy set if an error occured, dh is the error number.  If cy
;  is not set, don't print anything.  Don't change any registers.
	pushf
	jnc	print_error_1
	or	dh,dh
	je	print_error_1
	push	ax
	push	bx
	push	dx
	push	ds

	movseg	ds,cs

	mov	bl,dh
	xor	bh,bh
	mov	dx,offset errunk	;in case we don't know about it.
	cmp	bl,error_count		;Do we know about this error number?
	jae	print_error_2		;  no, bail out.
	shl	bx,1
	mov	dx,errlist[bx]
print_error_2:
	mov	ah,9
	int	21h

	mov	al,13			;crlf.
	call	chrout
	mov	al,10
	call	chrout

	pop	ds
	pop	dx
	pop	bx
	pop	ax
print_error_1:
	popf
	ret
