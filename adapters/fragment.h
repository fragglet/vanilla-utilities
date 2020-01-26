
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

