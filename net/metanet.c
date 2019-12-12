
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <bios.h>
#include "lib/inttypes.h"

#include "lib/flag.h"
#include "net/doomnet.h"

#define MAXDRIVERS 16

// Magic number field overlaps with the normal Doom checksum field.
// We ignore the top nybble so we can ignore NCMD_SETUP packets from
// the underlying driver, and use the bottom nybble for packet type.
#define META_MAGIC        0x01C36440L
#define META_MAGIC_MASK   0x0FFFFFF0L

#define NODE_STATUS_GOT_DISCOVER  0x01
#define NODE_STATUS_READY         0x02
#define NODE_STATUS_LAUNCHED      0x04

#define ADDR_DRIVER(x)  (((x) >> 5) & 0x1f)
#define ADDR_NODE(x)    ((x) & 0x1f)
#define MAKE_ADDRESS(driver, node)  (((driver) << 5) | node)

// A node_addr_t is a routing path to take through the network
// to reach the destination node. Each byte specifies the driver#
// and node# for next hop. There can be up to four hops.
typedef uint8_t node_addr_t[4];

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
    uint8_t data[1];  // [...]
};

struct meta_discover_msg
{
    struct meta_header header;
    unsigned int status;
    int num_neighbors;
    int station_id;
    // Next hop address byte for all immediate neighbors:
    uint8_t neighbors[MAXNETNODES];
};

struct node_data
{
    node_addr_t addr;
    uint8_t flags;
    int station_id;
};

static doomcom_t far *drivers[MAXDRIVERS];
static int num_drivers = 0;

static struct node_data nodes[MAXNETNODES];
static int num_nodes = 1;

static unsigned int stats_rx_packets, stats_tx_packets;
static unsigned int stats_wrong_magic, stats_too_many_hops, stats_invalid_dest;
static unsigned int stats_node_limit, stats_bad_send, stats_unknown_type;
static unsigned int stats_forwarded, stats_unknown_src, stats_setup_packets;

static const struct
{
    unsigned int *ptr;
    const char *name;
} stats[] = {
    { &stats_rx_packets,    "rx_packets" },
    { &stats_tx_packets,    "tx_packets" },
    { &stats_wrong_magic,   "wrong_magic" },
    { &stats_too_many_hops, "too_many_hops" },
    { &stats_invalid_dest,  "invalid_dest" },
    { &stats_node_limit,    "node_limit" },
    { &stats_bad_send,      "bad_send" },
    { &stats_unknown_type,  "unknown_type" },
    { &stats_forwarded,     "forwarded" },
    { &stats_unknown_src,   "unknown_src" },
    { &stats_setup_packets, "setup_packets" },
};

static void far_memmove(void far *dest, void far *src, size_t nbytes)
{
    uint8_t far *dest_p = (uint8_t far *) dest;
    uint8_t far *src_p = (uint8_t far *) src;
    int i;

    if (dest < src)
    {
        for (i = 0; i < nbytes; ++i)
        {
            *dest_p = *src_p;
            ++dest_p; ++src_p;
        }
    }
    else
    {
        dest_p += nbytes - 1;
        src_p += nbytes - 1;
        for (i = 0; i < nbytes; ++i)
        {
            *dest_p = *src_p;
            --dest_p; --src_p;
        }
    }
}

static void far_bzero(void far *dest, size_t nbytes)
{
    uint8_t far *dest_p = dest;
    int i;

    for (i = 0; i < nbytes; ++i)
    {
        *dest_p = 0;
        ++dest_p;
    }
}

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
        if ((hdr->magic & ~NCMD_CHECKSUM) == NCMD_SETUP)
        {
            ++stats_setup_packets;
            continue;
        }
        if ((hdr->magic & META_MAGIC_MASK) != META_MAGIC)
        {
            ++stats_wrong_magic;
            continue;
        }
        // Update src address to include return path; this is needed
        // even if we are delivering the packet to ourself so we get
        // an accurate source address.
        if (hdr->src[sizeof(hdr->src) - 1] != 0)
        {
            ++stats_too_many_hops;
            continue;
        }
        far_memmove(hdr->src + 1, hdr->src, sizeof(hdr->src) - 1);
        hdr->src[0] = MAKE_ADDRESS(driver_index, dc->remotenode);
        // Packet for ourself?
        if (hdr->dest[0] == 0)
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
            ++stats_invalid_dest;
            continue;
        }
        // Update destination.
        far_memmove(hdr->dest, hdr->dest + 1, sizeof(hdr->dest) - 1);
        hdr->dest[sizeof(hdr->dest) - 1] = 0;

        // Copy into destination driver's buffer and send.
        drivers[ddriver]->datalength = dc->datalength;
        drivers[ddriver]->remotenode = dnode;
        far_memmove(&drivers[ddriver]->data, dc->data, dc->datalength);
        NetSendPacket(drivers[ddriver]);
        ++stats_forwarded;
    }

    return 0;
}

