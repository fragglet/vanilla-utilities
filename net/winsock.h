
// DOS API code for accessing Winsock functions via VxD backdoors.

#define AF_INET         2

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

#define INADDR_ANY        0x00000000UL
#define INADDR_BROADCAST  0xffffffffUL

struct in_addr {
    unsigned long s_addr;
};

struct sockaddr_in {
    unsigned short sin_family;  // Set this to AF_INET
    unsigned short sin_port;
    struct in_addr sin_addr;
};

// Socket handles are actually pointers rather than file handles like they
// are on most Unix systems; a long can always store a pointer.
typedef long SOCKET;

void WinsockInit(void);

SOCKET WS_socket(int domain, int type, int protocol);
int WS_close(SOCKET socket);

int WS_bind(SOCKET socket, struct sockaddr_in far *addr);

ssize_t WS_sendto(SOCKET socket, const void far *msg, size_t len, int flags,
                  const struct sockaddr_in far *to);
ssize_t WS_recvfrom(SOCKET socket, void far *buf, size_t len, int flags,
                    struct sockaddr_in far *from);

unsigned long WS_htonl(unsigned long val);
#define WS_ntohl WS_htonl
unsigned short WS_htons(unsigned short val);
#define WS_ntohs WS_htons
