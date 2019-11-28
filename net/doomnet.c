#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <dos.h>
#include "doomnet.h"

doomcom_t	doomcom;
int			vectorishooked;
void interrupt (*olddoomvect) (void);



/*
=================
=
= CheckParm
=
= Checks for the given parameter in the program's command line arguments
=
= Returns the argument number (1 to argc-1) or 0 if not present
=
=================
*/

int CheckParm (char *check)
{
	int             i;

	for (i = 1;i<_argc;i++)
		if ( !stricmp(check,_argv[i]) )
			return i;

	return 0;
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

void LaunchDOOM (void)
{
	char	*newargs[99];
	char	adrstring[10];
	long  	flatadr;
	int		p;
	unsigned char	far	*vector;

// prepare for DOOM
	doomcom.id = DOOMCOM_ID;

// hook an interrupt vector
	p= CheckParm ("-vector");

	if (p)
	{
		doomcom.intnum = sscanf ("0x%x",_argv[p+1]);
	}
	else
	{
		for (doomcom.intnum = 0x60 ; doomcom.intnum <= 0x66 ; doomcom.intnum++)
		{
			vector = *(char far * far *)(doomcom.intnum*4);
			if ( !vector || *vector == 0xcf )
				break;
		}
		if (doomcom.intnum == 0x67)
		{
			printf ("Warning: no NULL or iret interrupt vectors were found in the 0x60 to 0x66\n"
					"range.  You can specify a vector with the -vector 0x<num> parameter.\n");
			doomcom.intnum = 0x66;
		}
	}
	printf ("Communicating with interupt vector 0x%x\n",doomcom.intnum);

	olddoomvect = getvect (doomcom.intnum);
	setvect (doomcom.intnum,NetISR);
	vectorishooked = 1;

// build the argument list for DOOM, adding a -net &doomcom
	memcpy (newargs, _argv, (_argc+1)*2);
	newargs[_argc] = "-net";
	flatadr = (long)_DS*16 + (unsigned)&doomcom;
	sprintf (adrstring,"%lu",flatadr);
	newargs[_argc+1] = adrstring;
	newargs[_argc+2] = NULL;

//	spawnv  (P_WAIT, "m:\\newdoom\\doom", newargs);
	spawnv  (P_WAIT, "doom", newargs);

	printf ("Returned from DOOM\n");


}


