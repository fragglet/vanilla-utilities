// Player arbitration code for establishing initial connection over a
// point-to-point link (eg. serial/modem), and determining which player
// we are. Different versions of Doom's SERSETUP used two different
// protocols; we support both.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lib/inttypes.h"

#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/serarb.h"

static struct {
    doomcom_t *doomcom;
    void (*net_cmd)(void);
    char localid[7];
    clock_t last_send_time;
    int localstage;
    int new_protocol;
} arb;

int force_player1 = 0, force_player2 = 0;

static int DoGetPacket(void)
{
    arb.doomcom->command = CMD_GET;
    arb.net_cmd();
    return arb.doomcom->remotenode != -1;
}

static void DoSendPacket(uint8_t *data, int data_len)
{
    arb.doomcom->command = CMD_SEND;
    arb.doomcom->remotenode = 1;
    arb.doomcom->datalength = data_len;
    memcpy(arb.doomcom->data, data, data_len);
    arb.net_cmd();
}

static void ProcessPackets(void)
{
    char remoteid[7];
    int remoteplayer, remotestage;
    uint8_t *packet = arb.doomcom->data;

    while (DoGetPacket())
    {
        packet[arb.doomcom->datalength] = '\0';

        if (sscanf(packet, "ID%6c_%d", remoteid, &remotestage) == 2)
        {
            arb.new_protocol = 1;
            arb.doomcom->consoleplayer =
                memcmp(arb.localid, remoteid, 6) > 0;
            if (!memcmp(arb.localid, remoteid, 6))
            {
                // TODO: Don't use Error() here because this can occur inside
                // the interrupt handler in background answer mode.
                Error("Duplicate ID string received");
            }
        }
        else if (sscanf(packet, "PLAY%d_%d", &remoteplayer, &remotestage) == 2)
        {
            arb.new_protocol = 0;

            // The original sersetup code would swap the player number when
            // detecting a conflict; however, this is not an algorithm that
            // is guaranteed to ever terminate. In our case since we only
            // ever use the old protocol when the other side needs to, we
            // can use this asymmetry as a way of resolving the deadlock:
            // we stick to our guns and do not change player, safe in the
            // knowledge that the other side will adapt to us.
            if (remoteplayer == arb.doomcom->consoleplayer)
            {
                remotestage = 0;
            }
        }
        else
        {
            continue;
        }

        // We got a packet successfully. Trigger a response with new state.
        arb.localstage = remotestage + 1;
        arb.last_send_time = 0;
    }
}

static void MakeLocalID(char *buf)
{
    uint32_t id;

    if (force_player1)
    {
        id = 0;
    }
    else if (force_player2)
    {
        id = 999999UL;
    }
    else
    {
        id = (rand() << 16) | rand();
        id = id % 1000000L;
    }
    sprintf(buf, "%.6ld", id);
}

static void InitArbitration(void)
{
    arb.localstage = 0;
    arb.new_protocol = 1;
    arb.last_send_time = 0;

    // allow override of automatic player ordering
    if (force_player1)
    {
        arb.doomcom->consoleplayer = 0;
    }
    else if (force_player2)
    {
        arb.doomcom->consoleplayer = 1;
    }

    MakeLocalID(arb.localid);
}

void StartArbitratePlayers(doomcom_t *dc, void (*net_cmd)(void))
{
    arb.doomcom = dc;
    arb.net_cmd = net_cmd;
    InitArbitration();

    LogMessage("Attempting to connect across link.");
}

int PollArbitratePlayers(void)
{
    clock_t now;
    char str[20];

    if (arb.localstage >= 2)
    {
        // Once complete, flush out any extras
        while (DoGetPacket())
        {
        }

        return 1;
    }

    ProcessPackets();

    now = clock();
    if (now - arb.last_send_time >= CLOCKS_PER_SEC)
    {
        arb.last_send_time = now;
        if (arb.new_protocol)
        {
            sprintf(str, "ID%6s_%d", arb.localid, arb.localstage);
        }
        else
        {
            sprintf(str, "PLAY%i_%i", arb.doomcom->consoleplayer, arb.localstage);
        }
        DoSendPacket(str, strlen(str));
    }

    return 0;
}

// Figure out who is player 0 and 1
void ArbitratePlayers(doomcom_t *dc, void (*net_cmd)(void))
{
    StartArbitratePlayers(dc, net_cmd);
    while (!PollArbitratePlayers())
    {
        CheckAbort("Connection");
    }
}

void RegisterArbitrationFlags(void)
{
    BoolFlag("-player1", &force_player1, "(or -player2) force player#");
    BoolFlag("-player2", &force_player2, NULL);
}

