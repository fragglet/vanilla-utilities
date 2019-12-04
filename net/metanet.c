
#include <stdlib.h>
#include <time.h>
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

#define ADDR_DRIVER(x)  (((x) >> 5) & 0x1f)
#define ADDR_NODE(x)    ((x) & 0x1f)
#define MAKE_ADDRESS(driver, node)  (((driver) << 5) | node)

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
    struct meta_header header;
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
        hdr->src[0] = MAKE_ADDRESS(driver_index, dc->remotenode);
        // Packet for ourself?
        if (hdr->dest == 0)
        {
            return 1;
        }
        // This is a forwarding packet.
        // Decode next hop from first byte of routing dest:
        ddriver = ADDR_DRIVER(hdr->dest[0]);
        dnode = ADDR_NODE(hdr->dest[0]);
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
        // No more packets?
        if (i >= num_drivers)
        {
            doomcom.remotenode = -1;
            return;
        }

        if (HandlePacket(drivers[i]))
        {
            // Got a data packet and populated doomcom
            return;
        }
    }
}

static void SendDiscover(unsigned int node)
{
    doomcom_t far *dc;
    struct meta_discover_msg far *dsc;
    unsigned int first_hop;
    int d, n;

    first_hop = node_addrs[node][0];
    dc = drivers[ADDR_DRIVER(first_hop)];
    dsc = (struct meta_discover_msg far *) &dc->data;
    dsc->header.magic = META_MAGIC | META_PACKET_DISCOVER;
    _fmemset(&dsc->header.src, 0, sizeof(node_addr_t));
    _fmemcpy(&dsc->header.dest, node_addrs[node] + 1,
             sizeof(node_addr_t) - 1);
    dsc->header.dest[sizeof(node_addr_t) - 1] = 0;

    dsc->status = 0; // TODO
    dsc->num_neighbors = 0;

    for (d = 0; d < num_drivers; ++d)
    {
        // Don't include neighbors on the same interface as the node we're
        // sending to, because they're already accessible on the same
        // interface and including them could cause a routing loop.
        // The destination only cares about what can be accessed *through*
        // this node.
        if (d == ADDR_DRIVER(first_hop))
        {
            continue;
        }

        for (n = 1; n < drivers[d]->numnodes; ++n)
        {
            if (dsc->num_neighbors < MAXNETNODES)
            {
                dsc->neighbors[dsc->num_neighbors] = MAKE_ADDRESS(d, n);
                ++dsc->num_neighbors;
            }
        }
    }

    // Send packet.
    dc->datalength = sizeof(struct meta_discover_msg);
    dc->remotenode = ADDR_NODE(first_hop);
    NetSendPacket(dc);
}

static void SendPacket(void)
{
    doomcom_t far *dc;
    struct meta_data_msg far *msg;
    int first_hop;

    if (doomcom.remotenode < 0 || doomcom.remotenode >= num_nodes)
    {
        return;
    }

    // First entry in node_addr is the first hop
    first_hop = node_addrs[doomcom.remotenode][0];
    dc = drivers[ADDR_DRIVER(first_hop)];
    dc->remotenode = ADDR_NODE(first_hop);

    dc->datalength = sizeof(struct meta_header) + doomcom.datalength;
    msg = (struct meta_data_msg far *) &dc->data;
    msg->header.magic = META_MAGIC | META_PACKET_DATA;
    _fmemset(&msg->header.src, 0, sizeof(node_addr_t));
    _fmemcpy(&msg->header.dest, node_addrs[doomcom.remotenode] + 1,
             sizeof(node_addr_t) - 1);
    msg->header.dest[sizeof(node_addr_t) - 1] = 0;
    _fmemcpy(&msg->data, doomcom.data, doomcom.datalength);

    NetSendPacket(dc);
}

static void DiscoverNodes(void)
{
    unsigned int i, d, n;
    clock_t now, last_send = 0;

    num_nodes = 1;
    memset(node_addrs, 0, sizeof(node_addrs));

    for (d = 0; d < num_drivers; ++d)
    {
        for (n = 1; n < drivers[d]->numnodes; ++n)
        {
            if (num_nodes < MAXNETNODES)
            {
                node_addrs[num_nodes][0] = MAKE_ADDRESS(d, n);
                ++num_nodes;
            }
        }
    }

    for (;;)
    {
        GetPacket();

        now = clock();
        if (now - last_send > CLOCKS_PER_SEC)
        {
            for (i = 1; i < num_nodes; ++i)
            {
                SendDiscover(i);
            }
            last_send = now;
        }
    }
}

void interrupt NetISR(void)
{
    switch (doomcom.command)
    {
        case CMD_SEND:
            SendPacket();
            break;

        case CMD_GET:
            GetPacket();
            break;
    }
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

