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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "lib/dos.h"
#include "lib/log.h"
#include "net/llcall.h"
#include "net/winsock.h"

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

unsigned long WS_LastError;
static int winsock2 = 1;

static vxd_entrypoint vxdldr_entry;
static vxd_entrypoint winsock_entry;

static void VxdGetEntryPoint(vxd_entrypoint *entrypoint, int id)
{
    union REGS inregs, outregs;
    struct SREGS sregs;

    inregs.x.ax = 0x1684;
    inregs.x.bx = id;
    int86x(0x2f, &inregs, &outregs, &sregs);

    if (sregs.es == 0 && outregs.x.di == 0)
    {
        Error("Error getting entrypoint for VxD with ID %04x: es:di=null", id);
    }
    *entrypoint = MK_FP(sregs.es, outregs.x.di);
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

SOCKET WS_socket(int domain, int type, int protocol)
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

int WS_closesocket(SOCKET socket)
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

int WS_bind(SOCKET socket, struct sockaddr_in far *addr)
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

ssize_t WS_sendto(SOCKET socket, const void far *msg, size_t len, int flags,
                  const struct sockaddr_in far *to)
{
    if (winsock2)
    {
        return WS_sendto2(socket, msg, len, flags, to);
    }
    else
    {
        return WS_sendto1(socket, msg, len, flags, to);
    }
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

ssize_t WS_recvfrom(SOCKET socket, void far *buf, size_t len, int flags,
                    struct sockaddr_in far *from)
{
    if (winsock2)
    {
        return WS_recvfrom2(socket, buf, len, flags, from);
    }
    else
    {
        return WS_recvfrom1(socket, buf, len, flags, from);
    }
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

int WS_ioctlsocket(SOCKET socket, unsigned long cmd, void far *value)
{
    if (winsock2)
    {
        return WS_ioctlsocket2(socket, cmd, value);
    }
    else
    {
        return WS_ioctlsocket1(socket, cmd, value);
    }
}

int WS_inet_aton(const char *cp, struct in_addr *inp)
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

static void WinsockShutdown(void)
{
    VXDLDR_UnloadDevice("WSOCK.VXD");
    VXDLDR_UnloadDevice("WSOCK2.VXD");
}

static void CheckWindowsVersion(void)
{
    union REGS inregs, outregs;

    inregs.x.ax = 0x160a;
    int86(0x2f, &inregs, &outregs);

    // Must be Windows 4.x (9x).
    if (outregs.x.ax != 0 || outregs.x.cx != 3 ||  outregs.h.bh != 4)
    {
        Error("This program only works under Windows 9x.");
    }

    // Can't be NT. Check for DOS 7 (NT pretends to be DOS 5).
    inregs.h.ah = 0x30;
    inregs.h.al = 1;
    int86(0x21, &inregs, &outregs);

    switch (outregs.h.al)
    {
        case 5:
            Error("This program doesn't work under Windows NT, "
                  "only Windows 9x.");
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

void WinsockInit(void)
{
    CheckWindowsVersion();

    VxdGetEntryPoint(&vxdldr_entry, VXD_ID_VXDLDR);

    if (VXDLDR_LoadDevice("WSOCK2.VXD"))
    {
        winsock2 = 1;
        VxdGetEntryPoint(&winsock_entry, VXD_ID_WSOCK2);
        // TODO: Check the bugfix has been applied with WS2PATCH
    }
    else if (VXDLDR_LoadDevice("WSOCK.VXD"))  // TODO: And WSOCK.386?
    {
        winsock2 = 0;
        VxdGetEntryPoint(&winsock_entry, VXD_ID_WSOCK);
    }
    else
    {
        Error("Failed to load either WSOCK.VXD or WSOCK2.VXD.");
    }

    atexit(WinsockShutdown);

    // With Winsock2 we need to check the API really works.
    // It is the recvfrom() and sendto() calls that are broken; unless the
    // VxD is patched, the socket argument gets mistakenly treated as a memory
    // address and becomes an invalid socket handle, hence WSAENOTSOCK.
    if (winsock2)
    {
        struct sockaddr_in nowhere = {AF_INET, 0, {WS_htonl(INADDR_LOOPBACK)}};
        SOCKET s;
        char buf[1];
        int err;

        s = WS_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        // TODO: Check socket created ok

        if (WS_sendto(s, buf, 0, 0, &nowhere) < 0
         && WS_LastError == WSAENOTSOCK)
        {
            WS_closesocket(s);
            Error("You have Winsock2 installed but its VxD bug has not been "
                  "patched.\nRun WS2PATCH.EXE and then try again.");
        }

        WS_closesocket(s);
    }
}

