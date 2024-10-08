;
; Copyright(C) 2019-2023 Simon Howard
;
; You can redistribute and/or modify this program under the terms of the
; GNU General Public License version 2 as published by the Free Software
; Foundation, or any later version. This program is distributed WITHOUT
; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
; FITNESS FOR A PARTICULAR PURPOSE.
;

; Trampoline functions for calling low-level function calls with particular
; registers set, and then capturing the resulting register values. Used for
; calling into the IPX API.

.model small

ll_regs    struc
llr_ax     dw    ?
llr_bx     dw    ?
llr_cx     dw    ?
llr_dx     dw    ?
llr_es     dw    ?
llr_si     dw    ?
ll_regs    ends

.data
public _ll_regs, _ll_funcptr
_ll_regs       ll_regs    ?
_ll_funcptr    dd          0

.code

; Call the far function pointer set in ll_funcptr with the registers that
; are set in ll_regs.
; As used by the "new-style" IPX API.
public _LowLevelCall
_LowLevelCall:
    push bp
    push bx
    push es
    push si
    push di
    mov ax, _ll_regs.llr_ax
    mov bx, _ll_regs.llr_bx
    mov cx, _ll_regs.llr_cx
    mov dx, _ll_regs.llr_dx
    mov es, _ll_regs.llr_es
    mov si, _ll_regs.llr_si
    call ds:_ll_funcptr
    mov _ll_regs.llr_ax, ax
    mov _ll_regs.llr_bx, bx
    mov _ll_regs.llr_cx, cx
    mov _ll_regs.llr_dx, dx
    mov _ll_regs.llr_es, es
    mov _ll_regs.llr_si, si
    pop di
    pop si
    pop es
    pop bx
    pop bp
    ret

    end

