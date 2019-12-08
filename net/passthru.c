// Pass-through driver. Minimal example of the networking API.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include <bios.h>

#include "lib/flag.h"
#include "net/doomnet.h"

static doomcom_t far *inner_driver;

static void far_memcpy(void far *dest, void far *src, size_t nbytes)
{
    unsigned char far *dest_p = (unsigned char far *) dest;
    unsigned char far *src_p = (unsigned char far *) src;
    int i;

    for (i = 0; i < nbytes; ++i)
    {
        *dest_p = *src_p;
        ++dest_p; ++src_p;
    }
}

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

int main(int argc, char *argv[])
{
    doomcom_t far *d;
    char **args;

    NetRegisterFlags();
    args = ParseCommandLine(argc, argv);

    inner_driver = NetLocateDoomcom(args);
    assert(inner_driver != NULL);

    doomcom.numnodes = inner_driver->numnodes;
    doomcom.consoleplayer = inner_driver->consoleplayer;
    doomcom.numplayers = inner_driver->numplayers;
    doomcom.ticdup = inner_driver->ticdup;
    doomcom.extratics = inner_driver->extratics;

    LaunchDOOM(args);

    return 0;
}

