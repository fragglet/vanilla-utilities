//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
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

#include "net/dbserver.h"
#include "net/dossock.h"
#include "net/ipxnet.h"

#define DEFAULT_UDP_PORT  213  /* as used by dosbox */

const ipx_addr_t broadcast_addr = {0, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

static char *server_addr_flag = NULL;
static ipx_addr_t local_addr;
static packet_t packet;
static int in_game = 0;
static int run_server_flag = 0;
static struct sockaddr_in server_addr;
static unsigned int udpport = DEFAULT_UDP_PORT;
static unsigned int ipxsocket = DEFAULT_IPX_SOCKET;
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
                "See UDPSETUP-HOWTO for examples.");
    StringFlag("-connect", &server_addr_flag, "addr[:port]",
               "(or -c) connect to server at specified address");
    StringFlag("-c", &server_addr_flag, NULL, NULL);
    BoolFlag("-server", &run_server_flag,
             "(or -s) run server for other clients to connect to");
    BoolFlag("-s", &run_server_flag, NULL);
    UnsignedIntFlag("-ipxsocket", &ipxsocket, "socket", NULL);
    UnsignedIntFlag("-udpport", &udpport, "port",
                    "UDP port that server should use, default 213");
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

static int SameServerAddr(struct sockaddr_in *a, struct sockaddr_in *b)
{
    // TODO: MSClient doesn't seem to have a proper loopback interface, so
    // even though we send to 127.0.0.1, the replies come back from a
    // different address. What we should do instead is use getsockname(),
    // but that's going to require more reverse engineering work.
    return 1 //a->sin_addr.s_addr == b->sin_addr.s_addr
        && a->sin_port == b->sin_port;
}

static int CheckRegistrationReply(void)
{
    struct sockaddr_in addr;
    int result;

    for (;;)
    {
        result = recvfrom(sock, &packet, sizeof(packet), 0, &addr);
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

        // Not from the server?
        if (!SameServerAddr(&server_addr, &addr))
        {
            continue;
        }

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

    if (!run_server_flag)
    {
        LogMessage("Trying...");
    }

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
        RunServer();
    } while (!CheckRegistrationReply());

    if (!run_server_flag)
    {
        LogMessage("Connection accepted");
    }
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
    struct sockaddr_in bind_any_addr = {AF_INET, 0, {INADDR_ANY}};
    unsigned long trueval = 1;

    in_game = 0;

    if (server_addr_flag != NULL)
    {
        ParseServerAddress(server_addr_flag);
    }
    else if (run_server_flag)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(udpport);
        server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    else
    {
        Error("Must specify either -connect or -server!");
    }

    DosSockInit();

    if (run_server_flag)
    {
        StartServer(udpport);
        LogMessage("Server now running on port %d", udpport);
    }

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        Error("InitNetwork: failed to create UDP socket: err=%d",
              DosSockLastError);
    }

    atexit(ShutdownNetwork);

    if (bind(sock, &bind_any_addr) < 0)
    {
        Error("Binding to a port failed, err=%d", DosSockLastError);
    }

    if (ioctlsocket(sock, FIONBIO, &trueval) < 0)
    {
        Error("Setting nonblocking failed, err=%d", DosSockLastError);
    }

    Connect();

    memcpy(&packet.ipx.Src, &local_addr, sizeof(ipx_addr_t));
    packet.ipx.SrcSocket = htons(ipxsocket);
    packet.ipx.DestSocket = htons(ipxsocket);
}

void IPXSendPacket(const ipx_addr_t *addr, void *data, size_t data_len)
{
    size_t packet_len = data_len + sizeof(ipx_header_t) + 8;

    RunServer();

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

    // If we're running a server, we just sent our packet to it via loopback.
    // So now we need to run the server to ensure it needs to be delivered to
    // the appropriate destination(s).
    RunServer();
}

packet_t *IPXGetPacket(void)
{
    struct sockaddr_in addr;

    RunServer();

    do
    {
        if (recvfrom(sock, &packet, sizeof(packet), 0, &addr) < 0)
        {
            // No more packets to process for now.
            // TODO: Should probably check for and log any real errors.
            return NULL;
        }

        // Not from the server?
        if (!SameServerAddr(&server_addr, &addr))
        {
            continue;
        }
        // Check destination address is for us.
        if (memcmp(&packet.ipx.Dest, &local_addr, sizeof(ipx_addr_t)) != 0
         && memcmp(&packet.ipx.Dest, &broadcast_addr, sizeof(ipx_addr_t)) != 0)
        {
            continue;
        }

        // Check destination IPX socket#, since we only care about our
        // specific port. If there are other games in progress on the
        // server, we definitely want to ignore them.
        if (ntohs(packet.ipx.DestSocket) == ipxsocket)
        {
            return &packet;
        }
        else if (ntohs(packet.ipx.DestSocket) == 86
              && ntohs(packet.ipx.SrcSocket) == 86 && !in_game)
        {
            // We don't abort the program if the game's in progress.
            Error("Server has shut down");
        }
    } while (1);
}

void IPXReleasePacket(packet_t *packet)
{
    // No-op. Well, really, we should block IPXGetPacket() from
    // returning a new packet until the current one is returned.
    packet = NULL;
}

void IPXStartGame(void)
{
    in_game = 1;
}
