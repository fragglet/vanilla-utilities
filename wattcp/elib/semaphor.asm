PAGE    66,132
;
;
;
;
;
;
;
;
;  semaphor
;
;  perform semaphor stuff
;
;  (c) 1990 Erick Engelke
;
;
;
	include masmdefs.hsm
	include	model.hsm

codedef semaphor
datadef

;*************************************************************************
;  USAGE:  int sem_up( UINT far * p)
;	   1 on failure
;	   0 on success
;*************************************************************************
cstart  semaphor

cpublic	sem_up
	mov	AX, 1
	les	DI, +@AB [BP]		; get pointer
	xchg	AX, word ptr ES:[DI]	; perform swap
creturn sem_up

cend    semaphor
        end
