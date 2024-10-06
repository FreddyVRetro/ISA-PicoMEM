; ### This file is part of MOUSETST, by Eric Auer. Uses NASM Syntax.
; MOUSETST is free software; you can redistribute it and/or modify it
; under the terms of the GNU General Public License as published
; by the Free Software Foundation; either version 2 of the License,
; or (at your option) any later version. See http://www.gnu.org/
; ### MOUSETST does not come with ANY warranty...!

        org 100h        ; a com file
        cpu 8086
	; one could check if the int 33 vector is 0:0 first

	xor bp,bp
	mov ax,11h	; test for wheel support
	int 33h
	cmp ax,574dh	; "WM"
	jnz nowheelsupport
	or bp,1		; "have wheel support in driver"
	test cx,1
	jz nowheelpresent
	or bp,2		; "this mouse has a wheel"
nowheelpresent:
nowheelsupport:
	mov [cs:wheels],bp

	call fillscreen
	call hookmouse

key_loop:
	mov ax,[cs:buttons]
	or ax,ax
	jns no_update_screen
	and ax,7fffh
	mov [cs:buttons],ax
update_screen:
	call showstatus
no_update_screen:
	mov ah,1	; check for keystroke
	int 16h
	jz key_loop	; no key pressed
	mov ah,0	; fetch key
	int 16h
	; hotkeys: ESCape, mode 0..9, Show +/s, Hide -/h, Reset, Window,
	; fonts x..z, 
	cmp ah,1	; ESC?
	jz exit
	cmp al,27	; ESC?
	jnz no_exit
exit:
	mov dx, handler	; bogus, no longer called
	; ES already is segment handler
	xor cx,cx	; event mask: no events
	mov ax,0ch	; install handler
	int 33h
	mov ax,3	; text mode, clear screen
	int 10h
	mov ah,9	; show string
	mov dx,version	; offset
	int 21h
	mov ax,4c00h	; exit, errorlevel 0
	int 21h
no_exit:	
	cmp al,'s'
	jnz no_show
m_show:	mov ax,1	; show mouse cursor
	int 33h
	jmp key_loop
no_show:
	cmp al,'h'
	jnz no_hide
m_hide:	mov ax,2	; hide mouse cursor
	int 33h
	jmp key_loop
no_hide:
	cmp al,30h	; digit?
	jb no_digit
	cmp al,37h	; at most 7 (modes 0..7 supported)
	ja no_digit
	sub al,30h	; convert to hex
mode_common:
	mov ah,0	; set mode
	int 10h
	call fillscreen
	jmp update_screen
no_digit:
	cmp al,'r'
	jnz no_r
	mov ax,0	; reset driver, install check
	int 33h
	cmp ax,-1
	jz have_mouse
	jmp exit	; no driver present!
have_mouse:
	; also returns BX button count, 0 or -1 can mean "2"
	call hookmouse	; activate handler again
	; cursor is invisible after reset, so you want to hit + next
	jmp key_loop
no_r:
	cmp al,'w'
	jnz no_w
	mov cx,-40
	mov dx,640+40
	mov ax,7	; set window limits / X range
	int 33h
	mov cx,-20
	mov dx,480+20
	mov ax,8	; set window limits / Y range
	int 33h
	; you may want to enable the cursor again next
	jmp key_loop
no_w:
	cmp al,'x'
	jnz no_x
	mov ax,1112h	; select font 8x8
font_common:
	mov bl,0
	int 10h
	call fillscreen
	mov ax,21h	; software reset
	int 33h
	call hookmouse
	mov ax,1	; show cursor
	int 33h
	jmp key_loop
no_x:
	cmp al,'y'
	jnz no_y
	mov ax,1111h	; select font 8x14
	jmp short font_common
no_y:
	cmp al,'z'
	jnz no_z
	mov ax,1114h	; select font 8x16
	jmp short font_common
no_z:
	cmp al,'a'
	jb no_xmode
	cmp al,'g'
	ja no_xmode
	sub al,'a'
	add al,13	; a..g -> modes 13..19 (0d..13 hex)
	jmp mode_common
no_xmode:
	jmp key_loop




hookmouse:		; activate handler. destroys regs!
	mov dx, handler	; offset
	; ES already is segment handler
	mov cx,-1	; event mask: all events
	mov ax,0ch	; install handler
	int 33h
	ret


handler:		; store registers for later use and return
;	mov [cs:hand_ax],ax	; event bits
	mov [cs:buttons],bx	; buttons - high byte is wheel info
	mov [cs:column],cx	; column
	mov [cs:row],dx		; row
;	mov [cs:hand_si],si	; delta X
;	mov [cs:hand_di],di	; delta Y
	or word [cs:buttons],8000h	; flag event
	retf


showstatus:
	mov dx,200h	; 3rd row, 1st column
	call setcursor
	mov ax,[cs:wheels]
	mov bx,wheel1n
	test ax,1	; wheel driver support
	jz nobit1
	mov bx,wheel1y
nobit1:	call showstring
	mov ax,[cs:wheels]
	test ax,1	; wheel driver support?
	jz nowheelmsg	; if not, do not show wheel presence bit
	mov bx,wheel2n
	test ax,2	; wheel presence
	jz nobit2
	mov bx,wheel2y
