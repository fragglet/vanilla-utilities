//  Copyright 1994 Scott Coleman, American Society of Reverse Engineers

//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 1.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

// NOTE: Portions of this program were adapted from other freely available
// software, including SERSETUP and the Crynwr PLIP parallel port Internet
// Protocol driver.

#include <string.h>
#include <stdio.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/parport.h"

#define NUM_RX_BUFFERS 16
#define BUFSIZE 512

struct rx_buffer {
    uint8_t buffer[BUFSIZE];
    unsigned int len;
};

unsigned int portbase = 0x378;
unsigned irq = 7;

static void (interrupt far *oldisr) ();
static uint8_t oldmask;
unsigned int errors_wrong_checksum = 0;
unsigned int errors_packet_overwritten = 0;
unsigned int errors_wrong_start = 0;
unsigned int errors_timeout = 0;

static struct rx_buffer rx_buffers[NUM_RX_BUFFERS];
static unsigned int rx_buffer_head, rx_buffer_tail;

unsigned int bufseg = 0;
unsigned int bufofs = 0;
unsigned int recv_count = 0;

// Flags:
static int lpt2, lpt3;
static int port_flag;

extern void __stdcall PLIORecvPacket(void);

// Called by the I/O code when a complete packet has been received.
// When called, the buffer (in the rx_buffer_head'th buffer) has been
// populated, and recv_count is set to the packet length.
void __stdcall PacketReceived(void)
{
    struct rx_buffer *buf = &rx_buffers[rx_buffer_head];
    unsigned int next_head;

    if (buf->len != 0)
    {
        ++errors_packet_overwritten;
    }

    buf->len = recv_count;

    // Advance head, but we don't overflow the queue. If there are no more
    // buffers available, we will end up overwriting a packet.
    next_head = (rx_buffer_head + 1) & (NUM_RX_BUFFERS - 1);
    if (next_head != rx_buffer_tail)
    {
        rx_buffer_head = next_head;
    }
}

void interrupt far ReceiveISR(void)
{
    bufseg = FP_SEG(&rx_buffers[rx_buffer_head].buffer);
    bufofs = FP_OFF(&rx_buffers[rx_buffer_head].buffer);

    PLIORecvPacket();

    OUTPUT(0x20, 0x20);
}

// Returns zero if there is no packet waiting to be received.
unsigned int NextPacket(uint8_t *result_buf, unsigned int max_len)
{
    struct rx_buffer *buf;
    unsigned int result;

    while (rx_buffer_head != rx_buffer_tail)
    {
        buf = &rx_buffers[rx_buffer_tail];
        rx_buffer_tail = (rx_buffer_tail + 1) & (NUM_RX_BUFFERS - 1);

        // We skip over packets if they would overflow result_buf.
        if (buf->len <= max_len)
        {
            memcpy(result_buf, &buf->buffer, buf->len);
            result = buf->len;
            buf->len = 0;

            return result;
        }
    }

    return 0;
}

void InitISR(void)
{
    uint8_t mask;

    // install new interrupt hander for the printer port
    oldisr = _dos_getvect(irq + 8);
    _dos_setvect(irq + 8, ReceiveISR);

    // enable interrupts from the printer port
    OUTPUT(portbase + 2, INPUT(portbase + 2) | 0x10);
    oldmask = INPUT(0x21);
    mask = oldmask & ~(1 << irq);       // enable IRQ in ICR
    OUTPUT(0x21, mask);

    OUTPUT(0x20, 0x20);
}

void ParallelRegisterFlags(void)
{
    BoolFlag("-lpt2", &lpt2, "(or -lpt3) use LPTx instead of LPT1");
    BoolFlag("-lpt3", &lpt3, NULL);
    IntFlag("-port", &port_flag, "port number", NULL);
    IntFlag("-irq", &irq, "irq", NULL);
}

void GetPort(void)
{
    if (port_flag != 0)
    {
        portbase = port_flag;
    }
    else if (lpt2)
    {
        portbase = 0x278;
        SetLogDistinguisher("LPT2");
    }
    else if (lpt3)
    {
        portbase = 0x3bc;
        SetLogDistinguisher("LPT3");
    }
    else
    {
        SetLogDistinguisher("LPT1");
    }

    LogMessage("Using parallel port with base address 0x%x and IRQ %u.",
               portbase, irq);
}

void InitPort(void)
{
    // find the irq and i/o address of the port
    GetPort();

    InitISR();
}

void ShutdownPort(void)
{
    OUTPUT(0x21, oldmask);    // disable IRQs
    _dos_setvect(irq + 8, oldisr);   // restore vector
}

