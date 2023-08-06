// Library for doing DOS Winsock networking via VxD backdoors.
// This is some deep magic. Some recommended reading:
// * WSOCK.VXD pseudo-documentation (Richard Dawe)
//   <https://www.richdawe.be/archive/dl/wsockvxd.htm>
// * DJGPP libsocket, particularly the src/wsock directory (ls080s.zip).
// * Berczi Gabor's Winsock2 Pascal code (ws2dos.zip). VERY good explanation
//   of the bugs in WSOCK2.VXD, and the code is essential as a working example
//   of Winsock2 code.
// * Ralf Brown's Interrupt List, which has some documentation for the
//   Windows VxD backdoor APIs.
// * The book "Unauthorized Windows 95", Andrew Schulman. ISBN 978-1568841694
// * Berczi Gabor's (again) Freesock Pascal library, which has interfaces for
//   calling into pretty much every DOS networking API ever created.
//   MSSOCKS.PAS contains (some) details about the MSClient TCPIP driver API.
// * "Microsoft TCP/IP Sockets Development Kit", Microsoft's published SDK for
//   using the MSClient TCPIP driver, contains their official sockets API
//   implementation that calls into that driver. Some reverse engineering of
//   that helped to fill in the gaps missing from MSSOCKS.PAS.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <process.h>

#include "lib/dos.h"
#include "lib/log.h"
#include "net/llcall.h"
#include "net/winsock.h"

#pragma pack(1)

#define WSOCK_BIND_CMD              0x0101
#define WSOCK_CLOSESOCKET_CMD       0x0102
#define WSOCK_GETPEERNAME_CMD       0x0104
#define WSOCK_IOCTLSOCKET_CMD       0x0107
#define WSOCK_RECV_CMD              0x0109
#define WSOCK_SEND_CMD              0x010d
#define WSOCK_SOCKET_CMD            0x0110

#define SOCKADDR_SIZE 16 /* bytes = sizeof struct sockaddr */

// Windows type names:
typedef unsigned long DWORD;
typedef unsigned short WORD;

#define VXD_ID_VXDLDR    0x0027
#define VXD_ID_WSOCK     0x003e
#define VXD_ID_WSOCK2    0x3b0a

typedef void __stdcall far (*vxd_entrypoint)();

static enum { MSCLIENT, WINSOCK1, WINSOCK2 } stack;
unsigned long WS_LastError;

static vxd_entrypoint vxdldr_entry = NULL;
static vxd_entrypoint winsock_entry = NULL;

static int VxdGetEntryPoint(vxd_entrypoint *entrypoint, int id)
{
    union REGS inregs, outregs;
    struct SREGS sregs;

    inregs.x.ax = 0x1684;
    inregs.x.bx = id;
    int86x(0x2f, &inregs, &outregs, &sregs);

    if (sregs.es == 0 && outregs.x.di == 0)
    {
        return 0;
    }
    *entrypoint = MK_FP(sregs.es, outregs.x.di);
    return 1;
}

static int VXDLDR_LoadDevice(char *device)
{
    ll_funcptr = vxdldr_entry;
    ll_regs.x.ax = 0x0001;
    // DS is already equal to FP_SEG(device) (non-far ptr)
    ll_regs.x.dx = FP_OFF(device);
    LowLevelCall();
    return ll_regs.x.ax == 0;
}

static int VXDLDR_UnloadDevice(char *device)
{
    ll_funcptr = vxdldr_entry;
    ll_regs.x.ax = 0x0002;
    // DS is already equal to FP_SEG(device) (non-far ptr)
    ll_regs.x.dx = FP_OFF(device);
    LowLevelCall();
    return ll_regs.x.ax == 0;
}

static int WinsockCall(int function, void far *ptr)
{
    ll_funcptr = winsock_entry;
    ll_regs.x.ax = function;
    ll_regs.x.es = FP_SEG(ptr);
    ll_regs.x.bx = FP_OFF(ptr);
    LowLevelCall();
    WS_LastError = ll_regs.x.ax;
    return WS_LastError == 0 ? 0 : -1;
}

