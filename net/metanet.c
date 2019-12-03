
#include <stdlib.h>
#include <assert.h>

#include "lib/flag.h"
#include "net/doomnet.h"

#define MAXDRIVERS 16

// Magic number field overlaps with the normal Doom checksum field.
// We ignore the top nybble so we can ignore NCMD_SETUP packets from
// the underlying driver, and use the bottom nybble for packet type.
#define META_MAGIC        0x01C36440
#define META_MAGIC_MASK   0x0FFFFFF0L

#define NODE_STATUS_GOT_DISCOVER  0x01
#define NODE_STATUS_READY         0x02

// A node_addr_t is a routing path to take through the network
// to reach the destination node. Each byte specifies the driver#
// and node# for next hop. There can be up to four hops.
typedef unsigned char node_addr_t[4];

enum meta_packet_type
{
    META_PACKET_DATA,
    META_PACKET_DISCOVER,
    META_PACKET_STATUS,
};

struct meta_header
{
    // See META_MAGIC above.
    unsigned long magic;
    node_addr_t src, dest;
};

struct meta_data_msg
{
    struct meta_header hdr;
    unsigned char data[1];  // [...]
};

struct meta_discover_msg
{
    struct meta_header header;
    unsigned int status;
    int num_neighbors;
    // Next hop address byte for all immediate neighbors:
    unsigned char neighbors[MAXNETNODES];
};

static doomcom_t far *drivers[MAXDRIVERS];
static int num_drivers = 0;

static node_addr_t node_addrs[MAXNETNODES];
static unsigned char node_flags[MAXNETNODES];
static int num_nodes = 1;

// Read packets from the given interface, forwarding them to other
// interfaces if destined for another machine, and returning true
// if a packet is received for this machine.
static int GetAndForward(int driver_index)
{
    doomcom_t far *dc = drivers[driver_index];
    struct meta_header far *hdr;
    unsigned int ddriver, dnode;

    while (NetGetPacket(dc))
    {
        hdr = (struct meta_header far *) &dc->data;
        if ((hdr->magic & NCMD_CHECKSUM) == NCMD_SETUP
         || (hdr->magic & META_MAGIC_MASK) != META_MAGIC)
        {
            continue;
        }
        // Update src address to include return path; this is needed
        // even if we are delivering the packet to ourself so we get
        // an accurate source address.
        if (hdr->src[sizeof(hdr->src) - 1] != 0)
        {
            continue;
        }
        _fmemmove(hdr->src + 1, hdr->src, sizeof(hdr->src) - 1);
        hdr->src[0] = (driver_index << 5) | dc->remotenode;
        // Packet for ourself?
        if (hdr->dest == 0)
        {
            return 1;
        }
        // This is a forwarding packet.
        // Decode next hop from first byte of routing dest:
        ddriver = (hdr->dest[0] >> 5) & 0x1f;
        dnode = hdr->dest[0] & 0x1f;
        if (ddriver >= num_drivers || dnode >= num_nodes
         || ddriver == driver_index || dnode == 0)
        {
            continue;
        }
        // Update destination.
        _fmemmove(hdr->dest, hdr->dest + 1, sizeof(hdr->dest) - 1);
        hdr->dest[sizeof(hdr->dest) - 1] = 0;

        // Copy into destination driver's buffer and send.
        _fmemmove(&drivers[ddriver]->data, dc->data, dc->datalength);
        NetSendPacket(drivers[ddriver]);
    }

    return 0;
}

static int NodeForAddr(node_addr_t far addr)
{
    unsigned int i;

    for (i = 0; i < num_nodes; ++i)
    {
        if (!memcmp(&node_addrs[i], addr, sizeof(node_addr_t)))
        {
            return i;
        }
    }
    return -1;
}

static int AppendNextHop(node_addr_t addr, unsigned char next_hop)
{
    unsigned int i;

    for (i = 0; i < sizeof(node_addr_t); ++i)
    {
        if (addr[i] == 0)
        {
            addr[i] = next_hop;
            return 1;
        }
    }
    return 0;
}

static int NodeOrAddNode(node_addr_t addr)
{
    int node_idx;

    node_idx = NodeForAddr(addr);
    if (node_idx >= 0)
    {
        return node_idx;
    }
    else if (num_nodes >= MAXNETNODES)
    {
        return -1;
    }

    node_idx = num_nodes;
    ++num_nodes;
    memcpy(&node_addrs[node_idx], addr, sizeof(node_addr_t));

    return node_idx;
}

static void HandleDiscover(struct meta_discover_msg far *dsc)
{
    node_addr_t addr;
    int node_idx;
    int i;

    _fmemcpy(addr, dsc->header.src, sizeof(node_addr_t));
    node_idx = NodeOrAddNode(addr);
    if (node_idx < 0)
    {
        return;
    }
    node_flags[node_idx] = dsc->status | NODE_STATUS_GOT_DISCOVER;

    for (i = 0; i < dsc->num_neighbors && i < MAXNETNODES; ++i)
    {
        _fmemcpy(addr, dsc->header.src, sizeof(node_addr_t));

        if (AppendNextHop(addr, dsc->neighbors[i]))
        {
            NodeOrAddNode(addr);
        }
    }
}

static int HandlePacket(doomcom_t far *dc)
{
    struct meta_header far *hdr;
    struct meta_data_msg far *data;
    node_addr_t addr;

    hdr = (struct meta_header far *) &dc->data;
    switch (hdr->magic & META_MAGIC_MASK)
    {
        case META_PACKET_DATA:
            data = (struct meta_data_msg far *) hdr;
            doomcom.datalength =
                dc->datalength - sizeof(struct meta_header);
            _fmemmove(&doomcom.data, data->data, doomcom.datalength);
            _fmemcpy(addr, hdr->src, sizeof(node_addr_t));
            doomcom.remotenode = NodeForAddr(addr);
            return doomcom.remotenode > 0;

        case META_PACKET_DISCOVER:
            HandleDiscover((struct meta_discover_msg far *) hdr);
            break;

        case META_PACKET_STATUS:
            break;
    }

    // Keep processing more packets.
    return 0;
}

static void GetPacket(void)
{
    int i;

    for (;;)
    {
        for (i = 0; i < num_drivers; ++i)
        {
            if (GetAndForward(i))
            {
                break;
            }
        }
        if (i >= num_drivers)
        {
            doomcom.remotenode = -1;
            return;
        }

        if (HandlePacket(drivers[i]))
        {
            return;
        }
    }
}

static void DiscoverNodes(void)
{
    
}

void interrupt NetISR(void)
{
}

int main(int argc, char *argv[])
{
    doomcom_t far *d;
    char **args;

    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);

    for (;;)
    {
        d = NetLocateDoomcom(args);
        if (d == NULL)
        {
            break;
        }

        assert(num_drivers < MAXDRIVERS);
        drivers[num_drivers] = d;
        ++num_drivers;
    }

    return 0;
}

