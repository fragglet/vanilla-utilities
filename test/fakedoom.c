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

// Fake standin for doom.exe that simulates use of some of its interfaces.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "lib/inttypes.h"

#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"

#define MAGIC_NUMBER 0x83b3

struct test_packet
{
    unsigned int magic;
    int consoleplayer;
    int secret;
};

static int myargc;
static char **myargv;

FILE *log;
int net_secret = 1337;
doomcom_t far *doomcom = NULL;

static char **CheckParm(char *name, int nargs)
{
    int i;

    for (i = 1; i < myargc; i++)
    {
        if (strcmp(myargv[i], name) != 0)
        {
            continue;
        }
        if (i + nargs >= myargc)
        {
            Error("%s needs %d arguments", name, nargs);
        }
        return myargv + i + 1;
    }

    return NULL;
}

static void SendTestPackets(void)
{
    struct test_packet far *pkt = (void far *) doomcom->data;
    int i;

    for (i = 1; i < doomcom->numnodes; ++i)
    {
        pkt->magic = MAGIC_NUMBER;
        pkt->consoleplayer = doomcom->consoleplayer;
        pkt->secret = net_secret;
        doomcom->datalength = sizeof(struct test_packet);
        doomcom->remotenode = i;
        NetSendPacket(doomcom);
    }
}

static void RunNetworkTest(void)
{
    struct test_packet far *pkt = (void far *) doomcom->data;
    clock_t now, end_time, last_send = 0;
    int secrets[MAXPLAYERS];
    int i, got_nodes;

    LogMessage("Running network test with %d nodes", doomcom->numnodes);

    secrets[doomcom->consoleplayer] = net_secret;
    got_nodes = 1 << 0;

    end_time = 0;

    do
    {
        CheckAbort("Network test");
        now = clock();
        if (now - last_send > CLOCKS_PER_SEC / 2)
        {
            SendTestPackets();
            last_send = now;
        }

        if (!NetGetPacket(doomcom)) {
            continue;
        }

        if (doomcom->datalength != sizeof(struct test_packet)) {
            LogMessage("Packet from %d wrong length, %d != %d",
                       doomcom->remotenode, doomcom->datalength,
                       sizeof(struct test_packet));
            continue;
        }
        if (pkt->magic != MAGIC_NUMBER)
        {
            LogMessage("Packet from %d wrong magic number, %04x != %04x",
                       doomcom->remotenode, pkt->magic, MAGIC_NUMBER);
            continue;
        }
        secrets[pkt->consoleplayer] = pkt->secret;
        got_nodes |= 1 << doomcom->remotenode;
        if (got_nodes == (1 << doomcom->numnodes) - 1 && end_time == 0)
        {
            LogMessage("All nodes found. Waiting before quit.");
            end_time = clock() + 5 * CLOCKS_PER_SEC;
        }
    } while (end_time == 0 || clock() < end_time);

    fprintf(log, "dup=%d extratics=%d\n", doomcom->ticdup,
            doomcom->extratics);
    for (i = 0; i < doomcom->numplayers; ++i)
    {
        fprintf(log, "Player %d: secret=%d\n", i + 1, secrets[i]);
    }
}

int main(int argc, char *argv[])
{
    clock_t start;
    char **arg;
    char *filename = NULL;

    myargc = argc;
    myargv = argv;

    if (argc < 2)
    {
        printf("Usage: %s -out <filename> [other args]\n\n"
               "  -secret n     Secret number for network test\n",
               argv[0]);
        exit(1);
    }

    StringFlag("-out", &filename, "filename", "Output log file");
    IntFlag("-secret", &net_secret, "n", "Secret number for network test");

    arg = CheckParm("-out", 1);
    if (arg == NULL)
    {
        Error("Must supply -out <filename> for log");
    }
    log = fopen(*arg, "w");
    assert(log != NULL);

    arg = CheckParm("-secret", 1);
    if (arg != NULL)
    {
        net_secret = atoi(*arg);
    }

    arg = CheckParm("-net", 1);
    if (arg != NULL)
    {
        doomcom = NetGetHandle(atol(*arg));
        RunNetworkTest();
    }

    fclose(log);

    for (start = clock(); clock() < start + CLOCKS_PER_SEC / 2; )
    {
    }

    return 0;
}

