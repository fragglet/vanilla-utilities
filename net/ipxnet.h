//
// Copyright(C) 1993 id Software, Inc.
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

#include "lib/inttypes.h"

//===========================================================================

#define NUMPACKETS      10      // max outstanding packets before loss

// 0x869c is the official DOOM socket as registered with Novell back in the
// '90s. But the original IPXSETUP used a signed 16-bit integer for the port
// variable, causing an integer overflow. As a result, the actual default
// socket number is one lower.
#define DEFAULT_IPX_SOCKET 0x869b

typedef struct {
    uint32_t Network;              /* high-low */
    uint8_t Node[6];               /* high-low */
} ipx_addr_t;

typedef struct {
    uint16_t PacketCheckSum;       /* high-low */
    uint16_t PacketLength;         /* high-low */
    uint8_t PacketTransportControl;
    uint8_t PacketType;

    ipx_addr_t Dest;
    uint16_t DestSocket;           /* high-low */

    ipx_addr_t Src;
    uint16_t SrcSocket;            /* high-low */
} ipx_header_t;

// time is used by the communication driver to sequence packets returned
// to DOOM when more than one is waiting

typedef struct {
    ipx_header_t ipx;

    int32_t time;
    uint8_t payload[512];
} packet_t;

extern long ipx_localtime;          // for time stamp in packets
extern const ipx_addr_t broadcast_addr;

void IPXRegisterFlags(void);
void InitNetwork(void);
void ShutdownNetwork(void);
void IPXGetLocalAddress(ipx_addr_t *addr);
void IPXSendPacket(const ipx_addr_t *addr, void *data, size_t data_len);
void IPXReleasePacket(packet_t *packet);
packet_t *IPXGetPacket(void);
void IPXStartGame(void);
unsigned short ShortSwap(unsigned short i);

