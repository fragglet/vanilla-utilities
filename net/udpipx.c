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
#include <time.h>

#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"

#include "net/dossock.h"
#include "net/ipxnet.h"

#define DEFAULT_UDP_PORT  213  /* as used by dosbox */

const ipx_addr_t broadcast_addr = {0, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

static char *server_addr_flag = NULL;
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
    SetHelpText("Doom UDP/IP network device driver",
                "%s -nodes 4 -c 192.168.1.5 doom2.exe -warp 7 -deathmatch");
    UnsignedIntFlag("-ipxport", &ipxport, "port", NULL);
    StringFlag("-connect", &server_addr_flag, "addr[:port]",
               "[or -c] connect to server at specified address");
    StringFlag("-c", &server_addr_flag, NULL, NULL);
}

static void ParseServerAddress(const char *addr)
{
    const char *p;

    if (!inet_aton(addr, &server_addr.sin_addr))
    {
        Error("Not a valid server address: %s\n"
              "DNS names are not supported; you must specify an IP "
              "address.\nTo resolve a name, try using the 'ping' "
              "command.", addr);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_UDP_PORT);

    p = strchr(addr, ':');
    if (p != NULL)
    {
        server_addr.sin_port = htons(atoi(p + 1));
    }
}

static void SendRegistration(void)
{
    ipx_header_t tmphdr;

    memset(&tmphdr, 0, sizeof(tmphdr));
    tmphdr.DestSocket = htons(2);
    tmphdr.SrcSocket = htons(2);
    tmphdr.PacketCheckSum = htons(0xffff);
    tmphdr.PacketLength = htons(0x1e);
    tmphdr.PacketTransportControl = 0;
    tmphdr.PacketType = 0xff;

    if (sendto(sock, &tmphdr, sizeof(ipx_header_t), 0, &server_addr) < 0)
    {
        Error("Error sending registration packet: errno=%d",
              DosSockLastError);
    }
}

static int CheckRegistrationReply(void)
{
    int result;

    for (;;)
    {
        // TODO: Check the remote address matches the server.
        result = recvfrom(sock, &packet, sizeof(packet), 0, NULL);
        if (result < 0)
        {
            if (DosSockLastError == WSAEWOULDBLOCK)
            {
                return 0;
            }
            else
            {
                Error("Error receiving packet: errno=%d", DosSockLastError);
            }
        }

        LogMessage("got packet, src=%d, dest=%d", packet.ipx.SrcSocket, packet.ipx.DestSocket);

        if (ntohs(packet.ipx.SrcSocket) == 2
         && ntohs(packet.ipx.DestSocket) == 2)
        {
            memcpy(&local_addr, &packet.ipx.Dest, sizeof(ipx_addr_t));
            return 1;
        }
    }
}

static void Connect(void)
{
    clock_t start_time, last_send_time, now;

    start_time = clock();
    last_send_time = start_time - 2 * CLOCKS_PER_SEC;;

    do
    {
        CheckAbort("Connection to server");
        now = clock();
        if (now - start_time > 5 * CLOCKS_PER_SEC)
        {
            Error("No response from server");
        }
        else if (now - last_send_time > CLOCKS_PER_SEC)
        {
            last_send_time = now;
            SendRegistration();
        }
    } while (!CheckRegistrationReply());
}

void ShutdownNetwork(void)
{
    if (sock != INVALID_SOCKET)
    {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}

void InitNetwork(void)
{
    unsigned long trueval = 1;

    if (server_addr_flag == NULL)
    {
        Error("Must specify server address with -connect!");
    }

    ParseServerAddress(server_addr_flag);
    DosSockInit();

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        Error("InitNetwork: failed to create UDP socket: err=%d",
              DosSockLastError);
    }

    atexit(ShutdownNetwork);

    if (ioctlsocket(sock, FIONBIO, &trueval) < 0)
    {
        Error("Setting nonblocking failed, err=%d", DosSockLastError);
    }

    Connect();

    memcpy(&packet.ipx.Src, &local_addr, sizeof(ipx_addr_t));
    packet.ipx.SrcSocket = htons(ipxport);
    packet.ipx.DestSocket = htons(ipxport);
}

void IPXSendPacket(const ipx_addr_t *addr, void *data, size_t data_len)
{
    size_t packet_len = data_len + sizeof(ipx_header_t) + 8;

    if (data_len > sizeof(packet.payload))
    {
        return;
    }

    memcpy(&packet.ipx.Src, &local_addr, sizeof(ipx_addr_t));
    memcpy(&packet.ipx.Dest, addr, sizeof(ipx_addr_t));
    packet.ipx.PacketLength = htons(packet_len);
    packet.time = ipx_localtime;
    memcpy(packet.payload, data, data_len);
    if (sendto(sock, &packet, packet_len, 0, &server_addr) < 0)
    {
        // TODO: Log error.
    }
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
    packet = NULL;
}
