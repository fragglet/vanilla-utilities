//
// Copyright(C) 2023 Simon Howard
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

// Mini dosbox server implementation

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/dos.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/dossock.h"
#include "net/ipxnet.h"

extern const ipx_addr_t broadcast_addr;
const ipx_addr_t null_addr = {0, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

static struct sockaddr_in server_client_addrs[MAXNETNODES];
static int server_num_clients = 0;

static packet_t packet;
static SOCKET server_sock = INVALID_SOCKET;

static void SockaddrToIPX(struct sockaddr_in *inaddr, ipx_addr_t *ipxaddr)
{
    ipxaddr->Network = 0;
    memcpy(&ipxaddr->Node[0], &inaddr->sin_addr, 4);
    memcpy(&ipxaddr->Node[4], &inaddr->sin_port, 2);
}

static void IPXToSockaddr(ipx_addr_t *ipxaddr, struct sockaddr_in *inaddr)
{
    inaddr->sin_family = AF_INET;
    memcpy(&inaddr->sin_addr, &ipxaddr->Node[0], 4);
    memcpy(&inaddr->sin_port, &ipxaddr->Node[4], 2);
}

static int InClientsList(struct sockaddr_in *addr)
{
    int i;

    for (i = 0; i < server_num_clients; i++)
    {
        if (!memcmp(&server_client_addrs[i], addr, sizeof(struct sockaddr_in)))
        {
            return 1;
        }
    }

    return 0;
}

static void ForwardPacket(struct sockaddr_in *src_addr, int len)
{
    struct sockaddr_in dest_addr;
    int i;

    if (memcmp(&packet.ipx.Dest, &broadcast_addr, sizeof(ipx_addr_t)) != 0)
    {
        IPXToSockaddr(&packet.ipx.Dest, &dest_addr);
        if (sendto(server_sock, &packet, len, 0, &dest_addr) < 0)
        {
            // TODO: log error
        }
        return;
    }

    for (i = 0; i < server_num_clients; i++)
    {
        if (memcmp(&server_client_addrs[i], src_addr,
                   sizeof(struct sockaddr_in)) != 0
         && sendto(server_sock, &packet, len, 0,
                   &server_client_addrs[i]) < 0)
        {
            // TODO: log error
        }
    }
}

static int IsRegistrationPacket(ipx_header_t *hdr)
{
    return !memcmp(&hdr->Dest, &null_addr, sizeof(ipx_addr_t))
        && ntohs(hdr->DestSocket) == 2;
}

static void NewClient(struct sockaddr_in *addr)
{
    ipx_header_t reply;

    if (!InClientsList(addr))
    {
        if (server_num_clients >= MAXNETNODES)
        {
            return;
        }
        memcpy(&server_client_addrs[server_num_clients], addr,
               sizeof(struct sockaddr_in));
        ++server_num_clients;
    }

    reply.PacketCheckSum = htons(0xffff);
    reply.PacketLength = htons(sizeof(ipx_header_t));;
    reply.PacketTransportControl = 0;
    reply.PacketType = 0;

    SockaddrToIPX(addr, &reply.Dest);
    reply.DestSocket = htons(2);

    memcpy(&reply.Src, &broadcast_addr, sizeof(ipx_addr_t));
    reply.Src.Network = htonl(1);
    reply.SrcSocket = htons(2);

    if (sendto(server_sock, &reply, sizeof(ipx_header_t), 0, addr) < 0)
    {
        // TODO: log error
    }
}

static void SendShutdown(void)
{
    ipx_header_t msg;
    int i, c;

    memset(&msg, 0, sizeof(msg));
    msg.PacketCheckSum = htons(0xffff);
    msg.PacketLength = htons(sizeof(msg));
    msg.PacketTransportControl = 0;
    msg.PacketType = 0xff;
    msg.DestSocket = htons(86);
    msg.SrcSocket = htons(86);

    memcpy(&msg.Src, &null_addr, sizeof(ipx_addr_t));

    for (c = 0; c < server_num_clients; c++)
    {
        SockaddrToIPX(&server_client_addrs[c], &msg.Dest);

        for (i = 0; i < 3; i++)
        {
            if (sendto(server_sock, &msg, sizeof(ipx_header_t), 0,
                       &server_client_addrs[c]) < 0)
            {
                LogMessage("Error sending shutdown packet: errno=%d",
                           DosSockLastError);
                return;
            }
        }
    }
}

void RunServer(void)
{
    struct sockaddr_in addr;
    int len;

    if (server_sock == INVALID_SOCKET)
    {
        return;
    }

    for (;;)
    {
        len = recvfrom(server_sock, &packet, sizeof(packet), 0, &addr);
        if (len < 0)
        {
            // TODO: WSAEWOULDBLOCK is expected, other errors are not.
            break;
        }
        if (IsRegistrationPacket(&packet.ipx))
        {
            NewClient(&addr);
            continue;
        }
        if (InClientsList(&addr))
        {
            ForwardPacket(&addr, len);
        }
    }
}

void ShutdownServer(void)
{
    if (server_sock != INVALID_SOCKET)
    {
        SendShutdown();
        closesocket(server_sock);
        server_sock = INVALID_SOCKET;
    }
}

void StartServer(uint16_t port)
{
    unsigned long trueval = 1;
    struct sockaddr_in bind_addr = {AF_INET, 0, {INADDR_ANY}};

    server_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_sock == INVALID_SOCKET)
    {
        Error("StartServer: failed to create UDP socket: err=%d",
              DosSockLastError);
    }

    atexit(ShutdownServer);

    if (ioctlsocket(server_sock, FIONBIO, &trueval) < 0)
    {
        Error("StartServer: setting nonblocking failed, err=%d",
              DosSockLastError);
    }

    bind_addr.sin_port = htons(port);
    if (bind(server_sock, &bind_addr) < 0)
    {
        Error("bind failed, err=%d", DosSockLastError);
    }

    server_num_clients = 0;
}
