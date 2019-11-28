// parsetup.c

//  Copyright 1994 Scott Coleman, American Society of Reverse Engineers

//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 1.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

// NOTE: Portions of this program were adapted from other freely available
// software, including SERSETUP and the Crynwr PLIP parallel port Internet
// Protocol driver.


#include <bios.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <mem.h>
#include <dos.h>
#include <time.h>
#include <string.h>
#include "parsetup.h"
#include "doomnet.h"

unsigned newpkt=0;

extern void recv(void);
extern void send_pkt(void);
extern byte pktbuf[];
extern unsigned recv_count;
extern unsigned errcnt;


/*
=================
=
= Error
=
= For abnormal program terminations
=
=================
*/

void Error (char *error, ...)
{
va_list argptr;

        ShutdownPort();

	if (error)
	{
		va_start (argptr,error);
		vprintf (error,argptr);
		va_end (argptr);
		printf ("\n");
		exit (1);
	}

        printf ("Clean exit from PARSETUP\n");
	exit (0);
}


/*
================
=
= ReadPacket
=
================
*/

#define MAXPACKET	512

boolean ReadPacket(void)
{

        if (newpkt) {
                newpkt=0;
                return true;   // true - got a good packet
                }

        return (false);     // false - no packet available

}


/*
=============
=
= WritePacket
=
=============
*/

int WritePacket(byte *data, unsigned len)
{

        _CX = len;
        _DS = FP_SEG(data);
        _SI = FP_OFF(data);
        _ES = _DI = 0;
        send_pkt();

        asm jnc sendok;

        return (_DH);

sendok:
	return (0);

}


/*
=============
=
= NetISR
=
=============
*/

void interrupt NetISR (void)
{
	if (doomcom.command == CMD_SEND)
	{
                // I_ColorBlack (0,0,63);
		WritePacket ((char *)&doomcom.data, doomcom.datalength);
	}
	else if (doomcom.command == CMD_GET)
	{
        //I_ColorBlack (63,63,0);

        if (ReadPacket () && recv_count <= sizeof(doomcom.data) )
		{
			doomcom.remotenode = 1;
                        doomcom.datalength = recv_count;
                        memcpy (&doomcom.data, &pktbuf, recv_count);
		}
		else
			doomcom.remotenode = -1;

	}
        //I_ColorBlack (0,0,0);
}




/*
=================
=
= Connect
=
= Figures out who is player 0 and 1
=================
*/

void Connect (void)
{
struct time     time;
int             oldsec;
int     localstage, remotestage;
char    str[20];


    printf ("Attempting to connect across parallel link, press escape to abort.\n");


        //
        // wait for a good packet
        //

	oldsec = -1;
	localstage = remotestage = 0;

	do
	{
		while ( bioskey(1) )
		{
			if ( (bioskey (0) & 0xff) == 27)
				Error ("\n\nNetwork game synchronization aborted.");
		}

		while (ReadPacket ())
		{
			pktbuf[recv_count] = 0;
			printf ("read: %s\n",pktbuf);
			if (recv_count != 7)
				goto badpacket;
			if (strncmp(pktbuf,"PLAY",4) )
				goto badpacket;
			remotestage = pktbuf[6] - '0';
			localstage = remotestage+1;
			if (pktbuf[4] == '0'+doomcom.consoleplayer)
			{
				doomcom.consoleplayer ^= 1;
				localstage = remotestage = 0;
			}
			oldsec = -1;
		}
badpacket:

		gettime (&time);
		if (time.ti_sec != oldsec)
		{
			oldsec = time.ti_sec;
			sprintf (str,"PLAY%i_%i",doomcom.consoleplayer,localstage);
			WritePacket (str,strlen(str));
                printf ("wrote: %s\n",str);
		}

	} while (remotestage < 1);

//
// flush out any extras
//
	while (ReadPacket ())
	;
}


/*
=================
=
= main
=
=================
*/

void main(void)
{
int p;
time_t t;

        //
        // set network characteristics
        //
	doomcom.ticdup = 1;
	doomcom.extratics = 0;
	doomcom.numnodes = 2;
	doomcom.numplayers = 2;
	doomcom.drone = 0;

	t = time(&t);

	printf("\n"
                "DOOM PRINTER PORT DEVICE DRIVER version 1.1\n"
                "Brought to you by the American Society of Reverse Engineers\n"
                "Send comments or (gasp!) bug reports to asre@uiuc.edu\n\n");

//
// allow override of automatic player ordering to allow a slower computer
// to be set as player 1 always
//
	if (CheckParm ("-player1"))
		doomcom.consoleplayer = 1;
	else
		doomcom.consoleplayer = 0;

//
// establish communications
//
	InitPort ();

        Connect ();

//
// launch DOOM
//
	LaunchDOOM ();

	Error (NULL);

}

