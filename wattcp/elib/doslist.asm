;
;
;  Usage :
;           void far *doslist()
;
;  (c) 1990 Erick Engelke
;
;  version
;
;    0.1    7-Nov -1990   E. P. Engelke
;
;
        include masmdefs.hsm
        include model.hsm

codedef DOSLIST
datadef

cstart  DOSLIST
cpublic doslist
        push    ES
        mov     AH, 52h         ; get dos list
        int     21h
        mov     AX, BX
        mov     DX, ES
        pop     ES
creturn doslist
cend    DOSLIST
        end

