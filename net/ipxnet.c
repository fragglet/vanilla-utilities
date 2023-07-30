// Interface code to IPX network API

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"

#include "net/doomnet.h"
#include "net/ipxnet.h"
#include "net/llcall.h"

// 0x869c is the official DOOM socket as registered with Novell back in the
// '90s. But the original IPXSETUP used a signed 16-bit integer for the port
// variable, causing an integer overflow. As a result, the actual default
// port number is one higher.
#define DOOM_DEFAULT_PORT 0x869d

extern doomcom_t doomcom;
static packet_t packets[NUMPACKETS];

nodeaddr_t nodeaddr[MAXNETNODES + 1];     // first is local, last is broadcast

nodeaddr_t remoteaddr;            // set by each GetPacket

static localaddr_t localaddr;            // set at startup

static unsigned int port_flag = DOOM_DEFAULT_PORT;
static int socketid;

long ipx_localtime;                 // for time stamp in packets
long ipx_remotetime;

int OpenSocket(unsigned short socketNumber)
{
    ll_regs.x.bx = 0;
    ll_regs.h.al = 0;              // longevity
    ll_regs.x.dx = socketNumber;
    LowLevelCall();
    if (ll_regs.h.al != 0)
    {
        Error("OpenSocket: 0x%x", ll_regs.h.al);
    }
    return ll_regs.x.dx;
}

void CloseSocket(short socketNumber)
{
    ll_regs.x.bx = 1;
    ll_regs.x.dx = socketNumber;
    LowLevelCall();
}

void ListenForPacket(ECB *ecb)
{
    ll_regs.x.si = FP_OFF(ecb);
    ll_regs.x.es = FP_SEG(ecb);
    ll_regs.x.bx = 4;
    LowLevelCall();
    if (ll_regs.h.al != 0)
    {
        Error("ListenForPacket: 0x%x", ll_regs.h.al);
    }
}

void GetLocalAddress(void)
{
    ll_regs.x.si = FP_OFF(&localaddr);
    ll_regs.x.es = FP_SEG(&localaddr);
    ll_regs.x.bx = 9;
    LowLevelCall();
    if (ll_regs.h.al != 0)
    {
        Error("Get inet addr: 0x%x", ll_regs.h.al);
    }
}

void IPXRegisterFlags(void)
{
    UnsignedIntFlag("-port", &port_flag, "port",
                    "use alternate IPX port number");
}

static void InitIPX(void)
{
    union REGS regs;
    struct SREGS sregs;

    // First, try to use the newer, redirector-based API:
    regs.x.ax = 0x7a00;
    int86x(0x2f, &regs, &regs, &sregs);
    if (regs.h.al != 0xff)
    {
        Error("IPX not detected");
    }

    ll_funcptr = MK_FP(sregs.es, regs.x.di);
}

void InitNetwork(void)
{
    int i, j;

    InitIPX();

    // allocate a socket for sending and receiving
    socketid = OpenSocket((port_flag >> 8) + ((port_flag & 255) << 8));
    if (port_flag != DOOM_DEFAULT_PORT)
    {
        char portnum[10];
        sprintf(portnum, "0x%x", port_flag);
        SetLogDistinguisher(portnum);
        LogMessage("Using alternate port %u for network", port_flag);
    }

    GetLocalAddress();

    // set up several receiving ECBs
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

    // set up a sending ECB
    memset(&packets[0], 0, sizeof(packets[0]));

    packets[0].ecb.ECBSocket = socketid;
    packets[0].ecb.FragmentCount = 2;
    packets[0].ecb.fAddress[0] = FP_OFF(&packets[0].ipx);
    packets[0].ecb.fAddress[1] = FP_SEG(&packets[0].ipx);
    for (j = 0; j < 4; j++)
    {
        packets[0].ipx.dNetwork[j] = localaddr.network[j];
    }
    packets[0].ipx.dSocket[0] = socketid & 255;
    packets[0].ipx.dSocket[1] = socketid >> 8;
    packets[0].ecb.f2Address[0] = FP_OFF(doomcom.data);
    packets[0].ecb.f2Address[1] = FP_SEG(doomcom.data);

    // known local node at 0
    for (i = 0; i < 6; i++)
    {
        nodeaddr[0].node[i] = localaddr.node[i];
    }

    // broadcast node at MAXNETNODES
    for (j = 0; j < 6; j++)
    {
        nodeaddr[MAXNETNODES].node[j] = 0xff;
    }
}

void ShutdownNetwork(void)
{
    CloseSocket(socketid);
}

// Send packet to the given destination.
// A destination of MAXNETNODES is a broadcast.
void SendPacket(int destination)
{
    int j;

    // set the time
    packets[0].time = ipx_localtime;

    // set the address
    for (j = 0; j < 6; j++)
    {
        packets[0].ipx.dNode[j] = packets[0].ecb.ImmediateAddress[j] =
            nodeaddr[destination].node[j];
    }

    // set the length (ipx + time + datalength)
    packets[0].ecb.fSize = sizeof(IPXPacket) + 4;
    packets[0].ecb.f2Size = doomcom.datalength + 4;

    // send the packet
    ll_regs.x.si = FP_OFF(&packets[0]);
    ll_regs.x.es = FP_SEG(&packets[0]);
    ll_regs.x.bx = 3;
    LowLevelCall();
    if (ll_regs.h.al != 0)
    {
        Error("SendPacket: 0x%x", ll_regs.h.al);
    }

    while (packets[0].ecb.InUseFlag != 0)
    {
        // IPX Relinquish Control - polled drivers MUST have this here!
        ll_regs.x.bx = 10;
        LowLevelCall();
    }
}

unsigned short ShortSwap(unsigned short i)
{
    return ((i & 255) << 8) + ((i >> 8) & 255);
}

// Returns false if no packet is waiting
int GetPacket(void)
{
    int packetnum;
    int i;
    long besttic;
    packet_t *packet;

    // if multiple packets are waiting, return them in order by time

    besttic = LONG_MAX;
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

    if (besttic == LONG_MAX)
    {
        return 0;               // no packets
    }

    packet = &packets[packetnum];

    if (besttic == -1 && ipx_localtime != -1)
    {
        ListenForPacket(&packet->ecb);
        return 0;               // setup broadcast from other game
    }

    ipx_remotetime = besttic;

    // got a good packet
    if (packet->ecb.CompletionCode != 0)
    {
        Error("GetPacket: ecb.CompletionCode = 0x%x",
              packet->ecb.CompletionCode);
    }

    // set remoteaddr to the sender of the packet
    memcpy(&remoteaddr, packet->ipx.sNode, sizeof(remoteaddr));
    for (i = 0; i < doomcom.numnodes; i++)
    {
        if (!memcmp(&remoteaddr, &nodeaddr[i], sizeof(remoteaddr)))
        {
            break;
        }
    }

    if (i < doomcom.numnodes)
    {
        doomcom.remotenode = i;
    }
    else if (ipx_localtime != -1)
    {
        // this really shouldn't happen
        ListenForPacket(&packet->ecb);
        return 0;
    }

    // copy out the data
    doomcom.datalength = ShortSwap(packet->ipx.PacketLength) - 38;
    memcpy(doomcom.data, packet->payload, doomcom.datalength);

    // repost the ECB
    ListenForPacket(&packet->ecb);

    return 1;
}
