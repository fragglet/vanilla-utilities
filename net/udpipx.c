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

// Alternative implementation of ipxnet.h interface that sends packets
// over UDP, implementing the DOSbox tunnelling protocol.

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"

#include "net/dossock.h"
#include "net/ipxnet.h"

const ipx_addr_t broadcast_addr = {0, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

static ipx_addr_t local_addr;
static packet_t packet;
static struct sockaddr_in server_addr;
static unsigned int ipxport = DOOM_DEFAULT_PORT;
static SOCKET sock;

long ipx_localtime;                 // for time stamp in packets

unsigned short ShortSwap(unsigned short i)
{
    return htons(i);
}

void IPXGetLocalAddress(ipx_addr_t *addr)
{
    memcpy(addr, &local_addr, sizeof(ipx_addr_t));
}

void IPXRegisterFlags(void)
{
    UnsignedIntFlag("-ipxport", &ipxport, "port", NULL);
}

void InitNetwork(void)
{
    // TODO: Determine server address.

    DosSockInit();

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        Error("InitNetwork: failed to create UDP socket: err=%d",
              DosSockLastError);
    }

    // TODO: Connect to server.

    memcpy(&packet.ipx.Src, &local_addr, sizeof(ipx_addr_t));
    packet.ipx.SrcSocket = htons(ipxport);
    packet.ipx.DestSocket = htons(ipxport);
}

void ShutdownNetwork(void)
{
    closesocket(sock);
}

void IPXSendPacket(const ipx_addr_t *addr, void *data, size_t data_len)
{
    size_t packet_len = data_len + sizeof(ipx_header_t) + 8;

    if (data_len > sizeof(packet.payload))
    {
        return;
    }

    memcpy(&packet.ipx.Dest, addr, sizeof(ipx_addr_t));
    packet.ipx.PacketLength = htons(packet_len);
    packet.time = ipx_localtime;
    memcpy(packet.payload, data, data_len);
    sendto(sock, &packet, packet_len, 0, &server_addr);
}

packet_t *IPXGetPacket(void)
{
    int result;

    result = recvfrom(sock, &packet, sizeof(packet), 0, NULL);
    if (result < 0)
    {
        // TODO: Should probably check for and log any real errors.
        return NULL;
    }

    return &packet;
}

void IPXReleasePacket(packet_t *packet)
{
    // No-op. Well, really, we should block IPXGetPacket() from
    // returning a new packet until the current one is returned.
}
