
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

#define WSABASEERR      10000
#define WSAEWOULDBLOCK  (WSABASEERR + 35)
#define WSAEOPNOTSUPP   (WSABASEERR + 45)

#define IOCPARM_MASK    0x7fUL          // parameters must be < 128 bytes
#define IOC_VOID        0x20000000UL    // no parameters
#define IOC_OUT         0x40000000UL    // copy out parameters
#define IOC_IN          0x80000000UL    // copy in parameters
#define IOC_INOUT       (IOC_IN|IOC_OUT)
#define _IO(x,y)        (IOC_VOID|((x)<<8)|(y))
#define _IOR(x,y,t)     (IOC_OUT|((sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))
#define _IOW(x,y,t)     (IOC_IN|((sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))

// ioctls:
#define FIONREAD    _IOR('f', 127, long)    // get # bytes to read
#define FIONBIO     _IOW('f', 126, long)    // set/clear non-blocking i/o
#define SIOCSHIWAT  _IOW('s',  0, long)     // set high watermark
#define SIOCGHIWAT  _IOR('s',  1, long)     // get high watermark
#define SIOCSLOWAT  _IOW('s',  2, long)     // set low watermark
#define SIOCGLOWAT  _IOR('s',  3, long)     // get low watermark
#define SIOCATMARK  _IOR('s',  7, long)     // at oob mark?

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
int WS_ioctlsocket(SOCKET socket, unsigned long cmd, void far *value);

unsigned long WS_htonl(unsigned long val);
#define WS_ntohl WS_htonl
unsigned short WS_htons(unsigned short val);
#define WS_ntohs WS_htons
