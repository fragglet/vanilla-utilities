// port.c

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

#include <dos.h>
#include <conio.h>
#include <string.h>
#include <stdio.h>
#include "net/doomnet.h"
#include "net/parsetup.h"

typedef unsigned char byte;

unsigned int portbase = 0x378;
unsigned irq = 7;

#define BUFSIZE 512
static void interrupt(*oldisr) ();
static byte oldmask;
unsigned int errcnt = 0;
static unsigned icnt = 0;

byte pktbuf[BUFSIZE];

unsigned int bufseg = 0;
unsigned int bufofs = 0;
unsigned int recv_count = 0;

// Flags:
static int lpt2, lpt3;
static int port_flag;

void CountInErr(void)
{

    errcnt++;
}

extern void recv(void);

void interrupt ReceiveISR(void)
{

    icnt++;
    recv();

    outportb(0x20, 0x20);

}

void InitISR(void)
{
    byte mask;

    // install new interrupt hander for the printer port
    oldisr = getvect(irq + 8);
    setvect(irq + 8, ReceiveISR);

    // enable interrupts from the printer port
    outportb(portbase + 2, inportb(portbase + 2) | 0x10);
    oldmask = inportb(0x21);
    mask = oldmask & ~(1 << irq);       // enable IRQ in ICR
    outportb(0x21, mask);

    outportb(0x20, 0x20);

}

void ParallelRegisterFlags(void)
{
    BoolFlag("-lpt2", &lpt2, "(and -lpt3) use LPTx instead of LPT1");
    BoolFlag("-lpt3", &lpt3, NULL);
    IntFlag("-port", &port_flag, "port number",
            "use I/O ports at given base address");
    IntFlag("-irq", &irq, "irq", "IRQ number for parallel port");
}

/*
==============
=
= GetPort
=
==============
*/

void GetPort(void)
{
    if (port_flag != 0)
    {
        portbase = port_flag;
    }
    else if (lpt2)
    {
        portbase = 0x278;
    }
    else if (lpt3)
    {
        portbase = 0x3bc;
    }

    printf("Using parallel printer port with base address 0x%x and IRQ %u\n",
           portbase, irq);
}

/*
===============
=
= InitPort
=
===============
*/

void InitPort(void)
{

    //
    // find the irq and io address of the port
    //
    GetPort();

    bufseg = FP_SEG(pktbuf);
    bufofs = FP_OFF(pktbuf);

    InitISR();

}

/*
=============
=
= ShutdownPort
=
=============
*/

void ShutdownPort(void)
{

    outportb(0x21, oldmask);    // disable IRQs
    setvect(irq + 8, oldisr);   // restore vector

}

//==========================================================================
