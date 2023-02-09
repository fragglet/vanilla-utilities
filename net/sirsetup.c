// Serial Infrared driver.
// This is identical in many ways to SERSETUP, with one big difference being
// that the rx/tx queues are queues of packets rather than bytes. It's also
// much simpler since there's no need to worry about modems.
// TODO: This is not yet complete; we do not yet prevent both sides
// transmitting at once, which is the main problem we're trying to solve.
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

#define QUEUE_LEN  4

// Escape characters are reused from SERSETUP; we use a common protocol.
#define FRAMECHAR             0x70
#define FRAMECHAR_END_PACKET  0x00 /* End of packet - same as SERSETUP. */

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

static int PacketReceived(void)
{
    struct packet *pkt;
    unsigned int next_head = (inque.head + 1) & (QUEUE_LEN - 1);
    int success;

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

    pkt = &outque.packets[outque.tail];

    // Nothing to send?
    if (outque.tail == outque.head)
    {
        return 0;
    }

    // End of packet?
    if (tx_offset >= pkt->data_len)
    {
        outque.tail = (outque.tail + 1) & (QUEUE_LEN - 1);
        tx_offset = 0;

	// TODO: Hand off to other node when there are no more packets.
        serial_tx_buffer[0] = FRAMECHAR;
        serial_tx_buffer[1] = FRAMECHAR_END_PACKET;

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

static void NetCallback(void)
{
    if (doomcom.command == CMD_SEND)
    {
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

    // launch DOOM
    NetLaunchDoom(&doomcom, args, NetCallback);
}
