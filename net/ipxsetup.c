// ipxsetup.c

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <string.h>
#include <stdarg.h>
#include <bios.h>
#include "lib/inttypes.h"

#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/ipxnet.h"

static int numnetnodes;

static setupdata_t nodesetup[MAXNETNODES];

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

    if (vectorishooked)
        setvect(doomcom.intnum, olddoomvect);

    va_start(argptr, error);
    vprintf(error, argptr);
    va_end(argptr);
    printf("\n");
    exit(1);
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
        localtime++;
        SendPacket(doomcom.remotenode);
    }
    else if (doomcom.command == CMD_GET)
    {
        GetPacket();
    }
}

/*
===================
=
= LookForNodes
=
= Finds all the nodes for the game and works out player numbers among them
=
= Exits with nodesetup[0..numnodes] and nodeadr[0..numnodes] filled in
===================
*/

void LookForNodes(void)
{
    int i;
    struct time time;
    int oldsec;
    setupdata_t *setup, *dest;
    int total, console;

    //
    // wait until we get [numnetnodes] packets, then start playing
    // the playernumbers are assigned by netid
    //
    LogMessage("Attempting to find %d players on IPX network", numnetnodes);
    LogMessage("Local address is %02x:%02x:%02x:%02x:%02x:%02x",
               nodeadr[0].node[0], nodeadr[0].node[1], nodeadr[0].node[2],
               nodeadr[0].node[3], nodeadr[0].node[4], nodeadr[0].node[5]);

    oldsec = -1;
    setup = (setupdata_t *) & doomcom.data;
    localtime = -1;             // in setup time, not game time

    //
    // build local setup info
    //
    nodesetup[0].nodesfound = 1;
    nodesetup[0].nodeswanted = numnetnodes;
    doomcom.numnodes = 1;

    do
    {
        //
        // check for aborting
        //
        while (bioskey(1))
        {
            if ((bioskey(0) & 0xff) == 27)
                Error("\n\nNetwork game synchronization aborted.");
        }

        //
        // listen to the network
        //
        while (GetPacket())
        {
            if (doomcom.remotenode == -1)
                dest = &nodesetup[doomcom.numnodes];
            else
                dest = &nodesetup[doomcom.remotenode];

            if (remotetime != -1)
            {                   // an early game packet, not a setup packet
                if (doomcom.remotenode == -1)
                    Error("Got an unknown game packet during setup");
                // if it allready started, it must have found all nodes
                dest->nodesfound = dest->nodeswanted;
                continue;
            }

            // update setup ingo
            memcpy(dest, setup, sizeof(*dest));

            if (doomcom.remotenode != -1)
                continue;       // allready know that node address

            //
            // this is a new node
            //
            memcpy(&nodeadr[doomcom.numnodes], &remoteadr,
                   sizeof(nodeadr[doomcom.numnodes]));

            //
            // if this node has a lower address, take all startup info
            //
            if (memcmp(&remoteadr, &nodeadr[0], sizeof(&remoteadr)) < 0)
            {
            }

            doomcom.numnodes++;

            LogMessage(
                "Found a node at %02x:%02x:%02x:%02x:%02x:%02x",
                remoteadr.node[0], remoteadr.node[1], remoteadr.node[2],
                remoteadr.node[3], remoteadr.node[4], remoteadr.node[5]);
        }
        //
        // we are done if all nodes have found all other nodes
        //
        for (i = 0; i < doomcom.numnodes; i++)
            if (nodesetup[i].nodesfound != nodesetup[i].nodeswanted)
                break;

        if (i == nodesetup[0].nodeswanted)
            break;              // got them all

        //
        // send out a broadcast packet every second
        //
        gettime(&time);
        if (time.ti_sec == oldsec)
            continue;
        oldsec = time.ti_sec;

        doomcom.datalength = sizeof(*setup);

        nodesetup[0].nodesfound = doomcom.numnodes;

        memcpy(&doomcom.data, &nodesetup[0], sizeof(*setup));

        SendPacket(MAXNETNODES);        // send to all

    } while (1);

    //
    // count players
    //
    total = 0;
    console = 0;

    for (i = 0; i < numnetnodes; i++)
    {
        if (nodesetup[i].drone)
            continue;
        total++;
        if (total > MAXPLAYERS)
            Error("More than %i players specified!", MAXPLAYERS);
        if (memcmp(&nodeadr[i], &nodeadr[0], sizeof(nodeadr[0])) < 0)
            console++;
    }

    if (!total)
        Error("No players specified for game!");

    doomcom.consoleplayer = console;
    doomcom.numplayers = total;

    LogMessage("Console is player %i of %i", console + 1, total);
}

/*
=============
=
= main
=
=============
*/

void main(int argc, char *argv[])
{
    char **args;

    //
    // determine game parameters
    //
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
    IPXRegisterFlags();
    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);

    // make sure the network exists and create a bunch of buffers
    InitNetwork();
    atexit(ShutdownNetwork);

    // get addresses of all nodes
    LookForNodes();

    localtime = 0;              // no longer in setup

    //
    // launch DOOM
    //
    LaunchDOOM(args);

    Error(NULL);
}
