/*
 *
 * Ethernet Interface
 */

extern byte *_eth_FormatPacket(), *_eth_WaitPacket();

typedef struct ether {
    byte	dest[6];       /* destination ethernet address */
    byte	src[6];        /* source ethernet address */
    word	type;
    byte	data[ 1500 ];
};


#define ETH_MIN	60              /* Minimum Ethernet packet size */
