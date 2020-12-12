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

doomcom_t doomcom;
static int numnetnodes;

static int force_player = -1;
static setupdata_t nodesetup[MAXNETNODES];

static void NetCallback(void)
{
    if (doomcom.command == CMD_SEND)
    {
        ipx_localtime++;
        SendPacket(doomcom.remotenode);
    }
    else if (doomcom.command == CMD_GET)
    {
        GetPacket();
    }
}

// Process a setup packet found in doomcom.
static void ProcessSetupPacket(void)
{
    setupdata_t *setup;
    int old_protocol;
    int n;

    setup = (setupdata_t *) doomcom.data;
    old_protocol = doomcom.datalength < sizeof(setupdata_t);
    // We support the xttl setup extensions but we still maintain compatibility
    // with the original IPXSETUP.
    if (old_protocol)
    {
        setup->dupwanted = 1;
        setup->plnumwanted = -1;
    }

    n = doomcom.remotenode;

    // New node?
    if (doomcom.remotenode == -1)
    {
        n = doomcom.numnodes;
        ++doomcom.numnodes;

        memcpy(&nodeaddr[n], &remoteaddr, sizeof(nodeaddr_t));

        LogMessage("Found a node at %02x:%02x:%02x:%02x:%02x:%02x",
                   remoteaddr.node[0], remoteaddr.node[1], remoteaddr.node[2],
                   remoteaddr.node[3], remoteaddr.node[4], remoteaddr.node[5]);

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

    if (ipx_remotetime != -1)
    {
        // an early game packet, not a setup packet
        if (doomcom.remotenode == -1)
        {
            Error("Got an unknown game packet during setup");
        }

        // if it already started, it must have found all nodes
        nodesetup[n].nodesfound = nodesetup[n].nodeswanted;
    }
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
    int i;
    clock_t now, last_time = 0;
    int total;

    if (force_player != -1
     && (force_player < 1 || force_player > numnetnodes))
    {
        Error("-player value must be in the range 1..%d", numnetnodes);
    }

    // wait until we get [numnetnodes] packets, then start playing
    // the playernumbers are assigned by netid
    LogMessage("Attempting to find %d players on IPX network", numnetnodes);
    LogMessage("Local address is %02x:%02x:%02x:%02x:%02x:%02x",
               nodeaddr[0].node[0], nodeaddr[0].node[1], nodeaddr[0].node[2],
               nodeaddr[0].node[3], nodeaddr[0].node[4], nodeaddr[0].node[5]);

    ipx_localtime = -1;             // in setup time, not game time

    // build local setup info
    nodesetup[0].nodesfound = 1;
    nodesetup[0].nodeswanted = numnetnodes;
    doomcom.numnodes = 1;

    do
    {
        CheckAbort("Network game synchronization");

        // listen to the network
        while (GetPacket())
        {
            ProcessSetupPacket();
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
            memcpy(&doomcom.data, &nodesetup[0], sizeof(setupdata_t));

            doomcom.datalength = sizeof(setupdata_t);
            SendPacket(MAXNETNODES);        // send to all
        }

    } while (1);

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

    SetHelpText("Doom IPX network device driver",
                "%s -nodes 4 doom.exe -warp 2 2 -deathmatch -skill 4");
    IntFlag("-nodes", &numnetnodes, "n",
            "number of nodes (players) in game, default 2");
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

    ipx_localtime = 0;              // no longer in setup

    // launch DOOM
    NetLaunchDoom(&doomcom, args, NetCallback);
}

