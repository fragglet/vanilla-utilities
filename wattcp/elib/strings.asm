PAGE    66,132
;
;
;
;
;
;  net_string - find a network string from a buffer
;
;
;  (c) 1992 Erick Engelke
;
;
;
	include masmdefs.hsm
	include	model.hsm

codedef net_string
datadef

cstart  net_string

;*************************************************************************
;  USAGE:  int net_string( byte *buf, int buflen, byte *strbuf, int strbuflen )
;  ALL POINTERS ASSUMED FAR
;*************************************************************************
cpublic net_string
        push    DS
        push    ES
        cld

        lds     SI, +@AB + 0  [BP]
        mov     BX, +@AB + 4  [BP]
        les     DI, +@AB + 6  [BP]
        mov     CX, +@AB + 10 [BP]

        cmp     CX, BX          ; find the lesser, ie. max string len
        ja      .0
        mov     CX, BX

.0:     sub     CX, 2           ; must leave room for the '\0'
        mov     BX, CX          ; need later

.1:     lodsb                   ; check each character to see if
        cmp     AL, 0eh         ; it is the string terminator
        jb      .3              ; a likely suspect is investigated
.2:     stosb
        loop    .1
        jcxz    .4              ; quite done
                                ; (MS-ASM generates phase error if we use JMP
                                ; phase-error? Get a life Microsoft, figuring
                                ; out the distance is pretty darn trivial,
                                ; even DEBUG can do it!)

.3:     cmp     AL, 0ah         ; <lf>
        je      .4
        cmp     AL, 0dh         ; <cr>
        jne     .2              ; no, so continue

        ; this is still for <cr>, we erase the next character
        jcxz    .5              ; no more characters - line incomplete
        dec     CX
        jmp     .4

        ; here we handle case when we didn't find a string
.5:

.4:     xor     AL, AL
        stosb

        ; now must return length of string to remove
        mov     AX, BX
        sub     AX, CX
.6:     pop     ES
        pop     DS
creturn net_string
cend    net_string
        end
