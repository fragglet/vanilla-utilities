PAGE    66,132
;
;
;
;
;
;
;
;
;  qcmp.asm
;
;  Efficient compare operation.  Does not handle segment wraps!!!!
;  Returns 0 if compare is identical
;
;
;  Usage :
;           qcmp( soff, sseg, doff, dseg, count )
;	    unsigned int soff;		/* source offset */
;	    unsigned int sseg;		/* source segment */
;	    unsigned int doff;		/* destination offset */
;	    unsigned int dseg;		/* destination segment */
;	    unsigned int count;		/* byte count */
;
;  (c) 1991 Erick Engelke
;
;  version
;
;    0.0    31-Oct-1990   E. P. Engelke  - brief version
;
;
	include masmdefs.hsm
	include	model.hsm

codedef QCMP
datadef

cstart  QCMP
cpublic qcmp
        mov     DX, DS
        mov     BX, ES                   ;save segment registers

        lds     SI, +@AB+0[BP]
        les     DI, +@AB+4[BP]
        mov     CX, +@AB+8[BP]          ;byte count

        cld
        repe    cmpsb

        mov     AX, CX

        mov     ES, BX
        mov     DS, DX
creturn qcmp

cend    QCMP

        end


