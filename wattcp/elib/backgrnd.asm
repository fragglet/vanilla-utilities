PAGE	66,132
;
;
;
;
;
;
;
;
;  backgrnd.asm
;
;  usage:
;	    extern init_bkg( int (*routine)(), char *stack, int stacklen)
;	    extern swap_bkg()
;	    extern kill_bkg()
;
;  (c) 1990 Erick Engelke
;
;
	include masmdefs.hsm
	include	model.hsm

codedef BACKGRND

SETVEC	equ	25h		; function to set interrupt vector
GETVEC	equ	35h
TICKER	equ	1ch
DOS	equ	21h


datadef

cstart  BACKGRND

oldint	dq 0

inside	db 0
stkbase dw 0		; used to check stack depth
stkptr	dw 0
stkseg	dw 0
retstk	dw 0,0
bkgptr	dw 0, 0

swapstk macro
	cli
	mov	AX, SS
	xchg	CS:stkseg, AX
	mov	SS, AX
	xchg	CS:stkptr, SP
	sti
	endm

	; note, we do not push AX!!!!!!, assembly routine must
push_em	macro
	push	BX
	push	CX
	push	DX
	push	BP
	push	SI
	push	DI
	push	DS
	push	ES
	endm

pop_em	macro
	pop	ES
	pop	DS
	pop	DI
	pop	SI
	pop	BP
	pop	DX
	pop	CX
	pop	BX
	pop	AX
	endm

	; bkg_int - the interrupt function called by the timer tick
bkg_int proc far
	; first chain
	pushf
	call	dword ptr CS:[oldint]

	; establish if we are active
	push	AX
	mov	AL, 1
	xchg	CS:inside, AL
	or	AL, AL
	jz	@1		; not busy, go ahead

	; must be already active
	pop	AX
	iret

@1:	push_em

	; switch over to the new stack
	swapstk

	pop_em

IF PTR_L
	retf
ELSE
	retn
ENDIF
bkg_int endp

	; use cproc, does not use BP
cproc	swap_bkg
	push	AX
	push_em   		; save them on
	swapstk
	pop_em

	xor	AL, AL		; clear flag
	mov	CS:inside, AL

	iret
creturn	swap_bkg

IF FUNC_L
OFSSTK	equ	4
OFSLEN	equ	OFSSTK + 4
ELSE
OFSSTK	equ	2
OFSLEN	equ	OFSSTK + 2
ENDIF

cpublic init_bkg	; int (*routine)(), char *stack, int stacklen
	; get pointer to routine
	mov	DX, +@AB [BP]
	mov	CS:bkgptr, DX
IF FUNC_L
	mov     CX, +@AB + 2 [BP]
ELSE
	mov	CX, CS
ENDIF
	mov	CS:bkgptr+2, CX

	; get stack area
	mov	AX, +@AB + OFSSTK [BP]
	mov	CS:stkbase, AX
IF PTR_L
	mov	BX, +@AB + OFSSTK + 2 [BP]
	mov	CS:stkseg, BX
ELSE
	mov	CS:stkseg, DS
ENDIF
	; get length
	add	AX, +@AB + OFSLEN [BP]
	sub	AX, 2
	mov	CS:stkptr, AX

	; get the stack area and set up the first call
	swapstk
	push	CX
	push	DX
	push	AX
	push_em

	; done with that stack
	swapstk

	; set up the interrupt
	push	ES
	push	DS
	mov 	AH, GETVEC
	mov	AL, TICKER
	int	DOS
	mov	word ptr CS:oldint, BX
	mov	word ptr CS:oldint+2, ES

	push	CS
	pop	DS
	mov	DX, offset bkg_int
	mov	AH, SETVEC
	mov	AL, TICKER
	int	DOS

	mov	DS, DX

	pop	DS
	pop	ES
	xor 	AX, AX
creturn init_bkg

cpublic	kill_bkg
	push	DS
	xor	AX, AX
	mov	DS, AX
	mov	BX, TICKER * 4
	cli
	mov	CX, word ptr CS:oldint
	mov	DX, word ptr CS:oldint+2
	mov	DS:[BX], CX
	mov	DS:[BX+2], DX
	sti
	pop	DS
creturn	kill_bkg
cend    BACKGRND

        end


