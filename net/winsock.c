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

#define WSOCK_CLOSESOCKET_CMD       0x0102
#define WSOCK_GETPEERNAME_CMD       0x0104
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

static int winsock2 = 1;

static vxd_entrypoint vxdldr_entry;
static vxd_entrypoint wsock2_entry;

static void VxdGetEntryPoint(vxd_entrypoint *entrypoint, int id)
{
    union REGS inregs, outregs;
    struct SREGS sregs;

    inregs.x.ax = 0x1684;
    inregs.x.bx = id;
    int86x(0x20, &inregs, &outregs, &sregs);

    if (sregs.es == 0 && outregs.x.di == 0)
    {
        Error("Error getting entrypoint for VxD with ID %04x: es:di=null", id);
    }
    *entrypoint = MK_FP(sregs.es, outregs.x.di);
}

static void VXDLDR_LoadDevice(char *device)
{
    ll_funcptr = vxdldr_entry;
    ll_regs.x.ax = 0x0001;
    // DS is already equal to FP_SEG(device) (non-far ptr)
    ll_regs.x.dx = FP_OFF(device);
    LowLevelCall();
    if (ll_regs.x.ax != 0)
    {
        Error("Error loading VxD '%s': ax=0x%x", device, ll_regs.x.ax);
    }
}

static void VXDLDR_UnloadDevice(char *device)
{
    ll_funcptr = vxdldr_entry;
    ll_regs.x.ax = 0x0002;
    // DS is already equal to FP_SEG(device) (non-far ptr)
    ll_regs.x.dx = FP_OFF(device);
    LowLevelCall();
    if (ll_regs.x.ax != 0)
    {
        Error("Error unloading VxD '%s': ax=0x%x", device, ll_regs.x.ax);
    }
}

static int WinsockCall(int function, void far *ptr)
{
    ll_funcptr = wsock2_entry;
    ll_regs.x.ax = function;
    ll_regs.x.es = FP_SEG(ptr);
    ll_regs.x.bx = FP_OFF(ptr);
    LowLevelCall();
    return ll_regs.x.ax;
}

SOCKET WS_socket(int domain, int type, int protocol)
{
    static DWORD handle_counter = 999900UL;
    int err;
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

    err = WinsockCall(WSOCK_SOCKET_CMD, &params);
    if (err != 0)
    {
        return err;
    }
    return params.NewSocket;
}

int WS_close(SOCKET socket)
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

// Winsock1 version of sendto().
static ssize_t WS_sendto1(SOCKET socket, const void *msg, size_t len, int flags,
                          const struct sockaddr_in *to)
{
    int err;
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

    err = WinsockCall(WSOCK_SEND_CMD, &params);
    if (err != 0)
    {
        return err;
    }

    return params.BytesSent;
}

struct WSABuffer {
    DWORD Length;
    DWORD Address;
};

// Winsock2 version.
static ssize_t WS_sendto2(SOCKET socket, const void *msg, size_t len, int flags,
                          const struct sockaddr_in *to)
{
    struct WSABuffer buffer;
    DWORD unused = SOCKADDR_SIZE;  // for AddrLenPtr
    int err;
    struct {
        struct WSABuffer far *Buffers;
        const void far *Address;     // ___ ^ Addresses translaed by VxD
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
    params.AddrLenPtr = &unused;  // MapFlatPointer()?

    err = WinsockCall(WSOCK_SEND_CMD, &params);
    if (err != 0)
    {
        return err;
    }

    return params.BytesSent;
}

ssize_t WS_sendto(SOCKET socket, const void *msg, size_t len, int flags,
                  const struct sockaddr_in *to)
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
static ssize_t WS_recvfrom1(SOCKET socket, void *buf, size_t len, int flags,
                            struct sockaddr_in *from)
{
    static uint8_t frombuf[SOCKADDR_SIZE];
    int err;
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

    err = WinsockCall(WSOCK_RECV_CMD, &params);
    if (err != 0)
    {
        return err;
    }

    memcpy(from, frombuf, sizeof(struct sockaddr_in));

    return params.BytesReceived;
}

static ssize_t WS_recvfrom2(SOCKET socket, void *buf, size_t len, int flags,
                            struct sockaddr_in *from)
{
    static uint8_t frombuf[SOCKADDR_SIZE];
    struct WSABuffer buffer;
    DWORD unused = SOCKADDR_SIZE;  // for AddrLenPtr
    int err;
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

    err = WinsockCall(WSOCK_RECV_CMD, &params);
    if (err != 0)
    {
        return err;
    }

    memcpy(from, frombuf, sizeof(struct sockaddr_in));

    return params.BytesReceived;
}

ssize_t WS_recvfrom(SOCKET socket, void *buf, size_t len, int flags,
                    struct sockaddr_in *from)
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

unsigned long WS_htonl(unsigned long val)
{
    return ((val & 0xff000000) >> 24)
         | ((val & 0x00ff0000) >> 8)
         | ((val & 0x0000ff00) << 8)
         | ((val & 0x000000ff) << 24);
}

unsigned short WS_htons(unsigned short val)
{
    return ((val & 0x00ff) << 8)
         | ((val & 0xff00) >> 8);
}

static void WinsockShutdown(void)
{
    VXDLDR_UnloadDevice("WSOCK2.DLL");
}

void WinsockInit(void)
{
    // TODO: Sanity check what version of Windows we're running under

    VxdGetEntryPoint(&vxdldr_entry, VXD_ID_VXDLDR);

    // TODO: Winsock1 (WSOCK.DLL); WfW (WSOCK.386) ...
    VXDLDR_LoadDevice("WSOCK2.DLL");

    VxdGetEntryPoint(&wsock2_entry, VXD_ID_WSOCK2);

    // TODO: Check the API really works
    // TODO: Hotpatch to work around Winsock2 bug
    atexit(WinsockShutdown);
}
