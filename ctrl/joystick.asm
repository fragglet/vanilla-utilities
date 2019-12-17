;
; Copyright(C) 1993-1996 Id Software, Inc.
; Copyright(C) 1993-2008 Raven Software
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation; either version 2
; of the License, or (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; Joystick routines, from i_ibm_a.asm in the Heretic source
.model  small

.data

_joystickx        dw        0
_joysticky        dw        0
saved_si          dw        0
saved_di          dw        0
saved_bp          dw        0
PUBLIC        _joystickx, _joysticky

.code

public _ReadJoystick
_ReadJoystick:
        pushf                      ; state of interrupt flag
        cli

        mov    dx, 0201h
        in     al, dx
        out    dx, al              ; Clear the resistors

        mov    ah, 1               ; Get masks into registers
        mov    ch, 2

        mov    saved_si, si
        mov    saved_di, di
        mov    saved_bp, bp
        xor    si, si              ; Clear count registers
        xor    di, di
        xor    bx, bx              ; Clear high byte of bx for later

        mov    bp, 10000           ; joystick is disconnected if value is this big

jloop:
        in     al, dx              ; Get bits indicating whether all are finished

        dec    bp                  ; Check bounding register
        jz     bad                 ; We have a silly value - abort

        mov    bl, al              ; Duplicate the bits
        and    bl, ah              ;
        add    si, bx              ; si += al & 0x01
        mov    cl, bl

        mov    bl, al
        and    bl, ch
        add    di, bx              ; di += al & 0x02

        add    cl, bl
        jnz    jloop               ; If both bits were 0, drop out

done:
        mov    _joystickx, si
        shr    di, 1               ; di is shifted left one bit
        mov    _joysticky, di

        popf                       ; restore interrupt flag
        mov    si, saved_si        ; restore saved registers
        mov    di, saved_di
        mov    bp, saved_bp
        mov    ax, 1               ; read was ok
        ret

bad:
        popf                       ; restore interrupt flag
        mov    si, saved_si        ; restore saved registers
        mov    di, saved_di
        mov    bp, saved_bp
        xor    ax, ax              ; read was bad
        ret

ENDP


END

