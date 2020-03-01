// ipxnet.h

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

typedef struct IPXPacketStructure {
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
} IPXPacket;

typedef struct {
    uint8_t network[4];            /* high-low */
    uint8_t node[6];               /* high-low */
} localadr_t;

typedef struct {
    uint8_t node[6];               /* high-low */
} nodeadr_t;

typedef struct ECBStructure {
    uint16_t Link[2];              /* offset-segment */
    uint16_t ESRAddress[2];        /* offset-segment */
    uint8_t InUseFlag;
    uint8_t CompletionCode;
    uint16_t ECBSocket;            /* high-low */
    uint8_t IPXWorkspace[4];       /* N/A */
    uint8_t DriverWorkspace[12];   /* N/A */
    uint8_t ImmediateAddress[6];   /* high-low */
    uint16_t FragmentCount;        /* low-high */

    uint16_t fAddress[2];          /* offset-segment */
    uint16_t fSize;                /* low-high */
    uint16_t f2Address[2];         /* offset-segment */
    uint16_t f2Size;               /* low-high */
} ECB;

// time is used by the communication driver to sequence packets returned
// to DOOM when more than one is waiting

typedef struct {
    ECB ecb;
    IPXPacket ipx;

    int32_t time;
    uint8_t payload[512];
} packet_t;

extern doomcom_t doomcom;
extern int gameid;

extern nodeadr_t nodeadr[MAXNETNODES + 1];
extern int localnodenum;

extern long ipx_localtime;          // for time stamp in packets
extern long ipx_remotetime;         // timestamp of last packet gotten

extern nodeadr_t remoteadr;

void IPXRegisterFlags(void);
void InitNetwork(void);
void ShutdownNetwork(void);
void SendPacket(int destination);
int GetPacket(void);

void PrintAddress(nodeadr_t *adr, char *str);
