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

// Types for 3DRealms COMMIT interface, based on the description given
// in "Communicating with 3DRealms Games" by Mark Dochtermann.

#define COMMIT_MAXPACKETSIZE 2048

enum
{
    COMMIT_CMD_SEND = 1,
    COMMIT_CMD_GET = 2,
    COMMIT_CMD_SENDTOALL = 3,
    COMMIT_CMD_SENDTOALLOTHERS = 4,
    COMMIT_CMD_SCORE = 5,
};

enum
{
    COMMIT_GAME_SERIAL = 1,
    COMMIT_GAME_MODEM = 2,
    COMMIT_GAME_NETWORK = 3,
};

#if __WATCOMC__
#pragma pack (1)
#endif

typedef struct
{
    int16_t intnum;

    int16_t command;
    int16_t remotenode;
    int16_t datalength;

    int16_t consoleplayer;
    int16_t numplayers;
    int16_t gametype;

    int16_t unused;

    uint8_t data[COMMIT_MAXPACKETSIZE];
} gamecom_t;

#if __WATCOMC__
#pragma pack (4)
#endif

