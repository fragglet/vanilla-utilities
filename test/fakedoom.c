// Fake standin for doom.exe that simulates use of some of its interfaces.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "lib/inttypes.h"

//#include "ctrl/control.h"
#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "stat/stats.h"

#define MAGIC_NUMBER 0x83b3

struct test_packet
{
    unsigned int magic;
    int consoleplayer;
    int secret;
};

FILE *log;
int net_secret = 1337;
//control_handle_t far *control = NULL;
doomcom_t far *doomcom = NULL;
wbstartstruct_t far *stats = NULL;

// TODO: Resolve ticcmd_t type conflict between headers.
/*
static void SetControlDriver(long l)
{
    assert(control == NULL);
    control = ControlGetHandle(l);
}
*/

static void SetNetDriver(long l)
{
    assert(doomcom == NULL);
    doomcom = NetGetHandle(l);
}

static void SetStatsDriver(long l)
{
    assert(stats == NULL);
    stats = StatsGetHandle(l);
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
    char *filename = NULL;

    //APIPointerFlag("-control", SetControlDriver);
    APIPointerFlag("-net", SetNetDriver);
    APIPointerFlag("-statcopy", SetStatsDriver);
    StringFlag("-out", &filename, "filename", "Output log file");
    IntFlag("-secret", &net_secret, "n", "Secret number for network test");
    NetRegisterFlags();
    ParseCommandLine(argc, argv);

    assert(filename != NULL);
    log = fopen(filename, "w");
    assert(log != NULL);

    if (doomcom != NULL)
    {
        RunNetworkTest();
    }

    fclose(log);

    for (start = clock(); clock() < start + CLOCKS_PER_SEC / 2; )
    {
    }

    return 0;
}

