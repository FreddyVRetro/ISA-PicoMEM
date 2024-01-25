phd_enviorn	equ	2ch

getenv:
;enter with si->environ string to search for.
;exit with cy if not found, or nc, es:di->value if found.
	mov	es,cs:[phd_enviorn]	;get our enviornment segment.
	xor	di,di
getenv_1:
	push	si
	push	di
getenv_2:
	lodsb				;get a character.
	or	al,al			;end of string?
	je	getenv_3		;yes.
	scasb				;did it match?
	je	getenv_2		;yes.
getenv_3:
	je	getenv_4

	pop	di
	pop	si

	xor	al,al			;skip to the next string.
	mov	cx,100h			;no string can be longer than 256 bytes.
	repnz	scasb
	jne	getenv_5		;go if environment is trashed.

	cmp	byte ptr es:[di],0	;is this the last one?
	jnz	getenv_1	;no - try again.
getenv_5:
	stc
	ret

getenv_4:
	add	sp,4			;pop the old stuff off the stack.
	clc
	ret
