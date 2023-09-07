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

// Serial Infrared driver.
// This is identical in many ways to SERSETUP, with one big difference being
// that the rx/tx queues are queues of packets rather than bytes. It's also
// much simpler since there's no need to worry about modems.
//
// The big challenge with Infrared communications is that both sides cannot
// transmit at the same time (it's a half-duplex connection). Hence, the
// normal SERSETUP driver does not work since both sides try to transmit to
// the other continuously while the game is in progress. For this driver, we
// use a token-passing scheme where only ever one side is transmitting at a
// time. Both sides queue up packets to send, and when the transmitting side
// has finished sending all packets, it sends a handoff indicator that the
// receiving side uses as a signal to begin transmitting. Because latency is
// crucial, we do this handoff inside the serial ISR so that it happens
// instantaneously.
//
// TODO: In theory it should be possible to support more than two players.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/ints.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/serarb.h"
#include "net/serport.h"

#define QUEUE_LEN  8

// Escape characters are reused from SERSETUP; we use a common protocol.
#define FRAMECHAR              0x70
#define FRAMECHAR_END_PACKET   0x00 /* End of packet - same as SERSETUP. */

// 0x10 ... 0x1f indicate a handoff to a peer, with the byte indicating which
// peer. 0x10 is player 1, 0x11 is player 2, etc.
#define FRAMECHAR_HANDOFF_BASE 0x10

struct packet_header
{
    uint8_t checksum;
    uint8_t dest, src;
};

struct packet
{
    uint8_t data[512];
    size_t data_len;
    int valid;
};

struct queue
{
    struct packet packets[QUEUE_LEN];
    unsigned int head, tail;
};

// setupdata_t is used as doomdata_t during setup
struct setup_data
{
    uint8_t setup_signature[16];
    int32_t station_id;
    int16_t wanted, found;
    int16_t dup;
    int16_t player;
};

static const uint8_t SETUP_SIGNATURE[16] = {
    0xb2, 0x88, 0x2e, 0x28, 0xb1, 0xc4, 0xaa, 0xdb,
    0x9e, 0xc2, 0x24, 0xec, 0x2c, 0x1f, 0x6c, 0xa2,
};

static struct setup_data node_data[MAXNETNODES];

static int nodes_flag = 2;
static int force_player = -1;

static struct queue inque, outque;
static unsigned int tx_offset;
static doomcom_t doomcom;

// We do addressing based on player number, but the doomcom interface has
// its own addressing where node 0 is always the local console.
// node_data[n].player maps doomcom.remotenode value to player number, and
// player_to_node does the opposite.
static int player_to_node[MAXNETNODES];

// The handoff partner is the peer we hand off to once we have completed
// sending packets. When we receive a handoff directed at us, we take over
// control of the channel.
static int handoff_partner;

// During gameplay we flip between STATE_TRANSMIT and STATE_WAIT as we pass
// the handoff token between nodes.
static enum { STATE_ARBITRATE, STATE_TRANSMIT, STATE_WAIT } state;
static clock_t last_handoff_time = 0;

// djb2 hash function
static uint8_t HashData(uint8_t *data, size_t data_len)
{
    uint16_t result = 5381;
    int i;

    for (i = 0; i < data_len; i++)
    {
        result = (result << 5) + result + data[i];
    }

    return (uint8_t) result;
}

static int PacketReceived(void)
{
    struct packet *pkt = &inque.packets[inque.head];
    struct packet_header *hdr = (struct packet_header *) pkt->data;
    unsigned int next_head = (inque.head + 1) & (QUEUE_LEN - 1);
    int queue_full = 0;

    // We only deliver packets addressed to us.
    if (pkt->data_len > sizeof(struct packet_header)
     && HashData(pkt->data + 1, pkt->data_len - 1) == hdr->checksum
     && (state == STATE_ARBITRATE || hdr->dest == doomcom.consoleplayer))
    {
        // If the queue is full, we just keep overwriting the last packet.
        queue_full = next_head == inque.tail;
        if (!queue_full)
        {
            pkt->valid = 1;
            inque.head = next_head;
            pkt = &inque.packets[next_head];
        }
    }

    pkt->valid = 0;
    pkt->data_len = 0;
    return !queue_full;
}

static void AddInByte(struct packet *pkt, uint8_t c)
{
    if (pkt->data_len < sizeof(pkt->data))
    {
        pkt->data[pkt->data_len] = c;
        ++pkt->data_len;
    }
}