static SOCKET WS_socket(int domain, int type, int protocol)
{
    static DWORD handle_counter = 999900UL;
    struct {
        DWORD   AddressFamily;
        DWORD   SocketType;
        DWORD   Protocol;
        SOCKET  NewSocket;
        DWORD   NewSocketHandle;  // ___  ^ Winsock1 only uses these
        DWORD   ProtocolCatalogID;
        DWORD   GroupID;
        DWORD   Flags;
    } params;

    memset(&params, 0, sizeof(params));

    params.AddressFamily = domain;
    params.SocketType = type;
    params.Protocol = protocol;
    params.NewSocketHandle = handle_counter;
    ++handle_counter;

    if (WinsockCall(WSOCK_SOCKET_CMD, &params) < 0)
    {
        return -1;
    }
    return params.NewSocket;
}

static int WS_closesocket(SOCKET socket)
{
    struct {
        SOCKET Socket;
    } params;

    params.Socket = socket;

    return WinsockCall(WSOCK_CLOSESOCKET_CMD, &params);
}

static DWORD MapFlatPointer(const void far *msg)
{
    struct {
        const void far *Address;
        SOCKET          Socket;
        DWORD           AddressLength;
    } params;

    // Trick found inside ws2dos code:
    // We invoke the getpeername() VxD call, which will do the translation
    // to a flat address for us. The operation always results in an error
    // since params.Socket=0, but it does the translation for us.
    params.Socket = 0;
    params.Address = msg;
    params.AddressLength = 0;

    WinsockCall(WSOCK_GETPEERNAME_CMD, &params);

    return (DWORD) params.Address;
}

static int WS_bind(SOCKET socket, struct sockaddr_in far *addr)
{
    struct {
        const void far *Address;
        SOCKET    Socket;
        DWORD     AddressLength;
        void far *ApcRoutine;
        DWORD     ApcContext;
        DWORD     ConnFamily;
    } params;

    memset(&params, 0, sizeof(params));

    params.Socket = socket;
    params.Address = addr;
    params.AddressLength = SOCKADDR_SIZE;

    return WinsockCall(WSOCK_BIND_CMD, &params);
}

// Winsock1 version of sendto().
static ssize_t WS_sendto1(SOCKET socket, const void far *msg, size_t len,
                          int flags, const struct sockaddr_in far *to)
{
    struct {
        const void far *Buffer;
        const void far *Address;      // ___ ^ Addresses translated by VxD
        SOCKET          Socket;
        DWORD           BufferLength;
        DWORD           Flags;
        DWORD           AddressLength;
        DWORD           BytesSent;
        void far       *ApcRoutine;
        DWORD           ApcContext;
        DWORD           Timeout;
    } params;

    memset(&params, 0, sizeof(params));

    params.Buffer = msg;
    params.Address = to;
    params.Socket = socket;
    params.BufferLength = len;
    params.Flags = flags;
    params.AddressLength = SOCKADDR_SIZE;

    if (WinsockCall(WSOCK_SEND_CMD, &params) < 0)
    {
        return -1;
    }

    return params.BytesSent;
}

struct WSABuffer {
    DWORD Length;
    DWORD Address;
};

// Winsock2 version.
static ssize_t WS_sendto2(SOCKET socket, const void far *msg, size_t len,
                          int flags, const struct sockaddr_in far *to)
{
    struct WSABuffer buffer;
    struct {
        struct WSABuffer far *Buffers;
        const void far *Address;     // ___ ^ Addresses translated by VxD
        SOCKET          Socket;
        DWORD           BufferCount;
        void far       *AddrLenPtr; //?
        DWORD           Flags;
        DWORD           AddressLength;
        DWORD           BytesSent;
        void far       *ApcRoutine;
        DWORD           ApcContext;
        DWORD           Unknown[3];
    } params;

    // wsock2.vxd translates the params.Buffers pointer (below) into a flat
    // address when invoked, but does not translate the address inside the
    // WSABuffer that we point to. We therefore have to translate for it.
    buffer.Address = MapFlatPointer(msg);
    buffer.Length = len;

    memset(&params, 0, sizeof(params));

    params.Buffers = &buffer;
    params.Address = to;
    params.Socket = socket;
    params.BufferCount = 1;
    params.Flags = flags;
    params.AddressLength = SOCKADDR_SIZE;

    if (WinsockCall(WSOCK_SEND_CMD, &params) < 0)
    {
        return -1;
    }

    return params.BytesSent;
}

