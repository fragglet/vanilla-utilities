
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <bios.h>
#include "lib/inttypes.h"

#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"

#define MAXDRIVERS 16

static void FlushBroadcastPending(void);

// Magic number field overlaps with the normal Doom checksum field.
// We ignore the top nybble so we can ignore NCMD_SETUP packets from
// the underlying driver, and use the bottom nybble for packet type.
#define META_MAGIC        0x01C36440L
#define META_MAGIC_MASK   0x0FFFFFF0L

#define NODE_STATUS_GOT_DISCOVER  0x01
#define NODE_STATUS_READY         0x02
#define NODE_STATUS_LAUNCHED      0x04
#define NODE_STATUS_FORWARDER     0x08

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
    // Assigned by AssignPlayerNumbers:
    int player_num;
};

static const node_addr_t broadcast = {0xff, 0xff, 0xff, 0xff};

// When we send packets, they sometimes get placed into the broadcast
// pending buffer to determine if they should be sent as broadcast packets
// to save bandwidth:
static doomcom_t bc_pending;
static int bc_pending_count;

static doomcom_t far *drivers[MAXDRIVERS];
static int num_drivers = 0;

static struct node_data nodes[MAXNETNODES];
static int num_nodes = 1;

// In forwarder mode, we don't launch or participate in the game, we just
// forward packets between interfaces.
static int forwarder = 0;

static unsigned long stats_rx_packets, stats_tx_packets;
static unsigned long stats_rx_broadcasts, stats_tx_broadcasts;
static unsigned long stats_wrong_magic, stats_too_many_hops, stats_invalid_dest;
static unsigned long stats_node_limit, stats_bad_send, stats_unknown_type;
static unsigned long stats_forwarded, stats_unknown_src, stats_setup_packets;

