#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <assert.h>

#include "lib/flag.h"
#include "lib/ints.h"
#include "lib/log.h"
#include "net/doomnet.h"

static struct interrupt_hook net_interrupt;
static void (far *isr_callback)(void);
int doomnet_dup = 1, doomnet_extratics = 0;

void NetRegisterFlags(void)
{
    IntFlag("-dup", &doomnet_dup, "n",
            "reduce movement resolution & bandwidth by factor n");
    IntFlag("-extratics", &doomnet_extratics, "n",
            "send n extra tics per packet as insurance");
    BoolFlag("-extratic", &doomnet_extratics, NULL);
    IntFlag("-vector", &net_interrupt.force_vector, "v",
            "use interrupt vector v for network API");
}

static void UnhookDoomVector(void)
{
    RestoreInterrupt(&net_interrupt);
}

static void interrupt far NetISR(void)
{
    SWITCH_ISR_STACK;
    isr_callback();
    RESTORE_ISR_STACK;
}

/*
=============
=
= NetLaunchDoom
=
These fields in doomcom should be filled in before calling:

	short	numnodes;		// console is always node 0
	short	ticdup;			// 1 = no duplication, 2-5 = dup for slow nets
	short	extratics;		// 1 = send a backup tic in every packet

	short	consoleplayer;	// 0-3 = player number
	short	numplayers;		// 1-4
	short	angleoffset;	// 1 = left, 0 = center, -1 = right
	short	drone;			// 1 = drone
=============
*/

void NetLaunchDoom(doomcom_t far *doomcom, char **args,
                   void (far *callback)(void))
{
    char addrstring[10];
    long flatadr;

    isr_callback = callback;

    if (doomnet_dup != 1)
    {
        doomcom->ticdup = (short) doomnet_dup;
    }
    if (doomnet_extratics != 0)
    {
        doomcom->extratics = (short) doomnet_extratics;
    }

    // prepare for DOOM
    doomcom->id = DOOMCOM_ID;

    if (!FindAndHookInterrupt(&net_interrupt, NetISR))
    {
        Error("Warning: no free interrupt handlers found. You can specify "
              "a vector with the -vector 0x<num> parameter.");
    }

    doomcom->intnum = net_interrupt.interrupt_num;

    // We unhook the vector anyway after the game exits, but just in case, set
    // an atexit handler as well - it will gracefully handle multiple calls.
    atexit(UnhookDoomVector);

    // Add -net &doomcom
    flatadr = (long) FP_SEG(doomcom) * 16 + FP_OFF(doomcom);
    sprintf(addrstring, "%lu", flatadr);
    args = AppendArgs(args, "-net", addrstring, NULL);

    SquashToResponseFile(args);
    spawnv(P_WAIT, args[0], args);

    UnhookDoomVector();
}

// NetGetHandle takes the given long value read from the command line
// and returns a doomcom_t pointer, performing appropriate checks.
doomcom_t far *NetGetHandle(long l)
{
    doomcom_t far *result = NULL;
    unsigned int seg;

    assert(l != 0);
    seg = (int) ((l >> 4) & 0xf000L);
    result = (void far *) MK_FP(seg, l & 0xffffL);
    assert(result->id == DOOMCOM_ID);

    return result;
}

void NetSendPacket(doomcom_t far *doomcom)
{
    union REGS regs;
    doomcom->command = CMD_SEND;
    int86(doomcom->intnum, &regs, &regs);
}

int NetGetPacket(doomcom_t far *doomcom)
{
    union REGS regs;
    doomcom->command = CMD_GET;
    int86(doomcom->intnum, &regs, &regs);
    return doomcom->remotenode != -1;
}


