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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <mem.h>
#include <time.h>
#include <string.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"
#include "net/parport.h"
#include "net/doomnet.h"

#define MAXPACKET	512

static doomcom_t doomcom;
unsigned newpkt = 0;

extern int __stdcall PLIOWritePacket(void);
extern uint8_t pktbuf[];
extern unsigned recv_count;
extern unsigned errcnt;

int ReadPacket(void)
{
    if (newpkt)
    {
        newpkt = 0;
        return 1;            // true - got a good packet
    }

    return 0;                // false - no packet available
}

int WritePacket(uint8_t *data, unsigned len)
{
    extern int plio_write_seg, plio_write_off, plio_write_len;

    plio_write_seg = FP_SEG(data);
    plio_write_off = FP_OFF(data);
    plio_write_len = len;

    return PLIOWritePacket();
}

static void NetCallback(void)
{
    if (doomcom.command == CMD_SEND)
    {
        WritePacket((char *)doomcom.data, doomcom.datalength);
    }
    else if (doomcom.command == CMD_GET)
    {
        if (ReadPacket() && recv_count <= sizeof(doomcom.data))
        {
            doomcom.remotenode = 1;
            doomcom.datalength = recv_count;
            memcpy(doomcom.data, pktbuf, recv_count);
        }
        else
        {
            doomcom.remotenode = -1;
        }
    }
}

// Figure out who is player 0 and 1
void Connect(void)
{
    clock_t last_time = 0, now;
    int localstage, remotestage;
    char str[20];

    LogMessage("Attempting to connect across parallel link.");

    // wait for a good packet
    localstage = remotestage = 0;

    do
    {
        CheckAbort("Parallel port synchronization");

        while (ReadPacket())
        {
            pktbuf[recv_count] = 0;
            // LogMessage("Read: %s", pktbuf);
            if (recv_count != 7)
            {
                goto badpacket;
            }
            if (strncmp(pktbuf, "PLAY", 4))
            {
                goto badpacket;
            }
            remotestage = pktbuf[6] - '0';
            localstage = remotestage + 1;
            if (pktbuf[4] == '0' + doomcom.consoleplayer)
            {
                doomcom.consoleplayer ^= 1;
                localstage = remotestage = 0;
            }
            last_time = 0;
        }
 badpacket:

        now = clock();
        if (now - last_time >= CLOCKS_PER_SEC)
        {
            last_time = now;
            sprintf(str, "PLAY%i_%i", doomcom.consoleplayer, localstage);
            WritePacket(str, strlen(str));
            // LogMessage("Wrote: %s", str);
        }

    } while (remotestage < 1);

    // flush out any extras
    while (ReadPacket())
    {
    }
}

void main(int argc, char *argv[])
{
    char **args;
    int force_player1;

    SetHelpText("Doom parallel port network device driver",
                "%s doom2.exe -warp 15 -skill 3");
    BoolFlag("-player1", &force_player1, "force this side to be player 1");
    ParallelRegisterFlags();
    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    // set network characteristics
    doomcom.ticdup = 1;
    doomcom.extratics = 0;
    doomcom.consoleplayer = 0;
    doomcom.numnodes = 2;
    doomcom.numplayers = 2;
    doomcom.drone = 0;

    // allow override of automatic player ordering to allow a slower computer
    // to be set as player 1 always
    if (force_player1)
    {
        doomcom.consoleplayer = 1;
    }

    // establish communications
    InitPort();
    atexit(ShutdownPort);

    Connect();

    // launch DOOM
    NetLaunchDoom(&doomcom, args, NetCallback);
}

