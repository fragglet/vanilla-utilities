PAGE    66,132
;
;
;
;
;
;
;
;
;  outhex.asm
;
;  print a hex byte to standard out
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

codedef OUTHEX
datadef

        cextrn outch

cstart  OUTHEX

h1      proc near
        mov     AL, +@AB [BP]    ; get byte
        and     AL, 0fh
        cmp     AL, 9
        jle     @1
        add     AL, 'A' - '9' - 1
@1:     add     AL, '0'
;
;        mov     AH, 2
;        int     21h
        push    AX
        ccall    outch
        pop     AX
        ret
h1      endp

cpublic outhex
        mov     AL, +@AB [BP]
        mov     CL, 4
        shr     AL, CL

        mov     DL, AL
        call    h1
        mov     AL, +@AB [BP]
        call    h1
creturn outhex
cend    OUTHEX
        end
