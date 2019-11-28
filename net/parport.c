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
#include "doomnet.h"
#include "parsetup.h"

typedef unsigned char byte;

unsigned portbase=0x378;
unsigned irq=7;

#define BUFSIZE 512
void interrupt (*oldisr)();
byte oldmask;
unsigned errcnt=0;
unsigned icnt=0;

byte pktbuf[BUFSIZE];

unsigned bufseg=0;
unsigned bufofs=0;
unsigned recv_count=0;



void count_in_err(void)
{

    errcnt++;
}

extern void recv(void);


void interrupt recvisr(void)
{

	icnt++;
        recv();

        outportb(0x20, 0x20);

}


void initisr(void)
{
byte mask;

    // install new interrupt hander for the printer port
    oldisr = getvect(irq+8);
    setvect(irq+8, recvisr);

    // enable interrupts from the printer port
    outportb(portbase+2, inportb(portbase+2)|0x10);
    oldmask = inportb(0x21);
    mask = oldmask & ~(1<<irq);        // enable IRQ in ICR
    outportb(0x21, mask);

    outportb(0x20, 0x20);

}


/*
==============
=
= GetPort
=
==============
*/

void GetPort (void)
{
int p;

    if (CheckParm ("-lpt2"))
        portbase = 0x278;
    else if (CheckParm ("-lpt3"))
        portbase = 0x3bc;

    p = CheckParm ("-port");
    if (p)
        sscanf (_argv[p+1],"0x%x",&portbase);

    p = CheckParm("-irq");
    if (p)
        sscanf(_argv[p+1], "%u", &irq);

    printf ("Using parallel printer port with base address 0x%x and IRQ %u\n",
        portbase, irq);
}


/*
===============
=
= InitPort
=
===============
*/

void InitPort (void)
{

//
// find the irq and io address of the port
//
    GetPort();

    bufseg = FP_SEG(pktbuf);
    bufofs = FP_OFF(pktbuf);

    initisr();

}


/*
=============
=
= ShutdownPort
=
=============
*/

void ShutdownPort ( void )
{

    outportb(0x21, oldmask);    // disable IRQs
    setvect(irq+8, oldisr);       // restore vector

}


//==========================================================================


