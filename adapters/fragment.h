//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
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

