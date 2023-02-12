// Library for doing DOS Winsock networking via VxD backdoors.
// This is some deep magic. Some recommended reading:
// * WSOCK.VXD pseudo-documentation (Richard Dawe)
//   <https://www.richdawe.be/archive/dl/wsockvxd.htm>
// * DJGPP libsocket, particularly the src/wsock directory (ls080s.zip).
// * Berczi Gabor's Winsock2 Pascal code (ws2dos.zip). VERY good
//   explanation of the bugs in WSOCK2.VXD.
// * Ralf Brown's Interrupt List, which has some documentation for the
//   Windows VxD backdoor APIs.
// * The book "Unauthorized Windows 95", Andrew Schulman. ISBN 978-1568841694

#include <stdlib.h>
#include <inttypes.h>

#include "lib/dos.h"
#include "lib/log.h"
#include "net/llcall.h"
#include "net/winsock.h"

// Windows type names:
typedef unsigned long DWORD;
typedef unsigned short WORD;

#define VXD_ID_VXDLDR    0x0027
#define VXD_ID_WSOCK     0x003e
#define VXD_ID_WSOCK2    0x3b0a

typedef void __stdcall far (*vxd_entrypoint)();

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
        DWORD   NewSocketHandle;
    } params;

    params.AddressFamily = domain;
    params.SocketType = type;
    params.Protocol = protocol;
    params.NewSocket = 0;
    params.NewSocketHandle = handle_counter;
    ++handle_counter;

    err = WinsockCall(0x110, &params);
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

    return WinsockCall(0x102, &params);
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