// Winsock1 version of recvfrom().
static ssize_t WS_recvfrom1(SOCKET socket, void far *buf, size_t len, int flags,
                            struct sockaddr_in far *from)
{
    static uint8_t frombuf[SOCKADDR_SIZE];
    struct {
        void far  *Buffer;
        void far  *Address;      // ___ ^ Addresses translated by VxD
        SOCKET     Socket;
        DWORD      BufferLength;
        DWORD      Flags;
        DWORD      AddressLength;
        DWORD      BytesReceived;
        void far  *ApcRoutine;
        DWORD      ApcContext;
        DWORD      Timeout;
    } params;

    memset(&params, 0, sizeof(params));

    params.Buffer = buf;
    params.Address = frombuf;
    params.Socket = socket;
    params.BufferLength = len;
    params.Flags = flags;
    params.AddressLength = SOCKADDR_SIZE;

    if (WinsockCall(WSOCK_RECV_CMD, &params) < 0)
    {
        return -1;
    }

    far_memcpy(from, frombuf, sizeof(struct sockaddr_in));

    return params.BytesReceived;
}

static ssize_t WS_recvfrom2(SOCKET socket, void far *buf, size_t len, int flags,
                            struct sockaddr_in far *from)
{
    static uint8_t frombuf[SOCKADDR_SIZE];
    struct WSABuffer buffer;
    DWORD unused = SOCKADDR_SIZE;  // for AddrLenPtr
    struct {
        struct WSABuffer far *Buffers;
        void far *Address;
        void far *AddrLenPtr;   // ___ ^ Addresses translated by VxD
        SOCKET    Socket;
        DWORD     BufferCount;
        DWORD     AddressLength;
        DWORD     Flags;
        DWORD     BytesReceived;
        void far *ApcRoutine;
        DWORD     ApcContext;
        DWORD     Unknown[2];
        DWORD     Overlapped;
    } params;

    // See comment in WS_sendto2 above.
    buffer.Address = MapFlatPointer(buf);
    buffer.Length = len;

    memset(&params, 0, sizeof(params));

    params.Buffers = &buffer;
    params.Address = frombuf;
    params.Socket = socket;
    params.BufferCount = 1;
    params.Flags = flags;
    params.AddressLength = SOCKADDR_SIZE;
    params.AddrLenPtr = &unused;

    if (WinsockCall(WSOCK_RECV_CMD, &params) < 0)
    {
        return -1;
    }

    far_memcpy(from, frombuf, sizeof(struct sockaddr_in));

    return params.BytesReceived;
}

static int WS_ioctlsocket1(SOCKET socket, unsigned long cmd, void far *value)
{
    struct {
        SOCKET  Socket;
        DWORD   Command;
        DWORD   Param;
    } params;

    if (cmd != FIONBIO && cmd != FIONREAD && cmd != SIOCATMARK)
    {
        return WSAEOPNOTSUPP;
    }

    memset(&params, 0, sizeof(params));

    params.Socket = socket;
    params.Command = cmd;
    params.Param = *((DWORD *) value);

    if (WinsockCall(WSOCK_IOCTLSOCKET_CMD, &params) < 0)
    {
        return -1;
    }

    *((DWORD *) value) = params.Param;

    return 0;
}

static int WS_ioctlsocket2(SOCKET socket, unsigned long cmd, void far *value)
{
    // To make sense of this, take a look at the documentation for WSPIoctl().
    struct {
        SOCKET    Socket;
        DWORD     Command;
        DWORD     ParamPointer;
        DWORD     Unknown1;      // lpvOutBuffer?
        DWORD     Unknown2;      // lpcbBytesReturned?
        DWORD     ParamBufLen;
        // This looks like it may be: cbOutBuffer, lpOverlapped,
        // lpCompletionRoutine, lpThreadId...
        DWORD     Unknown3[6];
    } params;

    memset(&params, 0, sizeof(params));

    params.Socket = socket;
    params.Command = cmd;
    params.ParamPointer = MapFlatPointer(value);
    params.ParamBufLen = sizeof(DWORD);

    return WinsockCall(WSOCK_IOCTLSOCKET_CMD, &params);
}

