
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

#define WSAEINTR                (WSABASEERR+4)
#define WSAEBADF                (WSABASEERR+9)
#define WSAEACCES               (WSABASEERR+13)
#define WSAEFAULT               (WSABASEERR+14)
#define WSAEINVAL               (WSABASEERR+22)
#define WSAEMFILE               (WSABASEERR+24)
#define WSAEWOULDBLOCK          (WSABASEERR+35)
#define WSAEINPROGRESS          (WSABASEERR+36)
#define WSAEALREADY             (WSABASEERR+37)
#define WSAENOTSOCK             (WSABASEERR+38)
#define WSAEDESTADDRREQ         (WSABASEERR+39)
#define WSAEMSGSIZE             (WSABASEERR+40)
#define WSAEPROTOTYPE           (WSABASEERR+41)
#define WSAENOPROTOOPT          (WSABASEERR+42)
#define WSAEPROTONOSUPPORT      (WSABASEERR+43)
#define WSAESOCKTNOSUPPORT      (WSABASEERR+44)
#define WSAEOPNOTSUPP           (WSABASEERR+45)
#define WSAEPFNOSUPPORT         (WSABASEERR+46)
#define WSAEAFNOSUPPORT         (WSABASEERR+47)
#define WSAEADDRINUSE           (WSABASEERR+48)
#define WSAEADDRNOTAVAIL        (WSABASEERR+49)
#define WSAENETDOWN             (WSABASEERR+50)
#define WSAENETUNREACH          (WSABASEERR+51)
#define WSAENETRESET            (WSABASEERR+52)
#define WSAECONNABORTED         (WSABASEERR+53)
#define WSAECONNRESET           (WSABASEERR+54)
#define WSAENOBUFS              (WSABASEERR+55)
#define WSAEISCONN              (WSABASEERR+56)
#define WSAENOTCONN             (WSABASEERR+57)
#define WSAESHUTDOWN            (WSABASEERR+58)
#define WSAETOOMANYREFS         (WSABASEERR+59)
#define WSAETIMEDOUT            (WSABASEERR+60)
#define WSAECONNREFUSED         (WSABASEERR+61)
#define WSAELOOP                (WSABASEERR+62)
#define WSAENAMETOOLONG         (WSABASEERR+63)
#define WSAEHOSTDOWN            (WSABASEERR+64)
#define WSAEHOSTUNREACH         (WSABASEERR+65)
#define WSAENOTEMPTY            (WSABASEERR+66)
#define WSAEPROCLIM             (WSABASEERR+67)
#define WSAEUSERS               (WSABASEERR+68)
#define WSAEDQUOT               (WSABASEERR+69)
#define WSAESTALE               (WSABASEERR+70)
#define WSAEREMOTE              (WSABASEERR+71)

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

#define htonl(x) \
    ((((x) & 0xff000000UL) >> 24) \
   | (((x) & 0x00ff0000UL) >> 8) \
   | (((x) & 0x0000ff00UL) << 8) \
   | (((x) & 0x000000ffUL) << 24))

#define htons(x) \
    ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))

#define ntohl(x) htonl(x)
#define ntohs(x) htons(x)

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

SOCKET socket(int domain, int type, int protocol);
int closesocket(SOCKET socket);

int bind(SOCKET socket, struct sockaddr_in far *addr);

ssize_t sendto(SOCKET socket, const void far *msg, size_t len, int flags,
               const struct sockaddr_in far *to);
ssize_t recvfrom(SOCKET socket, void far *buf, size_t len, int flags,
                 struct sockaddr_in far *from);
int ioctlsocket(SOCKET socket, unsigned long cmd, void far *value);

int inet_aton(const char *cp, struct in_addr *inp);

extern unsigned long WS_LastError;
