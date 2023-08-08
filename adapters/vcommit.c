//
// Copyright(C) 2019-2023 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//

// Adapter that converts a Doom network driver into a COMMIT driver
// used by various 3D Realms games.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <process.h>
#include <assert.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/ints.h"
#include "lib/log.h"
#include "net/doomnet.h"

#include "adapters/commit.h"
#include "adapters/fragment.h"
#include "adapters/nodemap.h"

static struct interrupt_hook net_interrupt;
static doomcom_t far *inner_driver;
static gamecom_t gamecom;

static void ExecuteCommand(void)
{
    struct reassembled_packet *pkt;
    int i;

    switch (gamecom.command)
    {
        case COMMIT_CMD_SEND:
            FragmentSendPacket(playertonode[gamecom.remotenode - 1],
                               gamecom.data, gamecom.datalength);
            break;

        case COMMIT_CMD_GET:
            pkt = FragmentGetPacket();
            if (pkt == NULL)
            {
                gamecom.remotenode = -1;
                return;
            }
            gamecom.remotenode = nodetoplayer[pkt->remotenode] + 1;
            gamecom.datalength = pkt->datalength;
            far_memcpy(gamecom.data, pkt->data, pkt->datalength);
            break;

        case COMMIT_CMD_SENDTOALL:
            for (i = 0; i < inner_driver->numnodes; ++i)
            {
                FragmentSendPacket(i, gamecom.data, gamecom.datalength);
            }
            break;

        case COMMIT_CMD_SENDTOALLOTHERS:
            for (i = 1; i < inner_driver->numnodes; ++i)
            {
                FragmentSendPacket(i, gamecom.data, gamecom.datalength);
            }
            break;
    }
}

static void interrupt far NetISR(void)
{
    SWITCH_ISR_STACK;
    ExecuteCommand();
    RESTORE_ISR_STACK;
}

static void SetDriver(long l)
{
    assert(inner_driver == NULL);
    inner_driver = NetGetHandle(l);
}

int main(int argc, char *argv[])
{
    char addrstring[16];
    long flataddr;
    char **args;

    SetHelpText("Doom to COMMIT network adapter",
                "ipxsetup -nodes 3 %s duke3d.exe");

    APIPointerFlag("-net", SetDriver);
    args = ParseCommandLine(argc, argv);
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    assert(inner_driver != NULL);
    InitFragmentReassembly(inner_driver);
    DiscoverPlayers(inner_driver);

    gamecom.consoleplayer = inner_driver->consoleplayer + 1;
    gamecom.numplayers = inner_driver->numplayers;
    gamecom.gametype = COMMIT_GAME_NETWORK;

    // Prepare to launch game
    if (!FindAndHookInterrupt(&net_interrupt, NetISR))
    {
        Error("Warning: no free interrupt handlers found. You can specify"
              "a vector with the -vector 0x<num> parameter.");
    }

    gamecom.intnum = net_interrupt.interrupt_num;

    // Add -net &gamecom
    flataddr = (long) FP_SEG(&gamecom) * 16 + FP_OFF(&gamecom);
    sprintf(addrstring, "%lu", flataddr);
    args = AppendArgs(args, "-net", addrstring, NULL);

    spawnv(P_WAIT, args[0], args);

    RestoreInterrupt(&net_interrupt);

    return 0;
}