static void WinsockShutdown(void)
{
    if (vxdldr_entry != NULL)
    {
        VXDLDR_UnloadDevice("WSOCK.VXD");
        VXDLDR_UnloadDevice("WSOCK2.VXD");
    }
}

static void CheckWindowsVersion(void)
{
    union REGS inregs, outregs;

    inregs.x.ax = 0x160a;
    int86(0x2f, &inregs, &outregs);

    // Must be Windows 4.x (9x).
    if (outregs.x.ax != 0 || outregs.h.bh < 3 || outregs.x.cx != 3)
    {
        Error("This program only works under Windows 9x "
              "or Windows 3.x enhanced mode.");
    }

    // If this is Windows 3, the DOS version doesn't matter.
    if (outregs.h.bh == 3)
    {
        return;
    }

    // We don't allow NT. Check for DOS 7 (NT pretends to be DOS 5).
    // TODO: But we should support WSOCKVDD.
    inregs.h.ah = 0x30;
    inregs.h.al = 1;
    int86(0x21, &inregs, &outregs);

    switch (outregs.h.al)
    {
        case 5:
            Error("This program doesn't work under Windows NT, only "
                  "Windows 9x or Windows 3.x enhanced mode.");
        case 8:
            LogMessage("This hasn't been tested under Windows ME. "
                       "Let me know if it works.");
        case 7:
            // 95 or 98
            break;

        default:
            LogMessage("DOS %d?!?", outregs.h.al);
    }
}

struct mssock_header {
    uint8_t FuncCode;
    void far *StatusPtr;
    void far *ResultPtr;
    uint16_t ProcessID;
};

static const int msclient_to_winsock_err[] = {
    WSAENOTSOCK,         // 100 - Socket operation on non-socket
    WSAEDESTADDRREQ,     // 101 - Destination address required
    WSAEMSGSIZE,         // 102 - Message too long
    WSAEPROTOTYPE,       // 103 - Protocol wrong type for socket
    WSAENOPROTOOPT,      // 104 - Protocol not available
    WSAEPROTONOSUPPORT,  // 105 - Protocol not supported
    WSAESOCKTNOSUPPORT,  // 106 - Socket type not supported
    WSAEOPNOTSUPP,       // 107 - Operation not supported on socket
    WSAEPFNOSUPPORT,     // 108 - Protocol family not supported
    WSAEAFNOSUPPORT,     // 109 - Address family not supported
    WSAEADDRINUSE,       // 110 - Address already in use
    WSAEADDRNOTAVAIL,    // 111 - Can't assign requested address
    WSAENETDOWN,         // 112 - Network is down
    WSAENETUNREACH,      // 113 - Network is unreachable
    WSAENETRESET,        // 114 - Network dropped connection or reset
    WSAECONNABORTED,     // 115 - Software caused connection abort
    WSAECONNRESET,       // 116 - Connection reset by peer
    WSAENOBUFS,          // 117 - No buffer space available
    WSAEISCONN,          // 118 - Socket is already connected
    WSAENOTCONN,         // 119 - Socket is not connected
    WSAESHUTDOWN,        // 120 - Can't send after socket shutdown
    WSAETIMEDOUT,        // 121 - Connection timed out
    WSAECONNREFUSED,     // 122 - Connection refused
    WSAEHOSTDOWN,        // 123 - Networking subsystem not started
    WSAEHOSTUNREACH,     // 124 - No route to host
    WSAEWOULDBLOCK,      // 125 - Operation would block
    WSAEINPROGRESS,      // 126 - Operation now in progress
    WSAEALREADY,         // 127 - Operation already in progress

    // There are more msclient-specific errors that follow WSAEALREADY, but
    // they don't have winsock equivalents.
};

static uint16_t MSClientToWinsockErr(uint16_t msclient_err)
{
    uint16_t err;

    if (msclient_err == 0)
    {
        return 0;
    }

    if (msclient_err < 100 || msclient_err > 127)
    {
        // Can't map to a winsock error code, but at least translate it
        // into something unique.
        return 20000 + msclient_err;
    }

    return msclient_to_winsock_err[msclient_err - 100];
}

