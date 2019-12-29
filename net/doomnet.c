#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <dos.h>
#include <assert.h>

#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"

static int force_vector = 0;
static int dup = 0, extratics = 0;
doomcom_t doomcom;
static int vectorishooked;
static void interrupt(*olddoomvect) (void);

void NetRegisterFlags(void)
{
    IntFlag("-dup", &dup, "n",
            "reduce movement resolution & bandwidth by factor n");
    IntFlag("-extratics", &extratics, "n",
            "send n extra tics per packet as insurance");
    BoolFlag("-extratic", &extratics, NULL);
    IntFlag("-vector", &force_vector, "v",
            "use interrupt vector v for network API");
}

static void UnhookDoomVector(void)
{
    if (vectorishooked)
    {
        setvect(doomcom.intnum, olddoomvect);
        vectorishooked = 0;
    }
}

/*
=============
=
= LaunchDOOM
=
These fields in doomcom should be filled in before calling:

	short	numnodes;		// console is allways node 0
	short	ticdup;			// 1 = no duplication, 2-5 = dup for slow nets
	short	extratics;		// 1 = send a backup tic in every packet

	short	consoleplayer;	// 0-3 = player number
	short	numplayers;		// 1-4
	short	angleoffset;	// 1 = left, 0 = center, -1 = right
	short	drone;			// 1 = drone
=============
*/

void LaunchDOOM(char **args)
{
    char addrstring[10];
    long flatadr;

    if (dup != 0)
    {
        doomcom.ticdup = (short) dup;
    }
    if (extratics != 0)
    {
        doomcom.extratics = (short) extratics;
    }

    // prepare for DOOM
    doomcom.id = DOOMCOM_ID;

    if (force_vector)
    {
        doomcom.intnum = force_vector;
    }
    else
    {
        for (doomcom.intnum = 0x60; doomcom.intnum <= 0x66; doomcom.intnum++)
        {
            if (getvect(doomcom.intnum) == NULL)
            {
                break;
            }
        }
        if (doomcom.intnum == 0x67)
        {
            LogMessage("Warning: no NULL or iret interrupt vectors were "
                       "found in the 0x60 to 0x66 range. You can specify a "
                       "vector with the -vector 0x<num> parameter.");
            doomcom.intnum = 0x66;
        }
    }

    LogMessage("Communicating with interupt vector 0x%x", doomcom.intnum);

    olddoomvect = getvect(doomcom.intnum);
    setvect(doomcom.intnum, NetISR);
    vectorishooked = 1;

    // We unhook the vector anyway after the game exits, but just in case, set
    // an atexit handler as well - it will gracefully handle multiple calls.
    atexit(UnhookDoomVector);

    // Add -net &doomcom
    flatadr = (long) FP_SEG(&doomcom) * 16 + FP_OFF(&doomcom);
    sprintf(addrstring, "%lu", flatadr);
    args = AppendArgs(args, "-net", addrstring, NULL);

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
    result = MK_FP(seg, l & 0xffffL);
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


