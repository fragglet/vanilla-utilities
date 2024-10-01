//
// Copyright (C) 1994-1995 Apogee Software, Ltd.
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#define ROTT_MAXPACKETSIZE    2048

#if __WATCOMC__
#pragma pack (1)
#endif

typedef struct
{
    short   intnum;            // ROTT executes an int to send commands

// communication between ROTT and the driver
    short   command;           // CMD_SEND or CMD_GET
    short   remotenode;        // dest for send, set by get (-1 = no packet)
    short   datalength;        // bytes in rottdata to be sent / bytes read

// info specific to this node
    short   consoleplayer;     // 0-3 = player number
    short   numplayers;        // 1-4
    short   client;            // 0 = server 1 = client
    short   gametype;          // 0 = modem  1 = network
    short   ticstep;           // 1 for every tic 2 for every other tic ...
    short   remoteridicule;    // 0 = remote ridicule is off 1= rr is on

// packet data to be sent
    char    data[ROTT_MAXPACKETSIZE];
} rottcom_t;

#if __WATCOMC__
#pragma pack (4)
#endif

#define ROTT_MODEM_GAME   0
#define ROTT_NETWORK_GAME 1

