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
#include "lib/ints.h"
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
static int irq = 7;

static struct irq_hook parport_interrupt;

unsigned int errors_wrong_checksum = 0;
unsigned int errors_packet_overwritten = 0;
unsigned int errors_timeout = 0;

static struct rx_buffer rx_buffers[NUM_RX_BUFFERS];
static unsigned int rx_buffer_head, rx_buffer_tail;

unsigned int bufseg = 0;
unsigned int bufofs = 0;
unsigned int recv_count = 0;

// Flags:
static int lpt2, lpt3;
static int irq_flag = 0, port_flag;

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

    EndOfIRQ(&parport_interrupt);
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
    HookIRQ(&parport_interrupt, ReceiveISR, irq);

    // enable interrupts from the printer port
    OUTPUT(portbase + 2, INPUT(portbase + 2) | 0x10);
}

void ParallelRegisterFlags(void)
{
    BoolFlag("-lpt2", &lpt2, "(or -lpt3) use LPTx instead of LPT1");
    BoolFlag("-lpt3", &lpt3, NULL);
    IntFlag("-port", &port_flag, "port number", NULL);
    IntFlag("-irq", &irq_flag, "irq", NULL);
}

void GetPort(void)
{
    if (port_flag != 0)
    {
        portbase = port_flag;
    }
    else if (lpt2)
    {
        SetLogDistinguisher("LPT2");
        if (irq_flag == 0)
        {
            LogMessage("Assuming IRQ 5 for LPT2; you might want to double "
                       "check this. Use -irq to specify the right IRQ if "
                       "this is wrong and you get problems.");
            irq = 5;
        }
        portbase = 0x278;
    }
    else if (lpt3)
    {
        SetLogDistinguisher("LPT3");
        if (irq_flag == 0)
        {
            Error("Cowardly refusing to guess IRQ for LPT3 because it's too "
                  "unusual. Please use the -irq flag to specify the IRQ "
                  "number for this port.");
        }
        portbase = 0x3bc;
    }
    else
    {
        SetLogDistinguisher("LPT1");
        irq = 7;
    }

    if (irq_flag != 0)
    {
        irq = irq_flag;
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
    // TODO: disable interrupts from the printer port
    RestoreIRQ(&parport_interrupt);
}

