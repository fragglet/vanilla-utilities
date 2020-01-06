PAGE    66,132
;
;
;
;
;
;
;
;
;  outch.asm
;
;  print a character to stdio, ttymode, does \n to \n\r
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

codedef OUTCH
datadef

cstart  OUTCH
cpublic outch
        push    BP
        mov     BL, 1           ; colour
        mov     AL, +@AB [BP]   ; character
        mov     AH, 0eh
        int     10h

        pop     BP

;        mov     DL, +@AB [BP]
;        mov     AH, 2
;        int     21h

creturn	outch
cend    OUTCH
        end

