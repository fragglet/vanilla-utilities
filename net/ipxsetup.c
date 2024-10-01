//
// Copyright(C) 1993 id Software, Inc.
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "lib/inttypes.h"

#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/ipxnet.h"

// setupdata_t is used as doomdata_t during setup
typedef struct {
    int16_t gameid;               // so multiple games can setup at once
    int16_t drone;
    int16_t nodesfound;
    int16_t nodeswanted;
    // xttl extensions:
    int16_t dupwanted;
    int16_t plnumwanted;
} setupdata_t;

doomcom_t doomcom;
static int numnetnodes;

static int force_player = -1;
static ipx_addr_t nodeaddr[MAXNETNODES];
static setupdata_t nodesetup[MAXNETNODES];

static void SendPacket(void)
{
    if (doomcom.remotenode < 1 || doomcom.remotenode >= numnetnodes)
    {
        return;
    }

    IPXSendPacket(&nodeaddr[doomcom.remotenode], doomcom.data,
                  doomcom.datalength);
}

static int NodeForAddress(ipx_addr_t *addr)
{
    int i;

    for (i = 0; i < doomcom.numnodes; i++)
    {
        if (!memcmp(addr, &nodeaddr[i], sizeof(ipx_addr_t)))
        {
            return i;
        }
    }

    return -1;
}

static void GetPacket(void)
{
    packet_t *packet;
    int i;

    doomcom.remotenode = -1;

    packet = IPXGetPacket();
    if (packet == NULL)
    {
        return;
    }

    if (packet->time == -1)
    {
        IPXReleasePacket(packet);
        return;               // setup broadcast from other game
    }

    i = NodeForAddress(&packet->ipx.Src);
    if (i != -1)
    {
        doomcom.remotenode = i;
        doomcom.datalength = ShortSwap(packet->ipx.PacketLength) - 38;
        memcpy(doomcom.data, packet->payload, doomcom.datalength);
    }

    // repost the ECB
    IPXReleasePacket(packet);
}

static void NetCallback(void)
{
    if (doomcom.command == CMD_SEND)
    {
        ipx_localtime++;
        SendPacket();
    }
    else if (doomcom.command == CMD_GET)
    {
        GetPacket();
    }
}

// Process a setup packet found in doomcom.
static void ProcessSetupPacket(packet_t *packet)
{
    ipx_addr_t *addr;
    setupdata_t *setup;
    int old_protocol;
    int n;

    // Confirm it is a setup packet.
    if (packet->time != -1)
    {
        return;
    }

    setup = (setupdata_t *) packet->payload;
    old_protocol = (ShortSwap(packet->ipx.PacketLength) - 38)
                 < sizeof(setupdata_t);

    // We support the xttl setup extensions but we still maintain compatibility
    // with the original IPXSETUP.
    if (old_protocol)
    {
        setup->dupwanted = 1;
        setup->plnumwanted = -1;
    }

    addr = &packet->ipx.Src;
    n = NodeForAddress(addr);

    // New node?
    if (n == -1)
    {
        n = doomcom.numnodes;
        ++doomcom.numnodes;

        memcpy(&nodeaddr[n], addr, sizeof(ipx_addr_t));

        LogMessage("Found a node at %02x:%02x:%02x:%02x:%02x:%02x",
                   addr->Node[0], addr->Node[1], addr->Node[2],
                   addr->Node[3], addr->Node[4], addr->Node[5]);

        if (force_player != -1 && old_protocol)
        {
            Error("Other node does not support -player.");
        }
        if (force_player != -1 && force_player == setup->plnumwanted)
        {
            Error("Other node is also using -player %d. One node must "
                  "be changed to avoid clash.",
                  setup->plnumwanted);
        }
        if (setup->dupwanted > doomnet_dup)
        {
            LogMessage("Other node is using -dup %d. Adjusting to match.",
                       setup->dupwanted);
            doomnet_dup = setup->dupwanted;
        }
    }

    // update setup info
    memcpy(&nodesetup[n], setup, sizeof(setupdata_t));
}