static void ReceivedHandoff(void)
{
    if (state == STATE_WAIT)
    {
        state = STATE_TRANSMIT;
        last_handoff_time = clock();

        // If player 2 has nothing to send, it sends back an empty packet to
        // hand the token back to player 1 immediately. If player 1 has
        // nothing to send, it stops until SendPacket() is called to send
        // something. This stops us bouncing the token back and forth
        // continually during startup if there's no work to be done. During
        // the game we will be sending packets regularly anyway.
        if (doomcom.consoleplayer != 0 && outque.head == outque.tail)
        {
            outque.packets[outque.head].data_len = 0;
            outque.head = (outque.head + 1) & (QUEUE_LEN - 1);
        }

        JumpStart();
    }
}

int SerialByteReceived(uint8_t c)
{
    static int in_escape = 0;
    struct packet *pkt;
    int success = 1;

    pkt = &inque.packets[inque.head];

    if (in_escape)
    {
        in_escape = 0;
        if (c == FRAMECHAR)
        {
            AddInByte(pkt, FRAMECHAR);
        }
        else if (c == FRAMECHAR_END_PACKET
              || (c >= FRAMECHAR_HANDOFF_BASE
               && c < FRAMECHAR_HANDOFF_BASE + MAXNETNODES))
        {
            success = PacketReceived();
        }
        // We take over the channel when we receive a handoff specifically
        // directed to us.
        if (c == FRAMECHAR_HANDOFF_BASE + doomcom.consoleplayer)
        {
            ReceivedHandoff();
        }
    }
    else if (c == FRAMECHAR)
    {
        in_escape = 1;
    }
    else
    {
        AddInByte(pkt, c);
    }

    return success;
}

unsigned int SerialMoreTXData(void)
{
    struct packet *pkt;
    unsigned int i;
    uint8_t c;

    // We can only transmit when we are holding the transmit token. The
    // exception is if we are still in the player arbitration phase,
    // where we haven't found the other node yet. Since arbitration
    // packets are only sent once a second, we don't need to worry about
    // clashes.
    if (state == STATE_WAIT)
    {
        return 0;
    }

    // Nothing more to send.
    if (outque.head == outque.tail)
    {
        return 0;
    }

    pkt = &outque.packets[outque.tail];

    // End of packet?
    if (tx_offset >= pkt->data_len)
    {
        outque.tail = (outque.tail + 1) & (QUEUE_LEN - 1);
        tx_offset = 0;

        // If it's the last packet, hand off back to the other node.
        if (outque.head == outque.tail)
        {
            serial_tx_buffer[0] = FRAMECHAR;
            serial_tx_buffer[1] = FRAMECHAR_HANDOFF_BASE + handoff_partner;
            if (state != STATE_ARBITRATE)
            {
                state = STATE_WAIT;
            }
        }
        else
        {
            serial_tx_buffer[0] = FRAMECHAR;
            serial_tx_buffer[1] = FRAMECHAR_END_PACKET;
        }

        return 2;
    }

    i = 0;
    while (i < sizeof(serial_tx_buffer) && tx_offset < pkt->data_len)
    {
        c = pkt->data[tx_offset];
        ++tx_offset;
        if (c != FRAMECHAR)
        {
            serial_tx_buffer[i] = c; i++;
        }
        else if (i == sizeof(serial_tx_buffer) - 1)
        {
            // We'll put the FRAMECHAR in the next block.
            break;
        }
        else
        {
            serial_tx_buffer[i] = FRAMECHAR; i++;
            serial_tx_buffer[i] = FRAMECHAR; i++;
        }
    }

    return i;
}

static void SendPacket(void)
{
    struct packet *pkt = &outque.packets[outque.head];
    struct packet_header *hdr = (struct packet_header *) pkt->data;
    unsigned int next_head = (outque.head + 1) & (QUEUE_LEN - 1);

    if (state != STATE_ARBITRATE
     && (doomcom.remotenode == 0 || doomcom.remotenode >= doomcom.numnodes))
    {
        return;
    }

    if (doomcom.datalength > sizeof(pkt->data) - sizeof(struct packet_header)
     || next_head == outque.tail)
    {
        return;
    }

    memcpy(pkt->data + sizeof(struct packet_header),
           doomcom.data, doomcom.datalength);
    pkt->data_len = doomcom.datalength + sizeof(struct packet_header);
    hdr->dest = node_data[doomcom.remotenode].player;
    hdr->src = node_data[0].player;
    hdr->checksum = HashData(pkt->data + 1, pkt->data_len - 1);
    outque.head = next_head;

    JumpStart();
}

