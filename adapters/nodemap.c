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

// Mini protocol for discovering node<->player mapping.
// Doom network node numbers have no relation to player numbers,
// and Doom in fact does its own discovery in d_net.c to discover
// which node is which player. However, the ROTTCOM and COMMIT APIs
// are just designed around player numbers. To be able to implement
// these interfaces we therefore need to discover players.

#include <string.h>
#include <time.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/log.h"
#include "net/doomnet.h"

#include "adapters/nodemap.h"

#define MAGIC_STRING "V~UTiLS!"

struct discover_packet
{
    char magic_string[8];
    uint8_t is_reply;
    uint8_t player;
};

int nodetoplayer[MAXNETNODES];
int playertonode[MAXPLAYERS];

static int HasMagicString(struct discover_packet far *pkt)
{
    return far_memcmp(pkt->magic_string, MAGIC_STRING,
                      sizeof(pkt->magic_string)) == 0;
}

static void SendDiscover(doomcom_t far *doomcom, int node, int is_reply)
{
    struct discover_packet far *pkt = (void far *) doomcom->data;

    far_memmove(pkt->magic_string, MAGIC_STRING, sizeof(pkt->magic_string));
    pkt->player = doomcom->consoleplayer;
    pkt->is_reply = is_reply;

    doomcom->remotenode = node;
    doomcom->datalength = sizeof(struct discover_packet);
    NetSendPacket(doomcom);
}

static void SendDiscoverToAll(doomcom_t far *doomcom)
{
    int i;

    for (i = 1; i < doomcom->numnodes; ++i)
    {
        SendDiscover(doomcom, i, 0);
    }
}

// Invoked by higher-level code after DiscoverPlayers() has finished, to
// reply to discover packets from other nodes still stuck in their own
// DiscoverPlayers() loop. Returns 1 if the packet should be ignored.
int CheckLateDiscover(doomcom_t far *doomcom)
{
    struct discover_packet far *pkt = (void far *) doomcom->data;

    // We may receive a late discover packet from another node still stuck
    // in the DiscoverPlayers() loop. If we do, send a reply.
    if (doomcom->datalength == sizeof(struct discover_packet)
     && HasMagicString(pkt) && !pkt->is_reply)
    {
        SendDiscover(doomcom, doomcom->remotenode, 1);
        return 1;
    }

    return 0;
}

void DiscoverPlayers(doomcom_t far *doomcom)
{
    struct discover_packet far *pkt = (void far *) doomcom->data;
    clock_t last_send = 0, now;
    uint32_t got_nodes = 0;

    LogMessage("Discovering players.");

    nodetoplayer[0] = doomcom->consoleplayer;
    playertonode[doomcom->consoleplayer] = 0;
    got_nodes = 1 << 0;

    while (got_nodes != (1 << doomcom->numnodes) - 1)
    {
        CheckAbort("Player discovery");
        now = clock();
        if (now - last_send > CLOCKS_PER_SEC)
        {
            SendDiscoverToAll(doomcom);
            last_send = now;
        }
        if (!NetGetPacket(doomcom))
        {
            continue;
        }

        if (doomcom->datalength == sizeof(struct discover_packet)
         && HasMagicString(pkt) && pkt->player < MAXPLAYERS)
        {
            nodetoplayer[doomcom->remotenode] = pkt->player;
            playertonode[pkt->player] = doomcom->remotenode;
            got_nodes |= 1 << doomcom->remotenode;
        }
    }

    SendDiscoverToAll(doomcom);
}

