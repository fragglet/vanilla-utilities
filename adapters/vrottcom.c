// Adapter that converts a Doom network driver into a ROTT rottcom
// driver.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <process.h>
#include <assert.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"

#include "adapters/fragment.h"
#include "adapters/nodemap.h"
#include "adapters/rottcom.h"

static struct interrupt_hook net_interrupt;
static doomcom_t far *inner_driver;
static rottcom_t rottcom;

static void interrupt far ExecuteCommand(void)
{
    struct reassembled_packet *pkt;

    switch (rottcom.command)
    {
        case CMD_SEND:
            FragmentSendPacket(playertonode[rottcom.remotenode - 1],
                               rottcom.data, rottcom.datalength);
            break;

        case CMD_GET:
            pkt = FragmentGetPacket();
            if (pkt == NULL)
            {
                rottcom.remotenode = -1;
                return;
            }
            rottcom.remotenode = nodetoplayer[pkt->remotenode] + 1;
            rottcom.datalength = pkt->datalength;
            far_memcpy(rottcom.data, pkt->data, pkt->datalength);
            break;
    }
}

static void interrupt far NetISR(void)
{
    SWITCH_ISR_STACK;
    ExecuteCommand();
    RESTORE_ISR_STACK;
}

static void SetDriver(long l)
{
    assert(inner_driver == NULL);
    inner_driver = NetGetHandle(l);
}

int main(int argc, char *argv[])
{
    char addrstring[16];
    long flataddr;
    int remoteridicule = 0;
    char **args;

    SetHelpText("Doom to ROTT network adapter",
                "ipxsetup -nodes 3 %s rott.exe");

    APIPointerFlag("-net", SetDriver);
    BoolFlag("-remoteridicule", &remoteridicule, "Enable remote ridicule");
    args = ParseCommandLine(argc, argv);
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    assert(inner_driver != NULL);
    InitFragmentReassembly(inner_driver);
    DiscoverPlayers(inner_driver);

    rottcom.consoleplayer = inner_driver->consoleplayer + 1;
    rottcom.numplayers = inner_driver->numplayers;
    rottcom.ticstep = inner_driver->ticdup;
    rottcom.client = rottcom.consoleplayer > 1;
    rottcom.gametype = ROTT_NETWORK_GAME;
    rottcom.remoteridicule = remoteridicule;

    // Prepare to launch game
    if (!FindAndHookInterrupt(&net_interrupt, NetISR))
    {
        Error("Warning: no free interrupt handlers found. You can specify"
              "a vector with the -vector 0x<num> parameter.");
    }

    rottcom.intnum = net_interrupt.interrupt_num;

    // Add -net &rottcom
    flataddr = (long) FP_SEG(&rottcom) * 16 + FP_OFF(&rottcom);
    sprintf(addrstring, "%lu", flataddr);
    args = AppendArgs(args, "-net", addrstring, NULL);

    spawnv(P_WAIT, args[0], args);

    RestoreInterrupt(&net_interrupt);

    return 0;
}

