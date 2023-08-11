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
#define FRAMECHAR             0x70
#define FRAMECHAR_END_PACKET  0x00 /* End of packet - same as SERSETUP. */
#define FRAMECHAR_HANDOFF     0x04 /* ASCII end of transmission */

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

static struct queue inque, outque;
static unsigned int tx_offset;
static doomcom_t doomcom;

// During gameplay we flip between STATE_TRANSMIT and STATE_WAIT as we pass
// the handoff token between nodes.
static enum { STATE_ARBITRATE, STATE_TRANSMIT, STATE_WAIT } state;
static clock_t last_handoff_time = 0;

static int PacketReceived(void)
{
    struct packet *pkt;
    unsigned int next_head = (inque.head + 1) & (QUEUE_LEN - 1);
    int success;

    if (inque.packets[inque.head].data_len == 0)
    {
        return 1;
    }

    // If the queue is full we just keep overwriting the last packet.
    success = next_head != inque.tail;
    if (success)
    {
        inque.packets[inque.head].valid = 1;
        inque.head = next_head;
    }

    pkt = &inque.packets[inque.head];
    pkt->valid = 0;
    pkt->data_len = 0;
    return success;
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
        switch (c)
        {
            case FRAMECHAR:
                AddInByte(pkt, FRAMECHAR);
                break;

            case FRAMECHAR_END_PACKET:
                success = PacketReceived();
                break;

            case FRAMECHAR_HANDOFF:
                success = PacketReceived();
                ReceivedHandoff();
                break;
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
            serial_tx_buffer[1] = FRAMECHAR_HANDOFF;
            state = STATE_WAIT;
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
    struct packet *pkt;
    unsigned int next_head = (outque.head + 1) & (QUEUE_LEN - 1);

    pkt = &outque.packets[outque.head];

    if (doomcom.remotenode != 1 || doomcom.datalength > sizeof(pkt->data)
     || next_head == outque.tail)
    {
        return;
    }

    memcpy(pkt->data, doomcom.data, doomcom.datalength);
    pkt->data_len = doomcom.datalength;
    outque.head = next_head;

    JumpStart();
}

static void GetPacket(void)
{
    struct packet *pkt;

    if (inque.head == inque.tail)
    {
        doomcom.remotenode = -1;
        return;
    }

    pkt = &inque.packets[inque.tail];
    if (!pkt->valid)
    {
        // Haven't finished reading yet.
        doomcom.remotenode = -1;
        return;
    }

    memcpy(doomcom.data, pkt->data, pkt->data_len);
    doomcom.datalength = pkt->data_len;
    doomcom.remotenode = 1;
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

void main(int argc, char *argv[])
{
    char **args;

    srand(GetEntropy());

    SetHelpText("Doom Serial Infrared network device driver",
                "%s -com2 doom.exe -deathmatch -nomonsters");
    RegisterArbitrationFlags();
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

    ArbitratePlayers(&doomcom, NetCallback);

    state = doomcom.consoleplayer == 0 ? STATE_TRANSMIT : STATE_WAIT;

    // launch DOOM
    NetLaunchDoom(&doomcom, args, NetCallback);
}
