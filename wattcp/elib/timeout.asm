;
;  Timer routines - use set_*timeout to receive a clock value which
;                   can later be tested by calling chk_timeout with
;                   that clock value to determine if a timeout has
;                   occurred.  chk_timeout returns 0 if NOT timed out.
;
;
;  Usage :
;           long set_timeout( int seconds );
;           long set_ttimeout( int pc_ticks );
;           int chk_timeout( long value );
;
;  (c) Erick Engelke
;
;  version
;
;    0.1    7-Nov -1990   E. P. Engelke
;
;
        include masmdefs.hsm
        include model.hsm

BIOSCLK	equ	046ch
HI_MAX	equ	018h
LO_MAX	equ     0b0h

codedef TIMEOUT
datadef

cstart  TIMEOUT

        ; code segment variables

date    dw 0,0
oldhour db 0

cpublic set_ttimeout
        xor     DX, DX
        mov     ES, DX
        mov     AX, +@AB [BP]
        pushf
        cli
        add     AX, ES:[ BIOSCLK ];
        adc     DX, ES:[ BIOSCLK+2];
        add     AX, CS: date            ; date extend
        adc     DX, CS: date + 2
        popf
creturn set_ttimeout

cpublic set_timeout
	xor	AX, AX			; reference low memory
	mov	ES, AX

        mov     AX, +@AB [BP]           ; seconds

	mov	DX, 1165
	mul	DX			; 1165/64 = 18.203...

	mov	CX, 6
tmp:
	shr	DX, 1
	rcr	AX, 1
	loop	tmp

        pushf
        cli
        add     AX, ES:[ BIOSCLK ]
	adc	DX, ES:[ BIOSCLK + 2 ]
        add     AX, CS: date            ; date extend
        adc     DX, CS: date + 2
        popf
creturn set_timeout
cpublic	chk_timeout
	xor	AX, AX
	mov	ES, AX
        pushf
        cli
	mov	CX, ES:[BIOSCLK]
	mov	BX, ES:[BIOSCLK+2]
        popf

        ; see if new date
        cmp     BL, CS:oldhour          ; is it earlier modulo one day
        je      samehr
        jge     sameday

        ; update the new date stuff
        add     CS:date, LO_MAX
        adc     CS:date+2, HI_MAX

sameday:mov     CS:oldhour, BL          ; save new hour for next time

samehr :add     CX, CS: date            ; date extend current time
        adc     BX, CS: date + 2

        mov     AX, +@AB + 0 [BP]       ; get supplied timeout value
	mov	DX, +@AB + 2 [BP]
        
        cmp     DX, BX                  ; if timeout < clock has expired
        jb      ret_tru
        ja      ret_fal
        cmp     AX, CX                  ; still checking timeout < clock
        jb      ret_tru

ret_fal:xor     AX, AX                  ; false
        jmp     retern

ret_tru:mov     AX, 1
retern:
creturn chk_timeout
cend    TIMEOUT
        end