nobit2:	call showstring
nowheelmsg:
	mov ax,[cs:buttons]
	mov bx,btnLn
	test ax,1	; left button
	jz nobit3
	mov bx,btnLy
nobit3:	call showstring
	mov ax,[cs:buttons]
	mov bx,btnMn
	test ax,4	; middle button
	jz nobit4
	mov bx,btnMy
nobit4:	call showstring
	mov ax,[cs:buttons]
	mov bx,btnRn
	test ax,2	; right button
	jz nobit5
	mov bx,btnRy
nobit5:	call showstring
	;
	mov ax,[cs:column]
	or ax,ax
	mov al,'-'
	js negcol
	mov al,' '
negcol: call showtty
	mov ax,[cs:column]
	or ax,ax
	jns poscol
	neg ax
poscol:	call hex2dec
	call showax
	mov al,'/'
	call showtty
	mov ax,[cs:row]
	or ax,ax
	mov al,'-'
	js negrow
	mov al,' '
negrow: call showtty
	mov ax,[cs:row]
	or ax,ax
	jns posrow
	neg ax
posrow:	call hex2dec
	call showax
	mov al,'/'
	call showtty
	mov ax,[cs:buttons]	; buttons, wheel
	mov al,ah		; wheel movement
	cbw
	or ax,ax
	mov al,'-'
	js negwh
	mov al,' '
negwh:	call showtty
	mov ax,[cs:buttons]	; buttons, wheel
	mov al,ah		; wheel movement
	cbw
	or ax,ax
	jns poswh
	neg ax
poswh:	call hex2dec
	call showax
	mov bx,filler
	call showstring
	ret


fillscreen:		; fill screen with a pattern. destroys regs!
	xor si,si	; row
	xor di,di	; column
	xor bp,bp	; high: color, low: char
fill_loop:
	mov ax,di
	add ax,si
	mov bh,10
	div bh		; ah = (row+column) modulo 10
	add ah,'0'
	mov bl,ah
	mov ax,bp
	mov al,bl	; set char
	inc ah		; color
	cmp ah,16	; wrap
	jnz nocolorwrap
	mov ah,1
nocolorwrap:
	mov bp,ax	; update color, char
	;
	mov cx,si	; row
	mov dx,di	; column, to DL
	mov dh,cl	; row, to DH
	call setcursor
	mov ax,bp	; al char, ah color
	mov bl,ah	; color (AL already is correct)
	cmp si,2	; row 0 1 or 2?
	ja no_upper
	mov bl,7	; fix color (nicer status / menu background)
no_upper:
	mov bh,0	; page
	mov ah,9	; write char in color
	mov cx,1	; one copy
	int 10h
	inc di		; next column
	call getwidth
	cmp di,ax
	jnz fill_loop
	xor di,di
	inc si		; next row
	call getheight
	cmp si,ax
	jnz fill_loop
	;
	xor dx,dx	; 1st row, 1st column: home
	call setcursor
	mov bx, help1	; offset
	call showstring
	mov dx,100h	; 2nd row, 1st column
	call setcursor
	mov bx, help2	; offset
	call showstring
	ret


setcursor:		; move cursor to DX. destroys AX BX
	mov bh,0	; page
	mov ah,2	; move cursor
	int 10h
	ret


showstring:		; show string at cs:bx. destroys AX BX
	xor ax,ax
	mov al,[cs:bx]
	inc bx
	cmp al,10
	jb eof
	call showtty
	jmp short showstring
eof:	xchg ax,bx	; shorter than mov bx,ax
	inc bx
eof2:	dec bx
	jz eof3
	mov al,' '
	call showtty
	jmp short eof2
eof3:	ret


getwidth:	; return number of text screen columns in AX
	push ds
	mov ax,40h
	mov ds,ax
	mov ax,[ds:4ah]
	pop ds
	ret


getheight:	; return number of text screen rows in AX
	push ds
	mov ax,40h
	mov ds,ax
	xor ax,ax
	mov al,[ds:84h]
	pop ds
	or ax,ax
	jnz vgaheight
	mov al,25-1
vgaheight:
	inc ax
	ret


%include "display.asm"	; ax=hex2dec(ax), showax(ax), showtty(al) ...


wheels	dw 0	; 1 driver, 2 mouse
buttons	dw 0	; high byte: wheel delta, low byte: 1 L 2 R 4 M
column	dw 0
row	dw 0

version	db "Mousetst 2007 by Eric Auer, License GPL2",13,10,"$"
	; showstring strings: bytes < 10 mean "add N spaces and stop"
help1	db "s show, h hide, r reset, w window,",6
help2	db "x/y/z fonts, ESC exit, 0..7/a..g modes",2
filler	db 9	; for 000 <-> 64048099 length difference

wheel1n	db "Wheel n/a",1	; driver has no wheel support
wheel1y db "Wheel:",1		; wheel supported
wheel2n	db "off",1		; no wheel mouse
wheel2y	db "YES",1		; wheel mouse present
btnLn	db "-l-",2
btnLy	db "LEFT",1
btnMn	db "-m-",1
btnMy	db "MID",1
btnRn	db " -r-",2
btnRy	db "RIGHT",1

