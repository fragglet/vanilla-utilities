; Trampoline function for calling the IPX API using the redirector
; direct-call API instead of invoking the 0x7a interrupt.

.model small

ipx_regs    struc
ipxr_ax     dw    ?
ipxr_bx     dw    ?
ipxr_cx     dw    ?
ipxr_dx     dw    ?
ipxr_es     dw    ?
ipxr_si     dw    ?
ipx_regs    ends

.data
public _ipx_regs, _ipx_entrypoint
_ipx_regs       ipx_regs    ?
_ipx_entrypoint dd          0

.code

; Old-style call into IPX API by invoking interrupt 7Ah:
public _OldIPXCall
_OldIPXCall:
    push bx
    push cx
    push dx
    push es
    push si
    mov ax, _ipx_regs.ipxr_ax
    mov bx, _ipx_regs.ipxr_bx
    mov cx, _ipx_regs.ipxr_cx
    mov dx, _ipx_regs.ipxr_dx
    mov es, _ipx_regs.ipxr_es
    mov si, _ipx_regs.ipxr_si
    int 7Ah
    mov _ipx_regs.ipxr_ax, ax
    mov _ipx_regs.ipxr_bx, bx
    mov _ipx_regs.ipxr_cx, cx
    mov _ipx_regs.ipxr_dx, dx
    mov _ipx_regs.ipxr_es, es
    mov _ipx_regs.ipxr_si, si
    pop si
    pop es
    pop dx
    pop cx
    pop bx
    ret

; New-style call into IPX API by performing a far call into the IPX entrypoint
; function. The _ipx_entrypoint variable must have been initialized first.
public _NewIPXCall
_NewIPXCall:
    push bx
    push cx
    push dx
    push es
    push si
    mov ax, _ipx_regs.ipxr_ax
    mov bx, _ipx_regs.ipxr_bx
    mov cx, _ipx_regs.ipxr_cx
    mov dx, _ipx_regs.ipxr_dx
    mov es, _ipx_regs.ipxr_es
    mov si, _ipx_regs.ipxr_si
    call ds:_ipx_entrypoint
    mov _ipx_regs.ipxr_ax, ax
    mov _ipx_regs.ipxr_bx, bx
    mov _ipx_regs.ipxr_cx, cx
    mov _ipx_regs.ipxr_dx, dx
    mov _ipx_regs.ipxr_es, es
    mov _ipx_regs.ipxr_si, si
    pop si
    pop es
    pop dx
    pop cx
    pop bx
    ret

    end

