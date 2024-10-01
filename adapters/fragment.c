//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

// Packet fragmenter and reassembler using a minimalist fragmenting
// protocol to identify and reassemble packets. This is implemented as
// a workaround for the fact that ROTTCOM/COMMIT drivers expect support
// for larger packet sizes that Doom's drivers do not support.

#include <stdlib.h>

#include "lib/dos.h"
#include "net/doomnet.h"
#include "adapters/fragment.h"
#include "adapters/nodemap.h"

#define FRAGMENT_SIZE 500
#define REASSEMBLY_BUFFERS 8
#define FRAGMENT_HEADER_SIZE (sizeof(struct fragment) - FRAGMENT_SIZE)

struct fragment
{
    uint8_t seq;
    uint8_t fragment;
    uint8_t payload[FRAGMENT_SIZE];
};

static struct reassembled_packet buffers[REASSEMBLY_BUFFERS];
static doomcom_t far *driver;
static uint8_t send_seq;

void InitFragmentReassembly(doomcom_t far *d)
{
    driver = d;
}

static struct reassembled_packet *FindOrAllocateBuffer(
    int remotenode, uint8_t seq)
{
    int i;

    for (i = 0; i < REASSEMBLY_BUFFERS; ++i)
    {
        if (buffers[i].remotenode == remotenode && buffers[i].seq == seq)
        {
            return &buffers[i];
        }
    }

    for (i = 0; i < REASSEMBLY_BUFFERS; ++i)
    {
        if (buffers[i].received == 0)
        {
            return &buffers[i];
        }
    }
    // Throw away an existing buffer.
    // TODO: better heuristics for throwing away buffers
    for (i = 0; i < REASSEMBLY_BUFFERS; ++i)
    {
        if (buffers[i].remotenode == remotenode)
        {
            buffers[i].received = 0;
            return &buffers[i];
        }
    }
    buffers[0].received = 0;
    return &buffers[0];
}

struct reassembled_packet *FragmentGetPacket(void)
{
    struct reassembled_packet *result;
    struct fragment far *f = (struct fragment far *) driver->data;
    int fnum, num_fragments;

    while (NetGetPacket(driver))
    {
        // FIXME: Fragment reassembler should not be coupled to node
        // discovery code.
        if (CheckLateDiscover(driver))
        {
            continue;
        }

        fnum = f->fragment & 0x0f;
        num_fragments = (f->fragment >> 4) & 0x0f;

        result = FindOrAllocateBuffer(driver->remotenode, f->seq);
        result->seq = f->seq;
        result->remotenode = driver->remotenode;
        if (fnum == num_fragments - 1)
        {
            result->datalength = (num_fragments - 1) * FRAGMENT_SIZE
                               + driver->datalength - FRAGMENT_HEADER_SIZE;
        }
        far_memcpy(result->data + fnum * FRAGMENT_SIZE, f->payload,
                   driver->datalength - FRAGMENT_HEADER_SIZE);

        // If we have received every fragment of this packet then each
        // bit will have been set and we can return the reassembled
        // packet. Reset received to zero so the buffer can be reused
        // for the next call.
        result->received |= 1 << fnum;
        if (result->received == (1 << num_fragments) - 1)
        {
            result->received = 0;
            return result;
        }
    }

    // We didn't manage to assemble a complete packet yet.
    return NULL;
}

void FragmentSendPacket(int remotenode, uint8_t *buf, size_t len)
{
    struct fragment far *f = (struct fragment far *) driver->data;
    int sendbytes;
    int num_fragments;
    int i;

    driver->remotenode = remotenode;
    num_fragments = (len + FRAGMENT_SIZE - 1) / FRAGMENT_SIZE;

    for (i = 0; i < num_fragments; ++i)
    {
        f->seq = send_seq;
        f->fragment = (i & 0xf) | (num_fragments << 4);
        sendbytes = len;
        if (sendbytes > FRAGMENT_SIZE)
        {
            sendbytes = FRAGMENT_SIZE;
        }
        far_memcpy(f->payload, buf, sendbytes);
        driver->datalength = sendbytes + FRAGMENT_HEADER_SIZE;
        NetSendPacket(driver);
        buf += sendbytes;
        len -= sendbytes;
    }

    ++send_seq;
}

