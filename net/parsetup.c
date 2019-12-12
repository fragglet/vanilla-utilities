// parsetup.c

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

#include <bios.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <mem.h>
#include <dos.h>
#include <time.h>
#include <string.h>
#include "lib/inttypes.h"

#include "lib/flag.h"
#include "lib/log.h"
#include "net/parsetup.h"
#include "net/doomnet.h"

unsigned newpkt = 0;

extern void recv(void);
extern void send_pkt(void);
extern uint8_t pktbuf[];
extern unsigned recv_count;
extern unsigned errcnt;

/*
=================
=
= Error
=
= For abnormal program terminations
=
=================
*/

void Error(char *error, ...)
{
    va_list argptr;

    ShutdownPort();

    if (error)
    {
        va_start(argptr, error);
        vprintf(error, argptr);
        va_end(argptr);
        printf("\n");
        exit(1);
    }

    exit(0);
}

/*
================
=
= ReadPacket
=
================
*/

#define MAXPACKET	512

boolean ReadPacket(void)
{

    if (newpkt)
    {
        newpkt = 0;
        return true;            // true - got a good packet
    }

    return (false);             // false - no packet available

}

/*
=============
=
= WritePacket
=
=============
*/

int WritePacket(uint8_t *data, unsigned len)
{

    _CX = len;
    _DS = FP_SEG(data);
    _SI = FP_OFF(data);
    _ES = _DI = 0;
    send_pkt();

    asm jnc sendok;

    return (_DH);

 sendok:
    return (0);

}

/*
=============
=
= NetISR
=
=============
*/

void interrupt NetISR(void)
{
    if (doomcom.command == CMD_SEND)
    {
        WritePacket((char *)&doomcom.data, doomcom.datalength);
    }
    else if (doomcom.command == CMD_GET)
    {
        if (ReadPacket() && recv_count <= sizeof(doomcom.data))
        {
            doomcom.remotenode = 1;
            doomcom.datalength = recv_count;
            memcpy(&doomcom.data, &pktbuf, recv_count);
        }
        else
            doomcom.remotenode = -1;
    }
}

/*
=================
=
= Connect
=
= Figures out who is player 0 and 1
=================
*/

void Connect(void)
{
    struct time time;
    int oldsec;
    int localstage, remotestage;
    char str[20];

    LogMessage("Attempting to connect across parallel link.");

    //
    // wait for a good packet
    //

    oldsec = -1;
    localstage = remotestage = 0;

    do
    {
        while (bioskey(1))
        {
            if ((bioskey(0) & 0xff) == 27)
                Error("\n\nNetwork game synchronization aborted.");
        }

        while (ReadPacket())
        {
            pktbuf[recv_count] = 0;
            // LogMessage("Read: %s", pktbuf);
            if (recv_count != 7)
                goto badpacket;
            if (strncmp(pktbuf, "PLAY", 4))
                goto badpacket;
            remotestage = pktbuf[6] - '0';
            localstage = remotestage + 1;
            if (pktbuf[4] == '0' + doomcom.consoleplayer)
            {
                doomcom.consoleplayer ^= 1;
                localstage = remotestage = 0;
            }
            oldsec = -1;
        }
 badpacket:

        gettime(&time);
        if (time.ti_sec != oldsec)
        {
            oldsec = time.ti_sec;
            sprintf(str, "PLAY%i_%i", doomcom.consoleplayer, localstage);
            WritePacket(str, strlen(str));
            // LogMessage("Wrote: %s", str);
        }

    } while (remotestage < 1);

    //
    // flush out any extras
    //
    while (ReadPacket())
        ;
}

/*
=================
=
= main
=
=================
*/

void main(int argc, char *argv[])
{
    char **args;
    int force_player1;
    time_t t;

    SetHelpText("Doom parallel port network device driver",
                "%s doom2.exe -warp 15 -skill 3");
    BoolFlag("-player1", &force_player1, "force this side to be player 1");
    ParallelRegisterFlags();
    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);

    //
    // set network characteristics
    //
    doomcom.ticdup = 1;
    doomcom.extratics = 0;
    doomcom.consoleplayer = 0;
    doomcom.numnodes = 2;
    doomcom.numplayers = 2;
    doomcom.drone = 0;

    t = time(&t);

    //
    // allow override of automatic player ordering to allow a slower computer
    // to be set as player 1 always
    //
    if (force_player1)
    {
        doomcom.consoleplayer = 1;
    }

    //
    // establish communications
    //
    InitPort();

    Connect();

    //
    // launch DOOM
    //
    LaunchDOOM(args);

    Error(NULL);

}
