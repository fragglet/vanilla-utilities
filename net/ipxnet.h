#include "lib/inttypes.h"

//===========================================================================

#define NUMPACKETS      10      // max outstanding packets before loss

// setupdata_t is used as doomdata_t during setup
typedef struct {
    int16_t gameid;               // so multiple games can setup at once
    int16_t drone;
    int16_t nodesfound;
    int16_t nodeswanted;
    // xttl extensions:
    int16_t dupwanted;
    int16_t plnumwanted;
} setupdata_t;

typedef struct {
    uint16_t PacketCheckSum;       /* high-low */
    uint16_t PacketLength;         /* high-low */
    uint8_t PacketTransportControl;
    uint8_t PacketType;

    uint8_t dNetwork[4];           /* high-low */
    uint8_t dNode[6];              /* high-low */
    uint8_t dSocket[2];            /* high-low */

    uint8_t sNetwork[4];           /* high-low */
    uint8_t sNode[6];              /* high-low */
    uint8_t sSocket[2];            /* high-low */
} ipx_header_t;

typedef struct {
    uint8_t network[4];            /* high-low */
    uint8_t node[6];               /* high-low */
} localaddr_t;

typedef struct {
    uint8_t node[6];               /* high-low */
} nodeaddr_t;

// time is used by the communication driver to sequence packets returned
// to DOOM when more than one is waiting

typedef struct {
    ipx_header_t ipx;

    int32_t time;
    uint8_t payload[512];
} packet_t;

extern long ipx_localtime;          // for time stamp in packets
extern const nodeaddr_t broadcast_addr;

void IPXRegisterFlags(void);
void InitNetwork(void);
void ShutdownNetwork(void);
void IPXGetLocalAddress(localaddr_t *addr);
void IPXSendPacket(const nodeaddr_t *addr, void *data, size_t data_len);
void IPXReleasePacket(packet_t *packet);
packet_t *IPXGetPacket(void);
unsigned short ShortSwap(unsigned short i);

