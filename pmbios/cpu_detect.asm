;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Detect the Processor Type -- by Richard C. Leinecker                 ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

_PTEXT      SEGMENT PARA PUBLIC 'CODE'
        ASSUME  CS:_PTEXT, DS:_PTEXT

        public  _Processor
; This routine returns the processor type as an integer value.
; C Prototype
; int Processor( void );
; Returns: 0 == 8088, 1 == 8086, 2 == 80286, 3 == 80386, 4 == 80486
; Code assumes es, ax, bx, cx, and dx can be altered. If their contents
; must be preserved, then you'll have to push and pop them.

_Processor  proc    far

        push    bp          ; Preserve bp
        mov bp,sp           ; bp = sp

        push    ds          ; Preserve ds
        push    di          ; and di

        mov ax,cs           ; Point ds
        mov ds,ax           ; to _PTEXT

        call    IsItAn8088      ; Returns 0 or 2

        cmp al,2            ; 2 = 286 or better
        jge AtLeast286      ; Go to 286 and above code

        call    IsItAn8086      ; Returns 0 or 1

        jmp short ExitProcessor ; Jump to exit code
AtLeast286:
        call    IsItA286        ; See if it's a 286

        cmp al,3            ; 4 and above for 386 and up
        jl  short ExitProcessor ; Jump to exit code
AtLeast386:
        call    IsItA386        ; See if it's a 386
ExitProcessor:
        pop di          ; Restore di,
        pop ds          ; ds,
        pop bp          ; and bp

        ret             ; Return to caller
_Processor  endp

; Is It an 8088?
; Returns ax==0 for 8088/8086, ax==2 for 80286 and above
IsItAn8088  proc
        pushf               ; Preserve the flags

        pushf               ; Push the flags so
        pop bx          ; we can pop them into bx

        and bx,00fffh       ; Mask off bits 12-15
        push    bx          ; and put it back on the stack

        popf                ; Pop value back into the flags
        pushf               ; Push it back. 8088/8086 will
                                                ; have bits 12-15 set
        pop bx          ; Get value into bx
        and bx,0f000h       ; Mask all but bits 12-15

        sub ax,ax           ; Set ax to 8088 value
        cmp bx,0f000h       ; See if the bits are still set
        je  Not286          ; Jump if not

        mov al,2            ; Set for 286 and above
Not286:
        popf                ; Restore the flags

        ret             ; Return to caller
IsItAn8088  endp

; Is It an 8086?
; Returns ax==0 for 8088, ax==1 for 8086
; Code takes advantage of the 8088's 4-byte prefetch queues and 8086's
; 6-byte prefetch queues. By self-modifying the code at a location exactly 5
; bytes away from IP, and determining if the modification took effect,
; you can differentiate between 8088s and 8086s.
IsItAn8086  proc

        mov ax,cs          ; es == code segment
        mov es,ax

        std            ; Cause stosb to count backwards

        mov dx,1           ; dx is flag and we'll start at 1
        mov di,OFFSET EndLabel ; di==offset of code tail to modify
        mov al,090h        ; al==nop instruction opcode
        mov cx,3           ; Set for 3 repetitions
        REP stosb          ; Store the bytes

        cld            ; Clear the direction flag
        nop            ; Three nops in a row
        nop            ; provide dummy instructions
        nop
        dec dx         ; Decrement flag (only with 8088)
        nop            ; dummy instruction--6 bytes
EndLabel:
        nop

        mov ax,dx          ; Store the flag in ax

        ret            ; Back to caller
IsItAn8086  endp

; Is It an 80286?
; Determines whether processor is a 286 or higher. Going into subroutine ax = 2
; If the processor is a 386 or higher, ax will be 3 before returning. The
; method is to set ax to 7000h which represent the 386/486 NT and IOPL bits
; This value is pushed onto the stack and popped into the flags (with popf).
; The flags are then pushed back onto the stack (with pushf). Only a 386 or 486
; will keep the 7000h bits set. If it's a 286, those bits aren't defined and
; when the flags are pushed onto stack these bits will be 0. Now, when ax is
; popped these bits can be checked. If they're set, we have a 386 or 486.
IsItA286    proc
        pushf               ; Preserve the flags
        mov ax,7000h        ; Set the NT and IOPL flag
                        ; bits only available for
                        ; 386 processors and above
        push    ax          ; push ax so we can pop 7000h
                        ; into the flag register
        popf                ; pop 7000h off of the stack
        pushf               ; push the flags back on
        pop ax          ; get the pushed flags
                        ; into ax
        and ah,70h          ; see if the NT and IOPL
                        ; flags are still set
        mov ax,2            ; set ax to the 286 value
        jz  YesItIsA286     ; If NT and IOPL not set
                        ; it's a 286
        inc ax          ; ax now is 4 to indicate
                        ; 386 or higher
YesItIsA286:
        popf                ; Restore the flags

        ret             ; Return to caller
IsItA286    endp

; Is It an 80386 or 80486?
; Determines whether processor is a 386 or higher. Going into subroutine ax=3
; If the processor is a 486, ax will be 4 before leaving. The method is to set
; the AC bit of the flags via EAX and the stack. If it stays set, it's a 486.
IsItA386    proc
        mov di,ax           ; Store the processor value
        mov bx,sp           ; Save sp
        and sp,0fffch       ; Prevent alignment fault
 .386
        pushfd              ; Preserve the flags

        pushfd              ; Push so we can get flags
        pop eax         ; Get flags into eax
        or  eax,40000h      ; Set the AC bit
        push    eax         ; Push back on the stack
        popfd               ; Get the value into flags
        pushfd              ; Put the value back on stack
        pop eax         ; Get value into eax
        test    eax,40000h      ; See if the bit is set
        jz  YesItIsA386     ; If not we have a 386

        add di,2            ; Increment to indicate 486
YesItIsA386:
        popfd               ; Restore the flags
 .8086
        mov sp,bx           ; Restore sp
        mov ax,di           ; Put processor value into ax

        ret             ; Back to caller
IsItA386    endp

.586
; Pentium detect routine. Call only after verifying processor is an i486 or
; later. Returns 4 if on i486, 5 if Pentium, 6 or greater for future
; Intel processors.
EF_ID   equ   200000h      ; ID bit in EFLAGS register
Pentium proc near

; Check for Pentium or later by attempting to toggle the ID bit in EFLAGS reg;
; if we can't, it's an i486.
   pushfd         ; get current flags
   pop   eax         ;
   mov   ebx,eax         ;
   xor   eax,EF_ID   ; attempt to toggle ID bit
   push   eax         ;
   popfd            ;
   pushfd         ; get new EFLAGS
   pop   eax         ;
   push   ebx      ; restore original flags
   popfd            ;
   and   eax,EF_ID   ; if we couldn't toggle ID, it's an i486
   and   ebx,EF_ID      ;
   cmp   eax,ebx         ;
   je   short Is486      ;

; It's a Pentium or later. Use CPUID to get processor family.
   mov   eax,1      ; get processor info form of CPUID
   cpuid            ;
   shr   ax,8      ; get Family field;  5 means Pentium.
   and   ax,0Fh         ;
   ret
Is486:
   mov   ax,4      ; return 4 for i486
   ret
pentium endp

_PTEXT      ENDS
        END