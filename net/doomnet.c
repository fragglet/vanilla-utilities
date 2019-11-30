#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <conio.h>
#include <dos.h>

#include "lib/flag.h"
#include "net/doomnet.h"

static int force_vector = 0;
static int dup = 0, extratics = 0;
doomcom_t doomcom;
int vectorishooked;
void interrupt(*olddoomvect) (void);

void NetRegisterFlags(void)
{
    IntFlag("-dup", &dup, "n",
            "reduce game solution by factor n");
    IntFlag("-extratics", &extratics, "n",
            "send n extra tics per packet as insurance");
    BoolFlag("-extratic", &extratics, NULL);
    IntFlag("-vector", &force_vector, "v",
            "use interrupt vector v for network API");
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
    unsigned char far *vector;

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
            vector = *(char far * far *)(doomcom.intnum * 4);
            if (!vector || *vector == 0xcf)
                break;
        }
        if (doomcom.intnum == 0x67)
        {
            printf("Warning: no NULL or iret interrupt vectors were found in the 0x60 to 0x66\n"
                 "range.  You can specify a vector with the -vector 0x<num> parameter.\n");
            doomcom.intnum = 0x66;
        }
    }

    printf("Communicating with interupt vector 0x%x\n", doomcom.intnum);

    olddoomvect = getvect(doomcom.intnum);
    setvect(doomcom.intnum, NetISR);
    vectorishooked = 1;

    // Add -net &doomcom
    flatadr = (long)_DS *16 + (unsigned)&doomcom;
    sprintf(addrstring, "%lu", flatadr);
    args = AppendArgs(args, "-net", addrstring, NULL);

    spawnv(P_WAIT, args[0], args);
}