static void GetPacket(void)
{
    struct packet *pkt = &inque.packets[inque.tail];
    struct packet_header *hdr = (struct packet_header *) pkt->data;

    if (inque.head == inque.tail)
    {
        doomcom.remotenode = -1;
        return;
    }

    if (!pkt->valid)
    {
        // Haven't finished reading yet.
        doomcom.remotenode = -1;
        return;
    }

    memcpy(doomcom.data, pkt->data + sizeof(struct packet_header),
           pkt->data_len - sizeof(struct packet_header));
    doomcom.datalength = pkt->data_len - sizeof(struct packet_header);

    if (state == STATE_ARBITRATE)
    {
        doomcom.remotenode = 1;
    }
    else if (hdr->src < doomcom.numplayers)
    {
        doomcom.remotenode = player_to_node[hdr->src];
    }
    else
    {
        doomcom.remotenode = -1;
    }

    inque.tail = (inque.tail + 1) & (QUEUE_LEN - 1);
    ResumeReceive();
}

// CheckTimeout is called before packets to detect the case where no handoff
// has been received in a long time. Since this is Infrared, it will not be
// unlikely that we occasionally lose connectivity, in which case we can end
// up in a state where both sides are waiting for the other. After a long
// enough timeout of not receiving a handoff, player 1 breaks the deadlock by
// resuming transmit.
#define HANDOFF_EXPIRY_TIME  (CLOCKS_PER_SEC / 4)
static void CheckTimeout(void)
{
    clock_t now;

    if (state == STATE_ARBITRATE || doomcom.consoleplayer != 0)
    {
        return;
    }

    now = clock();

    if (now - last_handoff_time > HANDOFF_EXPIRY_TIME)
    {
        last_handoff_time = now;
        state = STATE_TRANSMIT;
        JumpStart();
    }
}

static void NetCallback(void)
{
    if (doomcom.command == CMD_SEND)
    {
        CheckTimeout();
        SendPacket();
    }
    else if (doomcom.command == CMD_GET)
    {
        GetPacket();
    }
}

static int NodeForStationID(int32_t station_id)
{
    int i;

    for (i = 1; i < doomcom.numnodes; i++)
    {
        if (station_id == node_data[i].station_id)
        {
            return i;
        }
    }

    return -1;
}

static void ProcessSetupPacket(void)
{
    struct setup_data *setup = (struct setup_data *) doomcom.data;
    int n;

    // Sanity checks.
    if (doomcom.datalength < sizeof(struct setup_data)
     || memcmp(setup->setup_signature, SETUP_SIGNATURE,
               sizeof(SETUP_SIGNATURE) != 0))
    {
        return;
    }

    n = NodeForStationID(setup->station_id);

    // New node?
    if (n == -1)
    {
        n = doomcom.numnodes;
        ++doomcom.numnodes;

        LogMessage("Found a node with station ID %08lx", setup->station_id);

        assert(setup->wanted >= 1 && setup->dup >= 1);
        assert(setup->found <= setup->wanted && setup->wanted < MAXNETNODES);
        assert(setup->player >= -1 && setup->player < setup->wanted);

        if (node_data[0].player != -1 && node_data[0].player == setup->player)
        {
            Error("Other node is also using -player %d. One node must "
                  "be changed to avoid clash.", force_player);
        }
        if (setup->wanted > node_data[0].wanted)
        {
            LogMessage("Other node is using -nodes %d. Adjusting to match.",
                       setup->wanted);
            node_data[0].wanted = setup->wanted;
        }
        if (setup->dup > doomnet_dup)
        {
            LogMessage("Other node is using -dup %d. Adjusting to match.",
                       setup->dup);
            node_data[0].dup = setup->dup;
        }
    }

    // update setup info
    memcpy(&node_data[n], setup, sizeof(struct setup_data));
}

static void AssignPlayerNumbers(void)
{
    int i, j, best;

    // First of all, populate the player mapping table with those players
    // that requested a specific player number.
    for (i = 0; i < doomcom.numnodes; i++)
    {
        player_to_node[i] = -1;
    }

    for (i = 0; i < doomcom.numnodes; i++)
    {
        if (node_data[i].player != -1)
        {
            player_to_node[node_data[i].player] = i;
        }
    }

    // The remaining nodes get their player number automatically.
    for (i = 0; i < doomcom.numnodes; i++)
    {
        if (player_to_node[i] != -1)
        {
            continue;
        }

        // Which unassigned player has the lowest station ID?
        best = -1;
        for (j = 0; j < doomcom.numnodes; j++)
        {
            if (node_data[j].player == -1
             && (best == -1
              || node_data[j].station_id < node_data[best].station_id))
            {
                best = j;
            }
        }
        assert(best != -1);
        player_to_node[i] = best;
        node_data[best].player = i;
    }
}

