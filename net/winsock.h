
// DOS API code for accessing Winsock functions via VxD backdoors.

#define AF_INET         2

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

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

ssize_t WS_sendto(SOCKET socket, const void *msg, size_t len, int flags,
                  const struct sockaddr_in *to);
ssize_t WS_recvfrom(SOCKET socket, void *buf, size_t len, int flags,
                    struct sockaddr_in *from);