static int NodeForAddr(node_addr_t addr)
{
    unsigned int i;

    for (i = 0; i < num_nodes; ++i)
    {
        if (!memcmp(&nodes[i].addr, addr, sizeof(node_addr_t)))
        {
            return i;
        }
    }
    return -1;
}

static int AppendNextHop(node_addr_t addr, uint8_t next_hop)
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
    ++stats_too_many_hops;
    return 0;
}

static struct node_data *NodeOrAddNode(node_addr_t addr)
{
    struct node_data *result;
    int node_idx;

    node_idx = NodeForAddr(addr);
    if (node_idx >= 0)
    {
        return &nodes[node_idx];
    }
    else if (num_nodes >= MAXNETNODES)
    {
        ++stats_node_limit;
        return NULL;
    }

    result = &nodes[num_nodes];
    ++num_nodes;
    memcpy(&result->addr, addr, sizeof(node_addr_t));

    return result;
}

static void SendDiscover(struct node_data *node)
{
    doomcom_t far *dc;
    struct meta_discover_msg far *dsc;
    unsigned int first_hop;
    int d, n;

    first_hop = node->addr[0];
    dc = drivers[ADDR_DRIVER(first_hop)];
    dsc = (struct meta_discover_msg far *) &dc->data;
    dsc->header.magic = META_MAGIC | (unsigned long) META_PACKET_DISCOVER;
    far_bzero(dsc->header.src, sizeof(node_addr_t));
    far_memmove(dsc->header.dest, node->addr + 1, sizeof(node_addr_t) - 1);
    dsc->header.dest[sizeof(node_addr_t) - 1] = 0;

    dsc->status = nodes[0].flags;
    dsc->num_neighbors = 0;
    dsc->station_id = nodes[0].station_id;

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

static void HandleDiscover(struct meta_discover_msg far *dsc)
{
    struct node_data *node;
    static node_addr_t addr;
    int i;

    far_memmove(addr, dsc->header.src, sizeof(node_addr_t));

    node = NodeOrAddNode(addr);
    if (node == NULL)
    {
        return;
    }

    if ((nodes[0].flags & NODE_STATUS_LAUNCHED) != 0
     && (dsc->status & NODE_STATUS_LAUNCHED) == 0)
    {
	// We've launched but this other node hasn't, and is still sending us
	// discover messages and has fallen behind. Since we're no longer in
	// discovery phase, just send it a tit-for-tat response and return;
	// if it's waiting on us, this will allow it to proceed.
        SendDiscover(node);
        return;
    }

    node->flags = dsc->status | NODE_STATUS_GOT_DISCOVER;
    node->station_id = dsc->station_id;

    for (i = 0; i < dsc->num_neighbors && i < MAXNETNODES; ++i)
    {
        far_memmove(addr, dsc->header.src, sizeof(node_addr_t));

        if (AppendNextHop(addr, dsc->neighbors[i]))
        {
            NodeOrAddNode(addr);
        }
    }
}

static int HandlePacket(doomcom_t far *dc)
{
    struct meta_header far *hdr;
    struct meta_data_msg far *msg;
    static node_addr_t addr;

    hdr = (struct meta_header far *) dc->data;

    switch (hdr->magic & ~META_MAGIC_MASK)
    {
        case META_PACKET_DATA:
            msg = (struct meta_data_msg far *) dc->data;
            far_memmove(&addr, &msg->header.src, sizeof(node_addr_t));
            doomcom.remotenode = NodeForAddr(addr);
            if (doomcom.remotenode < 0)
            {
                ++stats_unknown_src;
                return 0;
            }
            doomcom.datalength = dc->datalength - sizeof(struct meta_header);
            far_memmove(doomcom.data, msg->data, doomcom.datalength);
            ++stats_rx_packets;
            return 1;

        case META_PACKET_DISCOVER:
            HandleDiscover((struct meta_discover_msg far *) hdr);
            break;

        default:
            ++stats_unknown_type;
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

static void SendPacket(void)
{
    doomcom_t far *dc;
    struct meta_data_msg far *msg;
    struct node_data *node;
    int first_hop;

    if (doomcom.remotenode < 0 || doomcom.remotenode >= num_nodes)
    {
        ++stats_bad_send;
        return;
    }

    // First entry in node_addr is the first hop
    node = &nodes[doomcom.remotenode];
    first_hop = node->addr[0];
    dc = drivers[ADDR_DRIVER(first_hop)];
    dc->remotenode = ADDR_NODE(first_hop);

    dc->datalength = sizeof(struct meta_header) + doomcom.datalength;
    msg = (struct meta_data_msg far *) dc->data;
    msg->header.magic = META_MAGIC | (unsigned long) META_PACKET_DATA;
    far_bzero(msg->header.src, sizeof(node_addr_t));
    far_bzero(msg->header.dest, sizeof(node_addr_t));
    far_memmove(msg->header.dest, node->addr + 1,
                sizeof(node_addr_t) - 1);
    far_memmove(msg->data, doomcom.data, doomcom.datalength);

    NetSendPacket(dc);
    ++stats_tx_packets;
}

static void InitNodes(void)
{
    unsigned int d, n;

    num_nodes = 1;
    memset(nodes, 0, sizeof(nodes));
    nodes[0].station_id = rand();
    nodes[0].flags = NODE_STATUS_GOT_DISCOVER;

    // Initially we're only aware of those nodes which are our
    // immediate neighbors.
    for (d = 0; d < num_drivers; ++d)
    {
        for (n = 1; n < drivers[d]->numnodes; ++n)
        {
            if (num_nodes < MAXNETNODES)
            {
                nodes[num_nodes].addr[0] = MAKE_ADDRESS(d, n);
                ++num_nodes;
            }
        }
    }
}

static int CompareStationIDs(const void *_a, const void *_b)
{
    const struct node_data *a = *((struct node_data **) _a),
                           *b = *((struct node_data **) _b);
    if (a->station_id < b->station_id)
    {
        return -1;
    }
    else if (a->station_id > b->station_id)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static void AssignPlayerNumber(void)
{
    struct node_data *players[MAXNETNODES];
    int i;

    for (i = 0; i < num_nodes; ++i)
    {
        players[i] = &nodes[i];
    }

    qsort(players, num_nodes, sizeof(struct node_data *),
          CompareStationIDs);

    for (i = 0; i < num_nodes; ++i)
    {
        if (players[i] == &nodes[0])
        {
            doomcom.consoleplayer = i;
            break;
        }
    }

    doomcom.numnodes = num_nodes;
    doomcom.numplayers = num_nodes;
    doomcom.ticdup = 1;
    doomcom.extratics = 0;
}

static int CheckReady(void)
{
    int i, j;
    int got_all_discovers = 1;

    for (i = 0; i < num_nodes; ++i)
    {
        if ((nodes[i].flags & NODE_STATUS_GOT_DISCOVER) == 0)
        {
            got_all_discovers = 0;
            continue;
        }
        for (j = i + 1; j < num_nodes; ++j)
        {
            if ((nodes[j].flags & NODE_STATUS_GOT_DISCOVER) == 0)
            {
                continue;
            }
            if (nodes[i].station_id == nodes[j].station_id)
            {
                fprintf(stderr, "Two nodes have the same station ID!\n"
                                "Node %d and %d both have station ID %d\n",
                                i, j, nodes[i].station_id);
                exit(1);
            }
        }
    }

    if (got_all_discovers)
    {
        nodes[0].flags |= NODE_STATUS_READY;
    }

    return got_all_discovers;
}

static void DiscoverNodes(void)
{
    clock_t now, last_send = 0, ready_start = 0;
    int i;

    InitNodes();

    do
    {
        GetPacket();

        now = clock();
        if (now - last_send > CLOCKS_PER_SEC)
        {
            for (i = 1; i < num_nodes; ++i)
            {
                SendDiscover(&nodes[i]);
            }
            last_send = now;
        }

        if (ready_start == 0 && CheckReady())
        {
            ready_start = now + CLOCKS_PER_SEC;
        }
    } while (ready_start == 0 || now < ready_start);

    // Discover phase has finished but we may still receive discover
    // messages from other nodes if they haven't (Byzantine Generals
    // problem). We use the LAUNCHED flag to resolve this.
    nodes[0].flags |= NODE_STATUS_LAUNCHED;
}

static void PrintStats(void)
{
    int i;

    printf("\n\n");
    for (i = 0; i < sizeof(stats) / sizeof(*stats); ++i)
    {
        if (*stats[i].ptr != 0)
        {
            printf("%16s %6d\n", stats[i].name, *stats[i].ptr);
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
    unsigned int entropy;

    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);

    entropy = biostime(0, 0);

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

        // So that different drivers started at the same time
        // do not generate the same random seed.
        entropy ^= (entropy << 8) | (d->numnodes << 4)  | d->consoleplayer;
    }

    srand(entropy);

    DiscoverNodes();
    AssignPlayerNumber();
    LaunchDOOM(args);

    PrintStats();

    return 0;
}