static int DetermineConsolePlayer(void)
{
    int cnt, i, result;

    if (force_player != -1)
    {
        return force_player - 1;
    }

    // How many auto-determined players have addresses lower than ours?
    cnt = 0;
    for (i = 0; i < numnetnodes; i++)
    {
        if (!nodesetup[i].drone
         && nodesetup[i].plnumwanted == -1
         && memcmp(&nodeaddr[i], &nodeaddr[0], sizeof(nodeaddr[0])) < 0)
        {
            cnt++;
        }
    }

    result = 0;
    for (;;)
    {
        // Keep skipping forward until we land on a player number that
        // hasn't been claimed by another node.
        for (i = 0; i < numnetnodes; i++)
        {
            if (result == nodesetup[i].plnumwanted - 1)
            {
                break;
            }
        }
        if (i < numnetnodes)
        {
            ++result;
            continue;
        }

        // We go through the above loop 'cnt' number of times.
        if (cnt == 0)
        {
            return result;
        }
        ++result;
        --cnt;
    }
}

// Find all nodes for the game and work out player numbers among them
// Exits with nodesetup[0..numnodes] and nodeaddr[0..numnodes] filled in
void LookForNodes(void)
{
    ipx_addr_t localaddr;
    int i;
    clock_t now, last_time = 0;
    int total;

    if (force_player != -1
     && (force_player < 1 || force_player > numnetnodes))
    {
        Error("-player value must be in the range 1..%d", numnetnodes);
    }

    IPXGetLocalAddress(&localaddr);
    memcpy(&nodeaddr[0], &localaddr, sizeof(ipx_addr_t));

    // wait until we get [numnetnodes] packets, then start playing
    // the playernumbers are assigned by netid
    LogMessage("Attempting to find %d players on network", numnetnodes);
    LogMessage("Local address is %02x:%02x:%02x:%02x:%02x:%02x",
               nodeaddr[0].Node[0], nodeaddr[0].Node[1], nodeaddr[0].Node[2],
               nodeaddr[0].Node[3], nodeaddr[0].Node[4], nodeaddr[0].Node[5]);

    ipx_localtime = -1;             // in setup time, not game time

    // build local setup info
    nodesetup[0].nodesfound = 1;
    nodesetup[0].nodeswanted = numnetnodes;
    doomcom.numnodes = 1;

    for (;;)
    {
        CheckAbort("Network game synchronization");

        // listen to the network
        for (;;)
        {
            packet_t *packet = IPXGetPacket();
            if (packet == NULL)
            {
                break;
            }

            ProcessSetupPacket(packet);
            IPXReleasePacket(packet);
        }

        // we are done if all nodes have found all other nodes
        for (i = 0; i < doomcom.numnodes; i++)
        {
            if (nodesetup[i].nodesfound != nodesetup[i].nodeswanted)
            {
                break;
            }
        }

        if (i == nodesetup[0].nodeswanted)
        {
            // found all nodes
            break;
        }

        // send out a broadcast packet every second
        now = clock();
        if (now - last_time >= CLOCKS_PER_SEC)
        {
            last_time = now;

            nodesetup[0].nodesfound = doomcom.numnodes;
            nodesetup[0].plnumwanted = force_player;
            nodesetup[0].dupwanted = doomnet_dup;
            IPXSendPacket(&broadcast_addr, &nodesetup[0], sizeof(setupdata_t));
        }
    }

    total = 0;

    for (i = 0; i < numnetnodes; i++)
    {
        if (nodesetup[i].drone)
        {
            continue;
        }
        total++;
        if (total > MAXPLAYERS)
        {
            Error("More than %i players specified!", MAXPLAYERS);
        }
    }

    if (total == 0)
    {
        Error("No players specified for game!");
    }

    doomcom.consoleplayer = DetermineConsolePlayer();
    doomcom.numplayers = total;

    LogMessage("Console is player %i of %i", doomcom.consoleplayer + 1, total);
}

void main(int argc, char *argv[])
{
    char **args;

    // determine game parameters
    numnetnodes = 2;
    doomcom.ticdup = 1;
    doomcom.extratics = 1;
    doomcom.episode = 1;
    doomcom.map = 1;
    doomcom.skill = 2;
    doomcom.deathmatch = 0;

    IntFlag("-nodes", &numnetnodes, "n",
            "number of players in game, default 2");
    IntFlag("-player", &force_player, "p", "force this to be player #p");
    IPXRegisterFlags();
    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    // make sure the network exists and create a bunch of buffers
    InitNetwork();
    atexit(ShutdownNetwork);

    // get addresses of all nodes
    LookForNodes();

    IPXStartGame();
    ipx_localtime = 0;              // no longer in setup

    // launch DOOM
    NetLaunchDoom(&doomcom, args, NetCallback);
}

