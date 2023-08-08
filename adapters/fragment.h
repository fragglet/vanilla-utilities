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

#define MAX_REASSEMBLED_PACKET 2048

struct reassembled_packet
{
    int remotenode;
    uint8_t seq;
    uint16_t received;
    unsigned int datalength;
    uint8_t data[MAX_REASSEMBLED_PACKET];
};

void InitFragmentReassembly(doomcom_t far *d);
struct reassembled_packet *FragmentGetPacket(void);
void FragmentSendPacket(int remotenode, uint8_t *buf, size_t len);