static const struct
{
    unsigned long *ptr;
    const char *name;
} stats[] = {
    { &stats_rx_packets,    "rx_packets" },
    { &stats_tx_packets,    "tx_packets" },
    { &stats_rx_broadcasts, "rx_broadcasts" },
    { &stats_tx_broadcasts, "tx_broadcasts" },
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

static int far_memcmp(void far *a, void far *b, size_t nbytes)
{
    uint8_t far *a_p = (uint8_t far *) a;
    uint8_t far *b_p = (uint8_t far *) b;
    int i;

    for (i = 0; i < nbytes; ++i)
    {
        if (*a_p != *b_p)
        {
            if (*a_p < *b_p)
            {
                return -1;
            }
            else
            {
                return 1;
            }
        }
        ++a_p; ++b_p;
    }
    return 0;
}

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

static void ForwardBroadcast(doomcom_t far *src)
{
    doomcom_t far *dest;
    int ddriver, dnode;

    for (ddriver = 0; ddriver < num_drivers; ++ddriver)
    {
        // We never broadcast back on the source address as it may
        // cause a routing loop.
        dest = drivers[ddriver];
        if (dest == src)
        {
            continue;
        }

        dest->datalength = src->datalength;
        far_memmove(dest->data, src->data, src->datalength);

        for (dnode = 1; dnode < dest->numnodes; ++dnode)
        {
            dest->remotenode = dnode;
            NetSendPacket(dest);
        }
    }
}

static void ForwardPacket(doomcom_t far *src)
{
    unsigned int ddriver, dnode;
    struct meta_header far *hdr;

    hdr = (struct meta_header far *) &src->data;

    // Decode next hop from first byte of routing dest:
    ddriver = ADDR_DRIVER(hdr->dest[0]);
    dnode = ADDR_NODE(hdr->dest[0]);
    if (ddriver >= num_drivers || dnode >= num_nodes
     || drivers[ddriver] == src || dnode == 0)
    {
        ++stats_invalid_dest;
        return;
    }
    // Update destination.
    far_memmove(hdr->dest, hdr->dest + 1, sizeof(hdr->dest) - 1);
    hdr->dest[sizeof(hdr->dest) - 1] = 0;

    // Copy into destination driver's buffer and send.
    drivers[ddriver]->datalength = src->datalength;
    drivers[ddriver]->remotenode = dnode;
    far_memmove(&drivers[ddriver]->data, src->data, src->datalength);
    NetSendPacket(drivers[ddriver]);
    ++stats_forwarded;
}

// Read packets from the given interface, forwarding them to other
// interfaces if destined for another machine, and returning true
// if a packet is received for this machine.
static int GetAndForward(int driver_index)
{
    doomcom_t far *dc = drivers[driver_index];
    struct meta_header far *hdr;

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
        if (far_memcmp(hdr->dest, broadcast, sizeof(node_addr_t)) == 0)
        {
            ForwardBroadcast(dc);
            ++stats_rx_broadcasts;
            return 1;
        }
        ForwardPacket(dc);
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

    // We want to ensure packets are never held up in the broadcast buffer
    // otherwise it may affect latency. Doom's NetUpdate() calls SendPacket
    // multiple times without calling GetPacket, so on call to GetPacket,
    // always flush any packets being held up.
    FlushBroadcastPending();

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

// SendFromBuffer reads a packet for sending from the given doomcom_t and
// sends it to the destination via the appropriate backend driver.
static void SendFromBuffer(doomcom_t *src)
{
    doomcom_t far *dc;
    struct meta_data_msg far *msg;
    struct node_data *node;
    int first_hop;

    // First entry in node_addr is the first hop
    node = &nodes[src->remotenode];
    first_hop = node->addr[0];
    dc = drivers[ADDR_DRIVER(first_hop)];
    dc->remotenode = ADDR_NODE(first_hop);

    dc->datalength = sizeof(struct meta_header) + src->datalength;
    msg = (struct meta_data_msg far *) dc->data;
    msg->header.magic = META_MAGIC | (unsigned long) META_PACKET_DATA;
    far_bzero(msg->header.src, sizeof(node_addr_t));
    far_bzero(msg->header.dest, sizeof(node_addr_t));
    far_memmove(msg->header.dest, node->addr + 1,
                sizeof(node_addr_t) - 1);
    far_memmove(msg->data, src->data, src->datalength);

    NetSendPacket(dc);
    ++stats_tx_packets;
}

// FlushBroadcastPending sends the packet waiting in bc_pending to all
// nodes for which it was held back - if any.
static void FlushBroadcastPending(void)
{
    int i;

    for (i = 0; i < bc_pending_count; ++i)
    {
        bc_pending.remotenode = i + 1;
        SendFromBuffer(&bc_pending);
    }

    bc_pending_count = 0;
}

// TryBroadcastStore tries to stage the packet in doomcom in bc_pending and
// returns 1 if the packet should not be sent yet.
static int TryBroadcastStore(void)
{
    // The objective here is to try to detect the specific sequence of calls
    // from Doom's NetUpdate() function - when a new tic is generated, it
    // does a transmit to each node of (usually) the exact same data.
    // We count up the number of such identical packets we have received and
    // when bc_pending_count == num_nodes - 1, we have successfully detected
    // something that can be sent as a broadcast packet.

    // If this looks like the first packet in a sequence, we store the packet
    // into the pending buffer.
    if (doomcom.remotenode == 1)
    {
        FlushBroadcastPending();
        bc_pending.datalength = doomcom.datalength;
        far_memmove(bc_pending.data, doomcom.data, doomcom.datalength);
        bc_pending_count = 0;
    }
    // The packet must exactly match the previous ones, and be in sequence.
    else if (doomcom.remotenode != bc_pending_count + 1
          || doomcom.datalength != bc_pending.datalength
          || memcmp(doomcom.data, bc_pending.data, bc_pending.datalength) != 0)
    {
        FlushBroadcastPending();
        return 0;
    }

    // We got the next in sequence successfully.
    ++bc_pending_count;
    if (bc_pending_count == num_nodes - 1)
    {
        // We have a complete broadcast packet. Prepend meta-header for send.
        struct meta_data_msg *msg;

        msg = (struct meta_data_msg *) bc_pending.data;
        memmove(msg->data, bc_pending.data, bc_pending.datalength);

        msg->header.magic = META_MAGIC | (unsigned long) META_PACKET_DATA;
        memset(msg->header.src, 0, sizeof(node_addr_t));
        memcpy(msg->header.dest, broadcast, sizeof(node_addr_t));
        bc_pending.datalength += sizeof(struct meta_header);

        // We reuse the ForwardBroadcast to send to all neighbors.
        ForwardBroadcast(&bc_pending);
        ++stats_tx_broadcasts;

        bc_pending_count = 0;
    }
    return 1;
}

static void SendPacket(void)
{
    if (doomcom.remotenode < 0 || doomcom.remotenode >= num_nodes)
    {
        ++stats_bad_send;
        return;
    }

    if (TryBroadcastStore())
    {
        return;
    }

    SendFromBuffer(&doomcom);
}

static void InitNodes(void)
{
    unsigned int d, n;

    num_nodes = 1;
    memset(nodes, 0, sizeof(nodes));
    nodes[0].station_id = rand();
    nodes[0].flags = NODE_STATUS_GOT_DISCOVER;

    if (forwarder)
    {
        nodes[0].flags |= NODE_STATUS_FORWARDER;
    }

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

static void SortNodes(struct node_data **result,
                      int (*func)(const void *a, const void *b))
{
    int i;

    for (i = 0; i < num_nodes; ++i)
    {
        result[i] = &nodes[i];
    }

    qsort(result, num_nodes, sizeof(struct node_data *), func);
}

static void RearrangeNodes(void)
{
    struct node_data tmp;
    int i, p;

    // Rearrange the nodes[] array so that forwarders are at the end.
    // We do this so that the next layer up does not know that there
    // are any forwarding nodes at all.
    p = num_nodes - 1;
    for (i = 1; i < p; ++i)
    {
        if ((nodes[i].flags & NODE_STATUS_FORWARDER) != 0)
        {
            memcpy(&tmp, &nodes[p], sizeof(struct node_data));
            memcpy(&nodes[p], &nodes[i], sizeof(struct node_data));
            memcpy(&nodes[i], &tmp, sizeof(struct node_data));
            --p;
        }
    }
}

static void AssignPlayerNumbers(void)
{
    struct node_data *players[MAXNETNODES];
    int i, num_players;

    SortNodes(players, CompareStationIDs);

    num_players = 0;
    for (i = 0; i < num_nodes; ++i)
    {
        if ((players[i]->flags & NODE_STATUS_FORWARDER) == 0)
        {
            players[i]->player_num = num_players;
            ++num_players;
        }
    }

    doomcom.consoleplayer = nodes[0].player_num;
    doomcom.numnodes = num_players;
    doomcom.numplayers = num_players;
    doomcom.ticdup = 1;
    doomcom.extratics = 0;
}

static int CompareAddrs(const void *_a, const void *_b)
{
    const struct node_data *a = *((struct node_data **) _a),
                           *b = *((struct node_data **) _b);
    return memcmp(a->addr, b->addr, sizeof(node_addr_t));
}

static char *NodeDescription(struct node_data *n)
{
    static char buf[20];
    if ((n->flags & NODE_STATUS_FORWARDER) != 0)
    {
        sprintf(buf, "Forwarder");
    }
    else
    {
        sprintf(buf, "Player %d", n->player_num + 1);
    }
    return buf;
}

static void PrintTopology(void)
{
    struct node_data *players[MAXNETNODES];
    char indent_buf[32];
    int driver;
    int last_first_hop;
    int il;
    int i;

    SortNodes(players, CompareAddrs);
    LogMessage("Discovered network topology:");
    LogMessage("  This machine (%s)", NodeDescription(&nodes[0]));

    last_first_hop = -1;
    for (i = 1; i < num_nodes; ++i)
    {
        driver = ADDR_DRIVER(players[i]->addr[0]);
        if (driver != ADDR_DRIVER(last_first_hop))
        {
            LogMessage("    \\ Driver %d:", driver);
        }
        for (il = 0; il < sizeof(node_addr_t); ++il)
        {
            if (players[i]->addr[il] == 0)
            {
                break;
            }
        }
        strncpy(indent_buf, "                             ",
                sizeof(indent_buf));
        indent_buf[il * 4 + 4] = '\0';
        LogMessage("%s\\ %s", indent_buf, NodeDescription(&nodes[i]));
        last_first_hop = players[i]->addr[0];
    }
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
                Error("Two nodes have the same station ID!\n"
                      "Node %d and %d both have station ID %d\n",
                      i, j, nodes[i].station_id);
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

    LogMessage("Discovering network topology.");
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

    LogMessage("Statistics:");
    for (i = 0; i < sizeof(stats) / sizeof(*stats); ++i)
    {
        if (*stats[i].ptr != 0)
        {
            LogMessage("%16s %6ld", stats[i].name, *stats[i].ptr);
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

static void RunForwarder(void)
{
    LogMessage("Entering packet forwarding mode.");

    // TODO: escape to abort
    for (;;)
    {
        GetPacket();
        // TODO: Print some statistics occasionally?
    }
}

static void AddDriver(long l)
{
    doomcom_t far *d = NetGetHandle(l);

    assert(num_drivers < MAXDRIVERS);
    drivers[num_drivers] = d;
    ++num_drivers;
}

static void SeedRandom(void)
{
    unsigned int entropy;
    int i;

    entropy = biostime(0, 0);

    for (i = 0; i < num_drivers; ++i)
    {
        // So that different drivers started at the same time
        // do not generate the same random seed.
        entropy ^= (entropy << 8)
                 | (drivers[i]->numnodes << 4)
                 | drivers[i]->consoleplayer;
    }

    srand(entropy);
}

int main(int argc, char *argv[])
{
    char **args;

    SetHelpText("Forwarding network-of-networks driver.",
                "sersetup -com1 sersetup -com2 %s doom.exe");
    APIPointerFlag("-net", AddDriver);
    BoolFlag("-forward", &forwarder,
             "Don't launch game, just forward packets.");
    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);

    if (forwarder)
    {
        if (args != NULL)
        {
            ErrorPrintUsage("Expecting no command in forwarder mode.");
        }
        if (num_drivers < 2)
        {
            ErrorPrintUsage("At least two drivers needed for forwarder mode.");
        }
    }
    else
    {
        if (args == NULL)
        {
            ErrorPrintUsage("No command given to run.");
        }
        if (num_drivers == 0)
        {
            ErrorPrintUsage("No drivers specified on command line.");
        }
    }

    SeedRandom();
    DiscoverNodes();
    RearrangeNodes();
    AssignPlayerNumbers();
    PrintTopology();

    if (forwarder)
    {
        RunForwarder();
    }
    else
    {
        LogMessage("Console is player %d of %d (%d nodes)",
                   doomcom.consoleplayer, doomcom.numplayers, num_nodes);
        LaunchDOOM(args);
    }

    PrintStats();

    return 0;
}

