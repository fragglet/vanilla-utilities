// Pass-through driver. Minimal example of the networking API.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"

static doomcom_t far *inner_driver;

void interrupt NetISR(void)
{
    switch (doomcom.command)
    {
        case CMD_SEND:
            inner_driver->remotenode = doomcom.remotenode;
            inner_driver->datalength = doomcom.datalength;
            far_memcpy(&inner_driver->data, doomcom.data, doomcom.datalength);
            NetSendPacket(inner_driver);
            break;

        case CMD_GET:
            NetGetPacket(inner_driver);
            if (inner_driver->remotenode == -1)
            {
                doomcom.remotenode = -1;
                return;
            }
            doomcom.remotenode = inner_driver->remotenode;
            doomcom.datalength = inner_driver->datalength;
            far_memcpy(doomcom.data, &inner_driver->data,
                       inner_driver->datalength);
            break;
    }
}

static void SetDriver(long l)
{
    assert(inner_driver == NULL);
    inner_driver = NetGetHandle(l);
}

int main(int argc, char *argv[])
{
    char **args;

    APIPointerFlag("-net", SetDriver);
    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    assert(inner_driver != NULL);

    doomcom.numnodes = inner_driver->numnodes;
    doomcom.consoleplayer = inner_driver->consoleplayer;
    doomcom.numplayers = inner_driver->numplayers;
    doomcom.ticdup = inner_driver->ticdup;
    doomcom.extratics = inner_driver->extratics;

    LaunchDOOM(args);

    return 0;
}

