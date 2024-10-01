//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

// Packet "aggregation" code: this detects when an identical packet is sent to
// every other node in sequence by the caller. Doom does this when it generates
// a new tic, so the majority of transmitted data originates this way. The code
// here identifies such packets and translates them into broadcast sends; this
// is used as an optimization for example in the metanet driver so that the
// rate of packets sent over the forwarding network is O(n) of the number of
// nodes rather than O(n^2).

#include <stdlib.h>
#include <string.h>

#include "lib/inttypes.h"
#include "net/doomnet.h"

#define PENDING_BUFFER_LEN 512

static int aggr_numnodes;
static void (*aggr_sendpkt)(int node, void *data, size_t data_len);

// When we send packets, they sometimes get placed into the broadcast
// pending buffer to determine if they should be sent as broadcast packets
// to save bandwidth:
static uint8_t pending_buffer[PENDING_BUFFER_LEN];
static size_t pending_buffer_len;
static int pending_count;

// Vanilla Doom for whatever transmits empty packets with no ticcmds. This
// conveys no useful information but does waste bandwidth, so we detect and
// drop them.
static int IsEmptyPacket(uint8_t *data, size_t data_len)
{
    unsigned long csgot, cswant;

    // Must be exactly the size of the doomdata_t header with zero tics,
    // and numtics field must = 0 tics too.
    if (data_len != 8 || data[7] != 0)
    {
        return 0;
    }

    // Check the checksum is a Doom game packet like we expect, and that
    // none of the other special bits are set (NCMD_RETRANSMIT, NCMD_SETUP...)
    csgot = 0x1234567
          + (((unsigned long) data[6]) << 16)
          + (((unsigned long) data[5]) << 8)
          + ((unsigned long) data[4]);
    cswant = (((unsigned long) data[3]) << 24)
           | (((unsigned long) data[2]) << 16)
           | (((unsigned long) data[1]) << 8)
           | ((unsigned long) data[0]);

    return (csgot & NCMD_CHECKSUM) == cswant;
}

// FlushPendingPackets sends the currently pending packet to any nodes for
// which it was held back - if any.
void FlushPendingPackets(void)
{
    int i;

    for (i = 0; i < pending_count; ++i)
    {
        aggr_sendpkt(i + 1, pending_buffer, pending_buffer_len);
    }

    pending_count = 0;
}

// TryBroadcastStore tries to stage the packet in aggregation_doomcom and
// returns 1 if the packet should not be sent yet.
static int TryBroadcastStore(int node, void *data, size_t data_len)
{
    // The objective here is to try to detect the specific sequence of calls
    // from Doom's NetUpdate() function - when a new tic is generated, it
    // does a transmit to each node of (usually) the exact same data.
    // We count up the number of such identical packets we have received and
    // when pending_count == num_nodes - 1, we have successfully detected
    // something that can be sent as a broadcast packet.

    // If this looks like the first packet in a sequence, we store the packet
    // into inner->data but don't send yet.
    if (node == 1)
    {
        FlushPendingPackets();
        if (data_len > PENDING_BUFFER_LEN)
        {
            return 0;
        }
        pending_buffer_len = data_len;
        memcpy(pending_buffer, data, data_len);
        pending_count = 0;
    }
    // The packet must exactly match the previous ones, and be in sequence.
    else if (node != pending_count + 1 || pending_buffer_len != data_len
          || memcmp(pending_buffer, data, data_len) != 0)
    {
        FlushPendingPackets();
        return 0;
    }

    // We got the next in sequence successfully.
    ++pending_count;
    if (pending_count == aggr_numnodes - 1)
    {
        // We have a complete broadcast packet.
        aggr_sendpkt(MAXNETNODES, pending_buffer, pending_buffer_len);
        pending_count = 0;
    }
    return 1;
}

void AggregatedSendPacket(int node, void *data, size_t data_len)
{
    if (IsEmptyPacket(data, data_len))
    {
        return;
    }

    if (node > 0 && node < aggr_numnodes
     && TryBroadcastStore(node, data, data_len))
    {
        return;
    }

    // No buffering, pass right through and send straight away.
    aggr_sendpkt(node, data, data_len);
}

void InitAggregation(int numnodes,
                     void (*send)(int node, void *data, size_t data_len))
{
    aggr_numnodes = numnodes;
    aggr_sendpkt = send;
}

