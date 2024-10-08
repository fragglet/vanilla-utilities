//
// Copyright(C) 1993 id Software, Inc.
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

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

typedef struct {
    void far *Link;                /* offset-segment */
    void far *ESRAddress;          /* offset-segment */
    uint8_t InUseFlag;
    uint8_t CompletionCode;
    uint16_t ECBSocket;            /* high-low */
    uint8_t IPXWorkspace[4];       /* N/A */
    uint8_t DriverWorkspace[12];   /* N/A */
    uint8_t ImmediateAddress[6];   /* high-low */
    uint16_t FragmentCount;        /* low-high */

    void far *fAddress;            /* offset-segment */
    uint16_t fSize;                /* low-high */
    void far *f2Address;           /* offset-segment */
    uint16_t f2Size;               /* low-high */
} ECB;

static packet_t packets[NUMPACKETS];
static ECB ecbs[NUMPACKETS];

const ipx_addr_t broadcast_addr = {0, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

static unsigned int socket_flag = DEFAULT_IPX_SOCKET;
static int socketid;

long ipx_localtime;                 // for time stamp in packets

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

void IPXGetLocalAddress(ipx_addr_t *addr)
{
    ll_regs.x.si = FP_OFF(addr);
    ll_regs.x.es = FP_SEG(addr);
    ll_regs.x.bx = 9;
    LowLevelCall();
    if (ll_regs.h.al != 0)
    {
        Error("Get inet addr: 0x%x", ll_regs.h.al);
    }
}

void IPXRegisterFlags(void)
{
    // We set the help text here rather than in ipxsetup.c, because
    // ipxsetup.c is reused between ipxsetup and udpsetup.
    SetHelpText("Doom IPX network device driver",
                "%s -nodes 4 doom.exe -warp 2 2 -deathmatch -skill 4");
    UnsignedIntFlag("-port", &socket_flag, "socket", NULL);
    UnsignedIntFlag("-socket", &socket_flag, "socket",
                    "(or -port) use alternate IPX socket number");
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
    ipx_addr_t localaddr;
    int i;

    InitIPX();

    // allocate a socket for sending and receiving
    socketid = OpenSocket(ShortSwap(socket_flag));
    if (socket_flag != DEFAULT_IPX_SOCKET)
    {
        char socketnum[10];
        sprintf(socketnum, "0x%x", socket_flag);
        SetLogDistinguisher(socketnum);
        LogMessage("Using alternate IPX socket %u for network", socket_flag);
    }

    // set up several receiving ECBs
    memset(packets, 0, NUMPACKETS * sizeof(packet_t));

    for (i = 1; i < NUMPACKETS; i++)
    {
        ecbs[i].ECBSocket = socketid;
        ecbs[i].FragmentCount = 1;
        ecbs[i].fAddress = &packets[i];
        ecbs[i].fSize = sizeof(packet_t);

        ListenForPacket(&ecbs[i]);
    }

    // set up a sending ECB
    memset(&packets[0], 0, sizeof(packets[0]));

    IPXGetLocalAddress(&localaddr);
    ecbs[0].ECBSocket = socketid;
    ecbs[0].FragmentCount = 2;
    ecbs[0].fAddress = &packets[0];
    memcpy(&packets[0].ipx.Dest, &localaddr, sizeof(ipx_addr_t));
    packets[0].ipx.DestSocket = ShortSwap(socket_flag);
}

void ShutdownNetwork(void)
{
    CloseSocket(socketid);
}

// Send packet to the given destination.
// A destination of MAXNETNODES is a broadcast.
void IPXSendPacket(const ipx_addr_t *addr, void *data, size_t data_len)
{
    // set the time
    packets[0].time = ipx_localtime;

    // set the address
    memcpy(&packets[0].ipx.Dest, addr, sizeof(ipx_addr_t));
    memcpy(ecbs[0].ImmediateAddress, addr->Node, 6);

    // set the length (ipx + time + datalength)
    ecbs[0].fSize = sizeof(ipx_header_t) + 4;
    ecbs[0].f2Address = data;
    ecbs[0].f2Size = data_len + 4;

    // send the packet
    ll_regs.x.si = FP_OFF(&ecbs[0]);
    ll_regs.x.es = FP_SEG(&ecbs[0]);
    ll_regs.x.bx = 3;
    LowLevelCall();
    if (ll_regs.h.al != 0)
    {
        Error("SendPacket: 0x%x", ll_regs.h.al);
    }

    while (ecbs[0].InUseFlag != 0)
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

packet_t *IPXGetPacket(void)
{
    int packetnum;
    int i;
    long besttic;
    ECB *ecb;

    // if multiple packets are waiting, return them in order by time
    besttic = LONG_MAX;
    packetnum = -1;

    for (i = 1; i < NUMPACKETS; i++)
    {
        if (ecbs[i].InUseFlag)
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
        return NULL;               // no packets
    }

    ecb = &ecbs[packetnum];

    // got a good packet
    if (ecb->CompletionCode != 0)
    {
        Error("GetPacket: ecb.CompletionCode = 0x%x", ecb->CompletionCode);
    }

    return &packets[packetnum];
}

void IPXReleasePacket(packet_t *packet)
{
    int packetnum = packet - packets;
    ListenForPacket(&ecbs[packetnum]);
}

void IPXStartGame(void)
{
    // no-op.
}