static int MSClientCall(int code, struct mssock_header *hdr)
{
    uint16_t status = 0;
    int16_t result = 0;

    hdr->FuncCode = code;
    hdr->StatusPtr = &status;
    hdr->ResultPtr = &result;
    hdr->ProcessID = getpid();

    // TODO...

    if (result < 0)
    {
        WS_LastError = MSClientToWinsockErr(status);
        return -1;
    }

    return result;
}

static SOCKET MSC_socket(int domain, int type, int protocol)
{
    struct {
        struct mssock_header Header;
        uint16_t             SockFamily;
        uint16_t             SockType;
        uint16_t             SockProtocol;
    } params;

    memset(&params, 0, sizeof(params));
    params.SockFamily = domain;
    params.SockType = type;
    params.SockProtocol = protocol;

    return MSClientCall(0x10, &params.Header);
}

static int MSC_closesocket(SOCKET socket)
{
    struct {
        struct mssock_header Header;
        uint16_t             Socket;
    } params;

    memset(&params, 0, sizeof(params));
    params.Socket = (uint16_t) socket;

    return MSClientCall(0x02, &params.Header);
}

static int MSC_bind(SOCKET socket, struct sockaddr_in far *addr)
{
    struct {
        struct mssock_header Header;
        uint16_t             Socket;
        void far            *Addr;
        uint16_t             AddrLen;
    } params;

    memset(&params, 0, sizeof(params));
    params.Socket = (uint16_t) socket;
    params.Addr = addr;
    params.AddrLen = sizeof(*addr);

    return MSClientCall(0x01, &params.Header);
}

static ssize_t MSC_sendto(SOCKET socket, const void far *msg, size_t len,
                          int flags, const struct sockaddr_in far *to)
{
    struct {
        struct mssock_header Header;
        uint16_t             Socket;
        const void far      *Buffer;
        uint16_t             BufferLen;
        uint16_t             Flags;
        const void far      *Addr;
        uint16_t             AddrLen;
        uint16_t             CallType;
    } params;

    memset(&params, 0, sizeof(params));
    params.Socket = (uint16_t) socket;
    params.Buffer = msg;
    params.BufferLen = len;
    params.Flags = flags;
    params.Addr = to;
    params.AddrLen = sizeof(*to);
    params.CallType = to != NULL;  // sendto(), not send()?

    return MSClientCall(0x0d, &params.Header);
}

static ssize_t MSC_recvfrom(SOCKET socket, void far *buf, size_t len,
                           int flags, struct sockaddr_in far *from)
{
    struct {
        struct mssock_header Header;
        uint16_t             Socket;
        void far            *Buffer;
        uint16_t             BufferLen;
        uint16_t             Flags;
        void far            *Addr;
        uint16_t             AddrLen;
        uint16_t             CallType;
    } params;

    memset(&params, 0, sizeof(params));
    params.Socket = (uint16_t) socket;
    params.Buffer = buf;
    params.BufferLen = len;
    params.Flags = flags;
    params.Addr = from;
    params.AddrLen = sizeof(*from);
    params.CallType = from != NULL;  // recvfrom(), not recv()?

    return MSClientCall(0x0b, &params.Header);
}

static int MSC_ioctlsocket(SOCKET socket, unsigned long cmd, void far *value)
{
    struct {
        struct mssock_header Header;
        uint16_t             Socket;
        uint16_t             Command;
        void far            *Value;
    } params;
    int result;

    // Winsock ioctl command# -> MSClient equivalent.
    switch (cmd)
    {
        case FIONBIO:  cmd = 1; break;
        case FIONREAD: cmd = 2; break;
        default:
            WS_LastError = WSAEOPNOTSUPP;
            return -1;
    }

    params.Socket = (uint16_t) socket;
    params.Command = cmd;
    params.Value = value;

    result = MSClientCall(0x09, &params.Header);

    // Winsock ioctl operates on DWORDs, but MSClient uses WORDs. If the
    // call set *value, zero off the top 16-bits of the DWORD.
    ((uint16_t far *) value)[1] = 0;

    return result;
}

SOCKET socket(int domain, int type, int protocol)
{
    switch (stack)
    {
        case MSCLIENT:
            return MSC_socket(domain, type, protocol);
        case WINSOCK1:
        case WINSOCK2:
            return WS_socket(domain, type, protocol);
        default:
            return -1;
    }
}

