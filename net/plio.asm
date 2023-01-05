ver equ     0
;History:562,1

;  Copyright, 1988-1992, Russell Nelson, Crynwr Software

;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, version 1.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program; if not, write to the Free Software
;   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

.model small

.data
_plio_write_seg        dw        0
_plio_write_off        dw        0
_plio_write_len        dw        0
PUBLIC        _plio_write_seg, _plio_write_off, _plio_write_len

.code

; The following says how to transfer a sequence of bytes.  The bytes
; are structured as [ count-low, count-high, bytes, bytes, bytes, checksum ].
; 
; Send		Recv
; 8->
; [ repeat the following
; 		<-1
; 10h+low_nib->
; 		<-0
; high_nib->
; until all bytes have been transferred ]
; 		<-0

DATA		equ	0
REQUEST_IRQ	equ	08h
STATUS		equ	1
CONTROL		equ	2

MTU		equ	512

CANT_SEND	equ	12		;the packet couldn't be sent (usually
					;hardware error)
NO_SPACE	equ	9		;operation failed because of
					;insufficient space

extrn   _PacketReceived: near

extrn   _bufseg:word
extrn   _bufofs:word
extrn   _recv_count:word
extrn   _portbase:word
extrn   _errors_wrong_checksum:word
extrn   _errors_packet_overwritten:word
extrn   _errors_wrong_start:word
extrn   _errors_timeout:word

send_nib_count	db	?
recv_byte_count	db	?

;put into the public domain by Russell Nelson, nelson@crynwr.com

;we read the timer chip's counter zero.  It runs freely, counting down
;from 65535 to zero.  We sample the count coming in and subract the previous
;count.  Then we double it and add it to our timeout_counter.  When it overflows,
;then we've waited a tick of 27.5 ms.

timeout		dw	?		;number of ticks to wait.
timeout_counter	dw	?		;old counter zero value.
timeout_value	dw	?

set_timeout:
;enter with ax = number of ticks (36.4 ticks per second).
	inc	ax			;the first times out immediately.
	mov	cs:timeout,ax
	mov	cs:timeout_counter,0
	call	latch_timer
	mov	cs:timeout_value,ax
	ret

latch_timer:
	mov	al,0			;latch counter zero.
	out	43h,al
	in	al,40h			;read counter zero.
	mov	ah,al
	in	al,40h
	xchg	ah,al
	ret


do_timeout:
;call at *least* every 27.5ms when checking for timeout.  Returns nz
;if we haven't timed out yet.
	call	latch_timer
	xchg	ax,cs:timeout_value
	sub	ax,cs:timeout_value
	shl	ax,1			;keep timeout in increments of 27.5 ms.
	add	cs:timeout_counter,ax	;has the counter overflowed yet?
	jnc	do_timeout_1		;no.
	dec	cs:timeout		;Did we hit the timeout value yet?
	ret
do_timeout_1:
	or	sp,sp			;ensure nz.
        ret


send_pkt:
;enter with es:di->upcall routine, (0:0) if no upcall is desired.
;  (only if the high-performance bit is set in driver_function)
;enter with ds:si -> packet, cx = packet length.
;if we're a high-performance driver, es:di -> upcall.
;exit with nc if ok, or else cy if error, dh set to error number.
	assume	ds:nothing

	cmp	cx,MTU			; Is this packet too large?
	ja	send_pkt_toobig

;cause an interrupt on the other end.
        mov     dx, _portbase
	mov	al,REQUEST_IRQ
	out	dx,al

;wait for the other end to ack the interrupt.
	mov	ax,18
	call	set_timeout
        mov     dx, _portbase
        inc     dx
send_pkt_1:
	in	al,dx
	test	al,1 shl 3		;wait for them to output 1.
	jne	send_pkt_2
	call	do_timeout
	jne	send_pkt_1
	jmp	short send_pkt_4	;if it times out, they're not listening.
send_pkt_2:

	mov	send_nib_count,0
        mov     dx, _portbase
	mov	al,cl			;send the count.
	call	send_byte
	jc	send_pkt_4		;it timed out.
	mov	al,ch
	call	send_byte
	jc	send_pkt_4		;it timed out.
	xor	bl,bl
send_pkt_3:
	lodsb				;send the data bytes.
	add	bl,al
	call	send_byte
	jc	send_pkt_4		;it timed out.
	loop	send_pkt_3

	mov	al,bl			;send the checksum.
        ; mov     hexout_color,30h        ;aqua
        ; call    hexout_more
	call	send_byte
	jc	send_pkt_4		;it timed out.

	mov	al,0			;go back to quiescent state.
	out	dx,al
	clc
	ret

send_pkt_toobig:
	mov	dh,NO_SPACE
	stc
	ret

send_pkt_4:
        mov     dx, _portbase
	mov	al,send_nib_count
        ; mov     hexout_color,20h        ;green
        ; call    hexout_more
	xor	al,al			;clear the data.
	out	dx,al
	mov	dh,CANT_SEND
	stc
	ret
;
; It's important to ensure that the most recent setport is a setport DATA.
;
send_byte:
;enter with al = byte to send.
;exit with cy if it timed out.
	push	ax
	or	al,10h			;set the clock bit.
	call	send_nibble
	pop	ax
	jc	send_nibble_2
	shr	al,1
	shr	al,1
	shr	al,1
	shr	al,1			;clock bit is cleared by shr.
