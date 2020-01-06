;
;
;  Usage :
;           void far *inchksum()
;
;  Internet compatible 1's complement checksum
;
;  (c) 1990 Erick Engelke
;
;  version
;
;    0.1    17 Dec -1990   E. P. Engelke
;
;
        include masmdefs.hsm
        include model.hsm

codedef INCHKSUM
datadef

cstart  INCHKSUM
cpublic inchksum
        ; Compute 1's-complement sum of data buffer
        ;
        ; unsigned lcsum( usigned far *buf, unsigned cnt)
	push	DS
        lds     SI, +@AB + 0 [BP]
	mov     CX, +@AB + 4 [BP]       ; cx = cnt

        mov     BL, CL

	shr     CX, 1			; group into words
	xor	DX, DX			; set checksum to 0
        cld

        jcxz    remain
        clc
deloop: lodsw
	adc	DX, AX
	loop	deloop

        adc     DX, 0                   ; only two necessary
        adc     DX, 0

remain: and     BL, 1
        jz      done

        xor     AH, AH
        lodsb
        add     DX, AX
        adc     DX, 0
        adc     DX, 0

done:   mov	AX,DX		; result into ax
        or      AX,AX
ok:     pop     DS
creturn inchksum
cend    INCHKSUM
        end
