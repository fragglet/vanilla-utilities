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

struct test_packet
{
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
    clock_t now, last_send = 0;
    int secrets[MAXPLAYERS];
    int i, got_nodes;

    LogMessage("Running network test with %d nodes", doomcom->numnodes);

    secrets[doomcom->consoleplayer] = net_secret;
    got_nodes = 1 << 0;

    do
    {
        CheckAbort("Network test");
        now = clock();
        if (now - last_send > 1)
        {
            SendTestPackets();
            last_send = now;
        }

        if (NetGetPacket(doomcom)
         && doomcom->datalength == sizeof(struct test_packet))
        {
            secrets[pkt->consoleplayer] = pkt->secret;
            got_nodes |= 1 << doomcom->remotenode;
        }
    } while (got_nodes != (1 << doomcom->numnodes) - 1);

    SendTestPackets();
    SendTestPackets();

    for (i = 0; i < doomcom->numplayers; ++i)
    {
        fprintf(log, "Player %d: secret=%d\n", i + 1, secrets[i]);
    }
}

int main(int argc, char *argv[])
{
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
    return 0;
}

