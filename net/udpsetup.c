//
// Copyright (C) 2023 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/dossock.h"

static SOCKET sock;
static int player2;
static struct sockaddr_in remotehost;
static doomcom_t doomcom;

static void SendPacket(void)
{
    if (doomcom.remotenode != 1)
    {
        return;
    }

    sendto(sock, doomcom.data, doomcom.datalength, 0, &remotehost);
}

static void ReceivePacket(void)
{
    ssize_t result;

    result = recvfrom(sock, doomcom.data, sizeof(doomcom.data),
                      0, &remotehost);
    if (result > 0)
    {
        doomcom.remotenode = 1;
        doomcom.datalength = result;
    }
    else
    {
        doomcom.remotenode = -1;
    }
}

// Our interrupt service routine that is invoked by Doom when it wants
// to send or receive a packet.
// If numnodes=1 then this can just be an empty function. We only bother
// doing anything so that we can simulate extra dummy players.
static void NetCallback(void)
{
    if (doomcom.command == CMD_SEND)
    {
        SendPacket();
    }
    else if (doomcom.command == CMD_GET)
    {
        ReceivePacket();
    }
}

// Initialize the doomcom structure to default values.
static void InitDoomcom(void)
{
    doomcom.numnodes = 2;
    doomcom.ticdup = 1;
    doomcom.extratics = 0;

    doomcom.consoleplayer = player2;
    doomcom.numplayers = 2;

    // The following are ignored by Doom anyway.
    doomcom.angleoffset = 0;
    doomcom.drone = 0;

    doomcom.deathmatch = 0;
    doomcom.savegame = -1;
    doomcom.episode = 1;
    doomcom.map = 1;
    doomcom.skill = 3;
}

static void Shutdown(void)
{
    if (closesocket(sock) < 0)
    {
        LogMessage("close failed: %d", DosSockLastError);
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in bindaddr;
    unsigned long trueval = 1;
    char **args;

    BoolFlag("-player2", &player2, "player 2");
    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);
    if (ArgListLength(args) < 4)
    {
        ErrorPrintUsage("localport remoteaddr remoteport command");
    }

    bindaddr.sin_family = AF_INET;
    bindaddr.sin_port = htons(atoi(args[0]));
    bindaddr.sin_addr.s_addr = INADDR_ANY;

    remotehost.sin_family = AF_INET;
    remotehost.sin_port = htons(atoi(args[2]));
    if (!inet_aton(args[1], &remotehost.sin_addr))
    {
        Error("invalid IP address: %s", args[1]);
    }

    DosSockInit();
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    atexit(Shutdown);

    if (bind(sock, &bindaddr) < 0)
    {
        Error("bind: err=%d", DosSockLastError);
    }
    if (ioctlsocket(sock, FIONBIO, &trueval) < 0)
    {
        Error("setting nonblocking failed, err=%d", DosSockLastError);
    }

    InitDoomcom();

    NetLaunchDoom(&doomcom, args + 3, NetCallback);

    return 0;
}
