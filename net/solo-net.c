//
// Doom solo / minimal netgame driver.
// Copyright (C) 2014-2023 Simon Howard
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"

#define RECV_QUEUE_LEN 8

static doomcom_t doomcom;
static doompacket_t *packet;

// Receive queue. We place packets onto the queue for receiving and
// dequeue them when the game issues CMD_GET.
static doompacket_t recv_queue[RECV_QUEUE_LEN];
static int recv_queue_head = 0, recv_queue_tail = 0;

// Copy the given packet and place it onto the receive queue.
static void QueuePacket(doompacket_t *sendpacket)
{
    // Don't overflow the queue. Drop packets if necessary.
    if (((recv_queue_tail + 1) % RECV_QUEUE_LEN) == recv_queue_head)
    {
        return;
    }

    memcpy(&recv_queue[recv_queue_tail], sendpacket, sizeof(doompacket_t));

    recv_queue_tail = (recv_queue_tail + 1) % RECV_QUEUE_LEN;
}

// Calculate the size (in bytes) of the given packet. This is important
// because the game will discard packets of the wrong length.
static int PacketSize(doompacket_t *sendpacket)
{
    return 8 + sizeof(ticcmd_t) * sendpacket->numtics;
}

// Calculate the checksum for the given packet and fill in the checksum
// field. If a packet has the wrong checksum the game will discard it.
static void CalculateChecksum(doompacket_t *sendpacket)
{
    unsigned long c;
    unsigned long *p;
    int i, l;

    c = 0x1234567l;

    // Length in 32-bit words. We do not include the checksum field,
    // or there is a bootstrapping problem.
    l = (PacketSize(sendpacket) - 4) / 4;
    p = ((unsigned long *)sendpacket) + 1;

    for (i = 0; i < l; ++i)
    {
        c += *p * (i + 1);
        ++p;
    }

    sendpacket->checksum = c & NCMD_CHECKSUM;
}

// Invoked when the game wants to send a packet.
static void SendPacket(void)
{
    static doompacket_t reply;
    int i;

    // The game sent a packet to us with some tics. To keep the game
    // runnning we need to send some tics back to it, otherwise it will
    // stall. So this is "self clocking": we create packets in a
    // tit-for-tat fashion as we receive them.

    memset(&reply, 0, sizeof(reply));
    reply.retransmitfrom = 0;
    reply.player = doomcom.remotenode;

    // If the game sent a setup packet, just respond with an "empty"
    // packet starting at tic #0 to let it know that we're in the
    // game and it can start. The game sent the episode/map encoded in
    // starttic so we can't use that. Otherwise, we send a packet back
    // with the same starttic as we received.
    if ((packet->checksum & NCMD_SETUP) != 0)
    {
        reply.starttic = 0;
        reply.numtics = 0;
    }
    else
    {
        reply.starttic = packet->starttic;
        reply.numtics = packet->numtics;

        // Fill in your own code for generating ticcmds here...
        // But there's not much that can be done. The game will quit
        // with a consistancy failure if the consistancy field is not
        // what it is expecting (ie. the lower 16 bits of the x
        // coordinate of the player). However - we can assume that
        // when the level starts it is equal to zero (as all things
        // are on a 1 unit boundary/granularity). This works until
        // one of the fake players moves - then it's game over.
        for (i = 0; i < reply.numtics; ++i)
        {
            reply.cmds[i].consistancy = 0;
        }
    }

    // Put the packet on the receive queue so that it will be read
    // when the game next asks for packets.
    CalculateChecksum(&reply);
    QueuePacket(&reply);
}

// Invoked when the game wants to check if a packet can be received.
static void ReceivePacket(void)
{
    // Receive queue empty?
    if (recv_queue_head == recv_queue_tail)
    {
        doomcom.remotenode = -1;
        doomcom.datalength = 0;
        return;
    }

    // Dequeue a packet.
    memcpy(packet, &recv_queue[recv_queue_head], sizeof(doompacket_t));
    recv_queue_head = (recv_queue_head + 1) % RECV_QUEUE_LEN;

    // Use the player number inside the packet as the remotenode.
    // Kind of hacky.
    doomcom.remotenode = packet->player;
    doomcom.datalength = PacketSize(packet);
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
    doomcom.numnodes = 1;
    doomcom.ticdup = 1;
    doomcom.extratics = 0;

    doomcom.consoleplayer = 0;
    doomcom.numplayers = 1;

    // The following are ignored by Doom anyway.
    doomcom.angleoffset = 0;
    doomcom.drone = 0;

    doomcom.deathmatch = 0;
    doomcom.savegame = -1;
    doomcom.episode = 1;
    doomcom.map = 1;
    doomcom.skill = 3;

    packet = (doompacket_t *) doomcom.data;
}

int main(int argc, char *argv[])
{
    char **args;
    int nodes = 1;

    SetHelpText("Doom single player network driver",
                "%s doom2.exe");
    IntFlag("-nodes", &nodes, "n",
            "total number of players (other players are simulated)");
    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    InitDoomcom();
    doomcom.numplayers = nodes;
    doomcom.numnodes = nodes;

    NetLaunchDoom(&doomcom, args, NetCallback);

    return 0;
}