int closesocket(SOCKET socket)
{
    switch (stack)
    {
        case MSCLIENT:
            return MSC_closesocket(socket);
        case WINSOCK1:
        case WINSOCK2:
            return WS_closesocket(socket);
        default:
            return -1;
    }
}

int bind(SOCKET socket, struct sockaddr_in far *addr)
{
    switch (stack)
    {
        case MSCLIENT:
            return MSC_bind(socket, addr);
        case WINSOCK1:
        case WINSOCK2:
            return WS_bind(socket, addr);
        default:
            return -1;
    }
}

ssize_t sendto(SOCKET socket, const void far *msg, size_t len, int flags,
               const struct sockaddr_in far *to)
{
    switch (stack)
    {
        case MSCLIENT:
            return MSC_sendto(socket, msg, len, flags, to);
        case WINSOCK1:
            return WS_sendto1(socket, msg, len, flags, to);
        case WINSOCK2:
            return WS_sendto2(socket, msg, len, flags, to);
        default:
            return -1;
    }
}

ssize_t recvfrom(SOCKET socket, void far *buf, size_t len, int flags,
                 struct sockaddr_in far *from)
{
    switch (stack)
    {
        case MSCLIENT:
            return MSC_recvfrom(socket,buf, len, flags, from);
        case WINSOCK1:
            return WS_recvfrom1(socket, buf, len, flags, from);
        case WINSOCK2:
            return WS_recvfrom2(socket, buf, len, flags, from);
        default:
            return -1;
    }
}

int ioctlsocket(SOCKET socket, unsigned long cmd, void far *value)
{
    switch (stack)
    {
        case MSCLIENT:
            return MSC_ioctlsocket(socket, cmd, value);
        case WINSOCK1:
            return WS_ioctlsocket1(socket, cmd, value);
        case WINSOCK2:
            return WS_ioctlsocket2(socket, cmd, value);
        default:
            return -1;
    }
}

int inet_aton(const char *cp, struct in_addr *inp)
{
    int a, b, c, d;

    if (sscanf(cp, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
    {
        return 0;
    }

    // Network byte order.
    inp->s_addr = (((unsigned long) d) << 24)
                | (((unsigned long) c) << 16)
                | (((unsigned long) b) << 8)
                | ((unsigned long) a);
    return 1;
}

void WinsockInit(void)
{
    if (getenv("NO_WINSOCK_CHECKS") == NULL)
    {
        CheckWindowsVersion();
    }

    if (!VxdGetEntryPoint(&vxdldr_entry, VXD_ID_VXDLDR))
    {
        LogMessage("Failed to get VxD entrypoint for VXDLDR.");
    }
    else if (!VXDLDR_LoadDevice("WSOCK.VXD")
          && !VXDLDR_LoadDevice("WSOCK.386")
          && !VXDLDR_LoadDevice("WSOCK2.VXD"))
    {
        LogMessage("Failed to load either WSOCK or WSOCK2 VxD.");
    }

    if (VxdGetEntryPoint(&winsock_entry, VXD_ID_WSOCK2))
    {
        stack = WINSOCK2;
    }
    else if (VxdGetEntryPoint(&winsock_entry, VXD_ID_WSOCK))
    {
        stack = WINSOCK1;
    }
    else
    {
        Error("Failed to get VxD entrypoint for either WSOCK or WSOCK2.");
    }

    atexit(WinsockShutdown);

    // With Winsock2 we need to check the API really works.
    // It is the recvfrom() and sendto() calls that are broken; unless the
    // VxD is patched, the socket argument gets mistakenly treated as a memory
    // address and becomes an invalid socket handle, hence WSAENOTSOCK.
    if (stack == WINSOCK2 && getenv("NO_WINSOCK_CHECKS") == NULL)
    {
        struct sockaddr_in nowhere = {AF_INET, 0, {htonl(INADDR_LOOPBACK)}};
        SOCKET s;
        char buf[1];
        int err;

        s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        // TODO: Check socket created ok

        if (sendto(s, buf, 0, 0, &nowhere) < 0
         && WS_LastError == WSAENOTSOCK)
        {
            closesocket(s);
            Error("You have Winsock2 installed but its VxD bug has not been "
                  "patched.\nRun WS2PATCH.EXE and then try again.");
        }

        closesocket(s);
    }
}

