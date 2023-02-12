
// DOS API code for accessing Winsock functions via VxD backdoors.

#define AF_INET         2

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

// Socket handles are actually pointers rather than file handles like they
// are on most Unix systems; a long can always store a pointer.
typedef long SOCKET;

void WinsockInit(void);

SOCKET WS_socket(int domain, int type, int protocol);
int WS_close(SOCKET socket);

