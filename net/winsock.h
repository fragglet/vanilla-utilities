
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
#define INADDR_LOOPBACK   0x7f000001UL  // 127.0.0.1
#define INADDR_BROADCAST  0xffffffffUL

#define WSABASEERR      10000
#define WSAEWOULDBLOCK  (WSABASEERR + 35)
#define WSAENOTSOCK     (WSABASEERR + 38)
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

#define WS_htonl(x) \
    ((((x) & 0xff000000UL) >> 24) \
   | (((x) & 0x00ff0000UL) >> 8) \
   | (((x) & 0x0000ff00UL) << 8) \
   | (((x) & 0x000000ffUL) << 24))

#define WS_htons(x) \
    ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))

#define WS_ntohl(x) WS_htonl(x)
#define WS_ntohs(x) WS_htons(x)

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
int WS_closesocket(SOCKET socket);

int WS_bind(SOCKET socket, struct sockaddr_in far *addr);

ssize_t WS_sendto(SOCKET socket, const void far *msg, size_t len, int flags,
                  const struct sockaddr_in far *to);
ssize_t WS_recvfrom(SOCKET socket, void far *buf, size_t len, int flags,
                    struct sockaddr_in far *from);
int WS_ioctlsocket(SOCKET socket, unsigned long cmd, void far *value);

int WS_inet_aton(const char *cp, struct in_addr *inp);

extern unsigned long WS_LastError;
