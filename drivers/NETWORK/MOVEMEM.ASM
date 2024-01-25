;put into the public domain by Russell Nelson, nelson@crynwr.com

;movemem has one of three values: movemem_test, movemem_386, movemem_86.
;it's initialized to the first, and the code that it points to changes
;it to the appropriate move routine.

;Some Ethernet boards implement memory that can be accessed only 32 bits
;at a time.  This code will successfully transfer *from* 32-bit memory
;even for byte counts not divisible by four.  To use it to transfer *to*
;32-bit memory, you must first ensure that the count is divisible by four,
;or do a "rep movsd" if you can afford to write extra bytes.
;     Also note that it loads an extra dword from the source even if it
;never uses it.

  ifndef is_386
	extrn	is_386: byte		;=0 if 80[12]8[68], =1 if 80[34]86.
  endif

movemem	dw	movemem_test

movemem_test:
	cmp	cs:is_386,0		;Are we running on a 386?
	mov	ax,offset movemem_86
	je	movemem_test_1		;no.
	mov	ax,offset movemem_386	;yes, use a 386-optimized move.
movemem_test_1:
	mov	cs:movemem,ax
	jmp	ax

movemem_386:
;transfer all complete dwords.
	push	cx
	db	0c1h, 0e9h, 02h		;shr cx,2 - convert byte count to dword
	db	066h
	rep	movsw			;rep movsd
	pop	cx

;now take take of any trailing words and/or bytes.
	db	066h
	push	ax			;push eax
	db	066h
	lodsw				;lodsd

	test	cx,2
	je	movemem_386_one_word
	stosw
	db	066h, 0c1h, 0e8h, 010h	;shr	eax,16
movemem_386_one_word:

	test	cx,1
	je	movemem_386_one_byte
	stosb
movemem_386_one_byte:
	db	066h
	pop	ax			;pop eax
	ret


movemem_86:
;does the same thing as "rep movsb", only 50% faster.
	shr	cx,1			; convert to word count
	rep	movsw			; Move the bulk as words
	jnc	movemem_cnte		; Go if the count was even
	movsb				; Move leftover last byte
movemem_cnte:
	ret


