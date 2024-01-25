;put into the public domain by Russell Nelson, nelson@clutx.clarkson.edu

	public	crlf
crlf:
	push	ax
	push	dx
	mov	dl,13
	mov	ah,2
	int	21h
	mov	dl,10
	mov	ah,2
	int	21h
	pop	dx
	pop	ax
	ret
