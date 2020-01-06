PAGE    66,132
;
;
;
;
;
;
;
;
;  outs.asm
;
;  print a string to stdio
;
;
;  Usage :
;	    outch( ch )
;
;  (c) 1990 Erick Engelke
;
;  version
;
;    0.1    7-Nov -1990   E. P. Engelke
;
;
	include masmdefs.hsm
	include	model.hsm

codedef OUTS
datadef

        cextrn  outch

cstart  OUTS
cpublic outs
	push	DS
	lds	SI, +@AB [BP]
@1:	lodsb
        or      AL, AL
	jz 	@2

        cmp     AL, 0dh         ;convert 0d to 0d 0a
        jnz     @3

        push    AX
        ccall    outch
        pop     AX
;        mov     DL, AL
;        mov     AX, 020ah
;        int     21h

@3:     push    AX
        ccall    outch
        pop     AX

;        mov     DL, AL
;        mov     AH, 2
;        int     21h
	jmp	@1

@2:	pop	DS
creturn outs
cend    OUTS
        end

