;
;
;  Usage :
;           _keep( int status, int paragraphs )
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

codedef KEEP
datadef

cstart  KEEP
cpublic _keep
        mov     AH, 62h                 ; get psp
        int     21h

        mov     ES, BX                  ; using psp
        mov     BX, ES:[2ch]            ; get environment

        mov     AH, 49h                 ; release environment
        mov     ES, BX
        int     21h

        mov     AH, 31h                 ; make resident
        mov     AL, +@AB + 0 [BP]
        mov     DX, +@AB + 2 [BP]
        int     21h
creturn _keep
cend    KEEP
        end