static int AllNodesReady(void)
{
    int i;

    // we are done if all nodes have found all other nodes
    for (i = 0; i < doomcom.numnodes; i++)
    {
        if (node_data[i].found != node_data[i].wanted)
        {
            return 0;
        }
    }

    return 1;
}

// Before transitioning out of the STATE_ARBITRATE state, we block for a
// maximum of one second to ensure the interrupt handler has finished
// transmitting all packets still buffered for transmit. This is important
// because we can otherwise end up with a corner-case deadlock where we're in
// STATE_WAIT waiting for the other side, while it's waiting for our
// last arbitration packet to launch the game.
static void FlushArbitrationPackets(void)
{
    int end_time = clock() + CLOCKS_PER_SEC;

    while (clock() < end_time && outque.head != outque.tail)
    {
        CheckAbort("Arbitration packet flush");
    }
}

static void SendSetupPacket(void)
{
    node_data[0].found = doomcom.numnodes;
    memcpy(doomcom.data, &node_data[0], sizeof(struct setup_data));
    doomcom.datalength = sizeof(struct setup_data);
    SendPacket();
}

// Find all nodes for the game and work out player numbers among them
void LookForNodes(void)
{
    clock_t now, last_time = 0;

    if (nodes_flag < 1 || nodes_flag > MAXNETNODES)
    {
        Error("-nodes value must be in the range 1..%d", MAXNETNODES);
    }

    if (force_player != -1
     && (force_player < 1 || force_player > nodes_flag))
    {
        Error("-player value must be in the range 1..%d", nodes_flag);
    }

    // TODO: Remove
    if (nodes_flag > 2)
    {
        LogMessage("Playing with more than two players has not been "
                   "extensively tested yet. Let me know if it works!");
    }

    // build local setup info
    memcpy(node_data[0].setup_signature, SETUP_SIGNATURE,
           sizeof(SETUP_SIGNATURE));
    node_data[0].station_id = GetEntropy();
    node_data[0].player = force_player == -1 ? -1 : force_player - 1;
    node_data[0].found = 1;
    node_data[0].wanted = nodes_flag;
    node_data[0].dup = doomnet_dup;
    doomcom.numnodes = 1;

    LogMessage("Attempting to find %d players", nodes_flag);
    LogMessage("Randomly generated station ID is %08lx",
               node_data[0].station_id);

    while (!AllNodesReady())
    {
        CheckAbort("Network game synchronization");

        // listen to the network
        for (;;)
        {
            GetPacket();
            if (doomcom.remotenode == -1)
            {
                break;
            }
            ProcessSetupPacket();
        }

        // send out a broadcast packet every second
        now = clock();
        if (now - last_time >= CLOCKS_PER_SEC)
        {
            SendSetupPacket();
            last_time = now;
        }
    }

    // Send one last setup packet before we launch the game, and block
    // for up to a second or until it really has been sent.
    SendSetupPacket();
    FlushArbitrationPackets();

    AssignPlayerNumbers();

    doomnet_dup = node_data[0].dup;
    doomcom.consoleplayer = node_data[0].player;
    doomcom.numplayers = doomcom.numnodes;

    LogMessage("Console is player %i of %i", doomcom.consoleplayer + 1,
               doomcom.numplayers);
    state = doomcom.consoleplayer == 0 ? STATE_TRANSMIT : STATE_WAIT;
}

void main(int argc, char *argv[])
{
    char **args;

    srand(GetEntropy());

    SetHelpText("Doom Serial Infrared network device driver",
                "%s -com2 doom.exe -deathmatch -nomonsters");
    IntFlag("-nodes", &nodes_flag, "n",
            "number of players in game, default 2");
    IntFlag("-player", &force_player, "p", "force this to be player #p");
    SerialRegisterFlags();
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

    // establish communications
    InitPort(115200);
    atexit(ShutdownPort);

    LookForNodes();

    // TODO: Currently, this is only assigned once. When nodes exit the
    // game, we need to recalculate the handoff partner.
    handoff_partner = (doomcom.consoleplayer + 1) % doomcom.numplayers;

    // launch DOOM
    NetLaunchDoom(&doomcom, args, NetCallback);
}
