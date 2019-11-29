// ipxnet.c

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <string.h>
#include <process.h>
#include <values.h>

#include "lib/flag.h"
#include "net/ipxnet.h"

#define DOOM_DEFAULT_PORT 0x869c /* 0x869c is the official DOOM socket */

/*
=============================================================================

						IPX PACKET DRIVER

=============================================================================
*/

static packet_t packets[NUMPACKETS];

nodeadr_t nodeadr[MAXNETNODES + 1];     // first is local, last is broadcast

nodeadr_t remoteadr;            // set by each GetPacket

static localadr_t localadr;            // set at startup

static int port_flag = DOOM_DEFAULT_PORT;
static int socketid;

static union REGS regs;                // scratch for int86 calls
static struct SREGS sregs;

static unsigned short enteripx[2];

long localtime;                 // for time stamp in packets
long remotetime;

//===========================================================================

static const char *hex = "0123456789abcdef";

void PrintAddress(nodeadr_t *adr, char *str)
{
    int i;

    for (i = 0; i < 6; i++)
    {
        *str++ = hex[adr->node[i] >> 4];
        *str++ = hex[adr->node[i] & 15];
    }
    *str = 0;
}

int OpenSocket(short socketNumber)
{
    regs.x.bx = 0;
    regs.h.al = 0;              // longevity
    regs.x.dx = socketNumber;
    int86(0x7A, &regs, &regs);
    if (regs.h.al)
        Error("OpenSocket: 0x%x", regs.h.al);
    return regs.x.dx;
}

void CloseSocket(short socketNumber)
{
    regs.x.bx = 1;
    regs.x.dx = socketNumber;
    int86(0x7A, &regs, &regs);
}

void ListenForPacket(ECB *ecb)
{
    regs.x.si = FP_OFF(ecb);
    sregs.es = FP_SEG(ecb);
    regs.x.bx = 4;

    int86x(0x7a, &regs, &regs, &sregs);
    if (regs.h.al)
        Error("ListenForPacket: 0x%x", regs.h.al);
}

void GetLocalAddress(void)
{
    regs.x.si = FP_OFF(&localadr);
    sregs.es = FP_SEG(&localadr);
    regs.x.bx = 9;

    int86x(0x7a, &regs, &regs, &sregs);
    if (regs.h.al)
        Error("Get inet addr: 0x%x", regs.h.al);
}

void IPXRegisterFlags(void)
{
    IntFlag("-port", &port_flag, "port", "use alternate IPX port number");
}

/*
====================
=
= InitNetwork
=
====================
*/

void InitNetwork(void)
{
    int i, j;

    //
    // get IPX function address
    //
    regs.x.ax = 0x7a00;
    int86x(0x2f, &regs, &regs, &sregs);
    if (regs.h.al != 0xff)
        Error("IPX not detected\n");

    enteripx[0] = regs.x.di;
    enteripx[1] = sregs.es;

    //
    // allocate a socket for sending and receiving
    //
    socketid = OpenSocket((port_flag >> 8) + ((port_flag & 255) << 8));
    if (port_flag != (int) DOOM_DEFAULT_PORT)
    {
        printf("Using alternate port %i for network\n", port_flag);
    }

    GetLocalAddress();

    //
    // set up several receiving ECBs
    //
    memset(packets, 0, NUMPACKETS * sizeof(packet_t));

    for (i = 1; i < NUMPACKETS; i++)
    {
        packets[i].ecb.ECBSocket = socketid;
        packets[i].ecb.FragmentCount = 1;
        packets[i].ecb.fAddress[0] = FP_OFF(&packets[i].ipx);
        packets[i].ecb.fAddress[1] = FP_SEG(&packets[i].ipx);
        packets[i].ecb.fSize = sizeof(packet_t) - sizeof(ECB);

        ListenForPacket(&packets[i].ecb);
    }

    //
    // set up a sending ECB
    //
    memset(&packets[0], 0, sizeof(packets[0]));

    packets[0].ecb.ECBSocket = socketid;
    packets[0].ecb.FragmentCount = 2;
    packets[0].ecb.fAddress[0] = FP_OFF(&packets[0].ipx);
    packets[0].ecb.fAddress[1] = FP_SEG(&packets[0].ipx);
    for (j = 0; j < 4; j++)
        packets[0].ipx.dNetwork[j] = localadr.network[j];
    packets[0].ipx.dSocket[0] = socketid & 255;
    packets[0].ipx.dSocket[1] = socketid >> 8;
    packets[0].ecb.f2Address[0] = FP_OFF(&doomcom.data);
    packets[0].ecb.f2Address[1] = FP_SEG(&doomcom.data);

    // known local node at 0
    for (i = 0; i < 6; i++)
        nodeadr[0].node[i] = localadr.node[i];

    // broadcast node at MAXNETNODES
    for (j = 0; j < 6; j++)
        nodeadr[MAXNETNODES].node[j] = 0xff;

}