send_nibble:
;enter with setport DATA, al[3-0] = nibble to output.
;exit with dx set to DATA.
	out	dx,al
	and	al,10h			;get the bit we're waiting for to come back.
	shl	al,1			;put it in the right position.
	shl	al,1
	shl	al,1
	mov	ah,al
        ; mov     hexout_color,60h        ;orange
        ; call    hexout_more

        mov     dx, _portbase
        inc     dx
	push	cx
	xor	cx,cx
send_nibble_1:
	in	al,dx			;keep getting the status until
	xor	al,87h
	and	al,80h
	cmp	al,ah			;  we get the status we're looking for.
	loopne	send_nibble_1
	pop	cx
	jne	send_nibble_2

        ; mov     hexout_color,50h        ;purple
        ; call    hexout_more

	inc	send_nib_count
        mov     dx, _portbase                ;leave with setport DATA.
	clc
	ret
send_nibble_2:
	stc
	ret


recv_char	db	'0'

        public  _PLIORecvPacket
_PLIORecvPacket:
;called from the recv isr.  All registers have been saved, and ds=cs.
;Upon exit, the interrupt will be acknowledged.

        mov     dx, _portbase
        inc     dx                      ; see if we've gotten a real interrupt.
	in	al,dx
        and     al, 11111000b           ; mask off the shit
        cmp     al,0c0h                 ; it must be 0c0h, otherwise spurious.
	je	recv_real
	inc     _errors_wrong_start
        jmp     recv_free
recv_real:

        ; mov     al,recv_char
        ; inc     recv_char
        ; and     recv_char,'7'
        ; to_scrn 24,79,al

        mov     ax, _bufseg
        mov     es, ax
        mov     di, _bufofs

        mov     dx, _portbase
	mov	al,1			;say that we're ready.
	out	dx,al

	mov	recv_byte_count,0

        mov     dx, _portbase
        inc     dx
	call	recv_byte		;get the count.
        jc      recv_timeout            ;it timed out.
	mov	cl,al
	call	recv_byte
        jc      recv_timeout            ;it timed out.
	mov	ch,al
	xor	bl,bl
        mov     _recv_count,cx
recv_1:
	call	recv_byte		;get a data byte.
        jc      recv_timeout            ;it timed out.
	add	bl,al
	stosb
	loop	recv_1

	call	recv_byte		;get the checksum.
        jc      recv_timeout            ;it timed out.
	cmp	al,bl			;checksum okay?
        jne     recv_wrong_checksum     ;no.

	call    _PacketReceived         ; packet received!

	jmp	short recv_free

recv_wrong_checksum:
	inc     _errors_wrong_checksum
	jmp     short recv_free

recv_timeout:
	inc     _errors_timeout
	jmp     short recv_free

recv_free:
;wait for the other end to reset to zero.
	mov	ax,10			;1/9th of a second.
	call	set_timeout
        mov     dx, _portbase
        inc     dx
recv_pkt_1:
	in	al,dx
        and     al, 11111000b
        cmp     al,80h                  ;wait for them to output 0.
	je	recv_4

        ; mov     hexout_color,20h        ;green
        ; call    hexout_more

	call	do_timeout
	jne	recv_pkt_1

	mov	al,recv_byte_count
        ; mov     hexout_color,20h        ;green
        ; call    hexout_more
recv_4:
        mov     dx, _portbase
	xor	al,al
	out	dx,al
	ret

        mov     dx, _portbase
        inc     dx
                                        ;this code doesn't get executed,
					;but it sets up for call to recv_byte.

recv_byte:
;called with setport STATUS.
;exit with nc, al = byte, or cy if it timed out.

	push	cx
	mov	cx,65535
recv_low_nibble:
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
        in      al,61h

	in	al,dx			;get the next data value.
	test	al,80h			;wait for handshake low (transmitted hi).
	loopne	recv_low_nibble
	pop	cx
	jne	recv_byte_1

	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,dx			;reread to make sure input has settled

	shr	al,1			;put our bits into position.
	shr	al,1
	shr	al,1
	mov	ah,al
	and	ah,0fh

	mov	al,10h			;send our handshake back.
        mov     dx, _portbase
	out	dx,al
        mov     dx, _portbase
        inc     dx

	push	cx
	mov	cx,65535
recv_high_nibble:
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
        in      al,61h

	in	al,dx			;get the next data value.
	test	al,80h			;check for handshake high (transmitted low).
	loope	recv_high_nibble
	pop	cx
	je	recv_byte_1

	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
	in	al,61h
        in      al,61h
	in	al,dx			;reread to make sure input has settled

	shl	al,1			;put our bits into position.
	and	al,0f0h
	or	ah,al

	mov	al,0			;send our handshake back.
        mov     dx, _portbase
	out	dx,al
        mov     dx, _portbase
        inc     dx

	inc	recv_byte_count

	mov	al,ah
        ; mov     hexout_color,40h        ;red
        ; call    hexout_more
	clc
	ret
recv_byte_1:
	stc
	ret


        public  timer_isr
timer_isr:
;if the first instruction is an iret, then the timer is not hooked
	iret

;any code after this will not be kept after initialization. Buffers
;used by the program, if any, are allocated from the memory between
;end_resident and end_free_mem.
	public end_resident,end_free_mem
end_resident	label	byte
	db	MTU dup(?)
end_free_mem	label	byte


public _PLIOWritePacket
_PLIOWritePacket:
	mov cx, _plio_write_len
	mov ds, _plio_write_seg
	mov si, _plio_write_off
	xor di, di
	mov es, di
	call send_pkt

	jnc sendok
	xor ah, ah
	mov al, dh
	ret

sendok:
	xor ax, ax
	ret

	end
