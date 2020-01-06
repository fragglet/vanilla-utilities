PAGE    66,132
;
;
;
;
;
;
;
;
;  outsn.asm
;
;  print a string up to a certain len to stdio
;
;
;  Usage :
;	    outsn( char far *s, int n)
;
;  (c) Erick Engelke
;
;  version
;
;    0.1    7-Nov -1990   E. P. Engelke
;
;
	include masmdefs.hsm
	include	model.hsm

codedef OUTSN
datadef

        cextrn  outch


cstart  OUTSN
cpublic outsn
	push	DS
	lds	SI, +@AB [BP]
	mov	CX, +@AB+4[BP]
@1:	lodsb
        or      AL, AL
        jz      @2

        cmp     AL, 0dh         ; convert 0d to 0d 0a
        jnz     @3

        push    ax
        ccall    outch
        pop     ax

;        mov     DL, AL
;        mov     AX, 020ah
;        int     21h

@3:     push    ax
        ccall    outch
        pop     ax
;        mov     DL, AL
;        mov     AH, 2
;        int     21h
	loop	@1

@2:	pop	DS
creturn outsn
cend    OUTSN
        end