/*
====================
=
= ShutdownNetwork
=
====================
*/

void ShutdownNetwork(void)
{
    CloseSocket(socketid);
}

/*
==============
=
= SendPacket
=
= A destination of MAXNETNODES is a broadcast
==============
*/

void SendPacket(int destination)
{
    int j;

    // set the time
    packets[0].time = localtime;

    // set the address
    for (j = 0; j < 6; j++)
        packets[0].ipx.dNode[j] = packets[0].ecb.ImmediateAddress[j] =
            nodeadr[destination].node[j];

    // set the length (ipx + time + datalength)
    packets[0].ecb.fSize = sizeof(IPXPacket) + 4;
    packets[0].ecb.f2Size = doomcom.datalength + 4;

    // send the packet
    regs.x.si = FP_OFF(&packets[0]);
    sregs.es = FP_SEG(&packets[0]);
    regs.x.bx = 3;

    int86x(0x7a, &regs, &regs, &sregs);

    if (regs.h.al)
        Error("SendPacket: 0x%x", regs.h.al);

    while (packets[0].ecb.InUseFlag != 0)
    {
        // IPX Relinquish Control - polled drivers MUST have this here!
        regs.x.bx = 10;
        int86x(0x7a, &regs, &regs, &sregs);
    }
}

unsigned short ShortSwap(unsigned short i)
{
    return ((i & 255) << 8) + ((i >> 8) & 255);
}

/*
==============
=
= GetPacket
=
= Returns false if no packet is waiting
=
==============
*/

int GetPacket(void)
{
    int packetnum;
    int i, j;
    long besttic;
    packet_t *packet;

    // if multiple packets are waiting, return them in order by time

    besttic = MAXLONG;
    packetnum = -1;
    doomcom.remotenode = -1;

    for (i = 1; i < NUMPACKETS; i++)
    {
        if (packets[i].ecb.InUseFlag)
        {
            continue;
        }

        if (packets[i].time < besttic)
        {
            besttic = packets[i].time;
            packetnum = i;
        }
    }

    if (besttic == MAXLONG)
        return 0;               // no packets

    packet = &packets[packetnum];

    if (besttic == -1 && localtime != -1)
    {
        ListenForPacket(&packet->ecb);
        return 0;               // setup broadcast from other game
    }

    remotetime = besttic;

    //
    // got a good packet
    //
    if (packet->ecb.CompletionCode)
        Error("GetPacket: ecb.ComletionCode = 0x%x",
              packet->ecb.CompletionCode);

    // set remoteadr to the sender of the packet
    memcpy(&remoteadr, packet->ipx.sNode, sizeof(remoteadr));
    for (i = 0; i < doomcom.numnodes; i++)
        if (!memcmp(&remoteadr, &nodeadr[i], sizeof(remoteadr)))
            break;
    if (i < doomcom.numnodes)
        doomcom.remotenode = i;
    else
    {
        if (localtime != -1)
        {                       // this really shouldn't happen
            ListenForPacket(&packet->ecb);
            return 0;
        }
    }

    // copy out the data
    doomcom.datalength = ShortSwap(packet->ipx.PacketLength) - 38;
    memcpy(&doomcom.data, &packet->data, doomcom.datalength);

    // repost the ECB
    ListenForPacket(&packet->ecb);

    return 1;
}
