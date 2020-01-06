;
;
;  Usage :
;	    -pktentry
;           _pktasminit( void far * buffers, int maxbufs, int buflen)
;
;  (c) 1991 University of Waterloo,
;           Faculty of Engineering,
;           Engineering Microcomputer Network Development Office
;
;  version
;
;    0.1    22-May-1992   E. P. Engelke
;
;
        include masmdefs.hsm
        include model.hsm

codedef ASMPKT
datadef

cstart  ASMPKT

maxbufs	dw	0
maxlen	dw	0
bufs	dw	0
bufseg	dw	0

cproc	_pktentry
	pushf

	cli		; no interruptions now

	or	AL, AL
	jnz	encue	; branch if was a 1 and must encue packet now

	; buffer request operation
	; to check our buffers we will need the same DS seg, set it now
        push    CX
	push	DS
        mov     DI, CS:bufseg
        mov     DS, DI

	; check the packet length
	cmp	CX, cs:maxlen
	jg	no_fnd		; pretend none were found

	mov	DI, CS:bufs
	mov	CX, CS:maxbufs
	mov	AL, 0ffh

srcloop:
;        test    AL, byte ptr DS:DI             ; 94.11.30 -- fix for tasm4
        test    AL, byte ptr DS:[DI]
	jz	found
	add	DI, CS:maxlen
	add	DI, 2
	loop	srcloop

no_fnd: xor	DI, DI		; for whatever error, throw away the buffer
	mov	DS, DI		; by returning 0000:0000
	sub	DI, 2

found:  push	DS
	pop	ES
	add	DI, 2
	pop	DS
	pop	CX
	popf
	retf

	; encue packet
	;
encue:	or	SI, SI
	jz	no_enqu		; not a valid pointer, cannot encue
	push	SI
	sub	SI, 2
	mov	AL, 1		; should be already, but just in case
;        mov     byte ptr DS:SI, AL             ; 94.11.30 -- fix for tasm4
        mov     byte ptr DS:[SI], AL
	pop	SI
no_enqu:
        popf
	retf
cendp	_pktentry

cpublic _pktasminit		; bufptr, maxbufs, buflen
	mov	AX, +@AB + 0 [BP]	; bufptr
	mov	BX, +@AB + 2 [BP]	; bufptr segment
	mov	CX, +@AB + 4 [BP]	; maxbufs
	mov	DX, +@AB + 6 [BP]	; buflen
	mov	CS:bufs, AX
	mov	CS:bufseg, BX
	mov	CS:maxbufs, CX
	mov	CS:maxlen, DX
creturn _pktasminit
cend    ASMPKT
        end
