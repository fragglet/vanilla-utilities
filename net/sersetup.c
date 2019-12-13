// sersetup.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <stdarg.h>
#include <bios.h>
#include "lib/inttypes.h"

#include "lib/flag.h"
#include "lib/log.h"
#include "net/sersetup.h"
#include "net/doomnet.h"

extern que_t inque, outque;

void JumpStart(void);
extern int uart;

static int usemodem;
static char startup[256], shutdown[256];
static long baudrate = 9600;

void ModemCommand(char *str);

/*
================
=
= WriteBuffer
=
================
*/

void WriteBuffer(char *buffer, unsigned int count)
{
    // if this would overrun the buffer, throw everything else out
    if (outque.head - outque.tail + count > QUESIZE)
        outque.tail = outque.head;

    while (count--)
        WriteByte(*buffer++);

    if (INPUT(uart + LINE_STATUS_REGISTER) & 0x40)
        JumpStart();
}

/*
=================
=
= Error
=
= For abnormal program terminations
=
=================
*/

void Error(char *error, ...)
{
    va_list argptr;

    if (usemodem)
    {
        LogMessage("Dropping DTR");
        OUTPUT(uart + MODEM_CONTROL_REGISTER,
               INPUT(uart + MODEM_CONTROL_REGISTER) & ~MCR_DTR);
        delay(1250);
        OUTPUT(uart + MODEM_CONTROL_REGISTER,
               INPUT(uart + MODEM_CONTROL_REGISTER) | MCR_DTR);
        ModemCommand("+++");
        delay(1250);
        ModemCommand(shutdown);
        delay(1250);

    }

    ShutdownPort();

    if (vectorishooked)
        setvect(doomcom.intnum, olddoomvect);

    if (error)
    {
        va_start(argptr, error);
        vprintf(error, argptr);
        va_end(argptr);
        printf("\n");
        exit(1);
    }

    exit(0);
}

/*
================
=
= ReadPacket
=
================
*/

#define MAXPACKET	512
#define	FRAMECHAR	0x70

char packet[MAXPACKET];
static int packetlen;
static int inescape;
static int newpacket;

boolean ReadPacket(void)
{
    int c;

    // if the buffer has overflowed, throw everything out

    if (inque.head - inque.tail > QUESIZE - 4)  // check for buffer overflow
    {
        inque.tail = inque.head;
        newpacket = true;
        return false;
    }

    if (newpacket)
    {
        packetlen = 0;
        newpacket = 0;
    }

    do
    {
        c = ReadByte();
        if (c < 0)
            return false;       // haven't read a complete packet
        //printf ("%c",c);
        if (inescape)
        {
            inescape = false;
            if (c != FRAMECHAR)
            {
                newpacket = 1;
                return true;    // got a good packet
            }
        }
        else if (c == FRAMECHAR)
        {
            inescape = true;
            continue;           // don't know yet if it is a terminator
        }                       // or a literal FRAMECHAR

        if (packetlen >= MAXPACKET)
            continue;           // oversize packet
        packet[packetlen] = c;
        packetlen++;
    } while (1);

}

/*
=============
=
= WritePacket
=
=============
*/

void WritePacket(char *buffer, int len)
{
    int b;
    char static localbuffer[MAXPACKET * 2 + 2];

    b = 0;
    if (len > MAXPACKET)
        return;

    while (len--)
    {
        if (*buffer == FRAMECHAR)
            localbuffer[b++] = FRAMECHAR;       // escape it for literal
        localbuffer[b++] = *buffer++;
    }

    localbuffer[b++] = FRAMECHAR;
    localbuffer[b++] = 0;

    WriteBuffer(localbuffer, b);
}

/*
=============
=
= NetISR
=
=============
*/

void interrupt NetISR(void)
{
    if (doomcom.command == CMD_SEND)
    {
        WritePacket((char *)&doomcom.data, doomcom.datalength);
    }
    else if (doomcom.command == CMD_GET)
    {
        if (ReadPacket() && packetlen <= sizeof(doomcom.data))
        {
            doomcom.remotenode = 1;
            doomcom.datalength = packetlen;
            memcpy(&doomcom.data, &packet, packetlen);
        }
        else
            doomcom.remotenode = -1;
    }
}

/*
=================
=
= Connect
=
= Figures out who is player 0 and 1
=================
*/

void Connect(void)
{
    struct time time;
    int oldsec;
    int localstage, remotestage;
    char str[20];

    //
    // wait for a good packet
    //
    LogMessage("Attempting to connect across serial link");

    oldsec = -1;
    localstage = remotestage = 0;

    do
    {
        while (bioskey(1))
        {
            if ((bioskey(0) & 0xff) == 27)
                Error("\n\nNetwork game synchronization aborted.");
        }

        while (ReadPacket())
        {
            packet[packetlen] = 0;
            //                     printf ("read: %s\n",packet);
            if (packetlen != 7)
                goto badpacket;
            if (strncmp(packet, "PLAY", 4))
                goto badpacket;
            remotestage = packet[6] - '0';
            localstage = remotestage + 1;
            if (packet[4] == '0' + doomcom.consoleplayer)
            {
                doomcom.consoleplayer ^= 1;
                localstage = remotestage = 0;
            }
            oldsec = -1;
        }
 badpacket:

        gettime(&time);
        if (time.ti_sec != oldsec)
        {
            oldsec = time.ti_sec;
            sprintf(str, "PLAY%i_%i", doomcom.consoleplayer, localstage);
            WritePacket(str, strlen(str));
            //                     printf ("wrote: %s\n",str);
        }

    } while (remotestage < 1);

    //
    // flush out any extras
    //
    while (ReadPacket())
        ;
}

/*
==============
=
= ModemCommand
=
==============
*/

void ModemCommand(char *str)
{
    LogMessage("Modem command: %s", str);
    WriteBuffer(str, strlen(str));
    WriteBuffer("\r", 1);
}

/*
==============
=
= ModemResponse
=
= Waits for OK, RING, CONNECT, etc
==============
*/

static char response[80];

void ModemResponse(char *resp)
{
    int c;
    int respptr;

    do
    {
        respptr = 0;
        do
        {
            while (bioskey(1))
            {
                if ((bioskey(0) & 0xff) == 27)
                    Error("\nModem response aborted.");
            }
            c = ReadByte();
            if (c == -1)
                continue;
            if (c == '\n' || respptr == 79)
            {
                response[respptr] = 0;
                LogMessage("Modem response: %s", response);
                break;
            }
            if (c >= ' ')
            {
                response[respptr] = c;
                respptr++;
            }
        } while (1);

    } while (strncmp(response, resp, strlen(resp)));
}

/*
=============
=
= InitModem
=
=============
*/

static void ReadModemCfg(void)
{
    char baudline[16];
    FILE *f;

    f = fopen("modem.cfg", "r");
    if (!f)
    {
        Error("Couldn't read MODEM.CFG");
    }
    if (fgets(startup, sizeof(startup), f) == NULL
     || fgets(shutdown, sizeof(shutdown), f) == NULL
     || fgets(baudline, sizeof(baudline), f) == NULL)
    {
        Error("Unexpected error reading from MODEM.CFG");
    }
    fclose(f);

    errno = 0;
    baudrate = strtol(baudline, NULL, 10);
    if (baudrate == 0 && errno != 0)
    {
        Error("Error parsing baud rate '%s'", baudline);
    }
}

/*
=============
=
= Dial
=
=============
*/

void Dial(char *dial_no)
{
    char cmd[80];

    usemodem = true;
    ModemCommand(startup);
    ModemResponse("OK");

    LogMessage("Dialing...");
    sprintf(cmd, "ATDT%s", dial_no);

    ModemCommand(cmd);
    ModemResponse("CONNECT");
    doomcom.consoleplayer = 1;
}

/*
=============
=
= Answer
=
=============
*/

void Answer(void)
{
    usemodem = true;
    ModemCommand(startup);
    ModemResponse("OK");
    LogMessage("Waiting for ring...");

    ModemResponse("RING");
    ModemCommand("ATA");
    ModemResponse("CONNECT");

    doomcom.consoleplayer = 0;
}

/*
=================
=
= main
=
=================
*/

void main(int argc, char *argv[])
{
    int answer = 0;
    int force_player1 = 0;
    char *dial_no = NULL;
    char **args;

    SetHelpText("Doom serial port network device driver",
                "%s -dial 555-1212 doom.exe -deathmatch -nomonsters");
    BoolFlag("-answer", &answer, "listen for incoming call");
    StringFlag("-dial", &dial_no, "phone#",
               "dial the given phone number");
    BoolFlag("-player1", &force_player1, "force this side to be player 1");
    SerialRegisterFlags();
    NetRegisterFlags();

    args = ParseCommandLine(argc, argv);

    //
    // set network characteristics
    //
    doomcom.ticdup = 1;
    doomcom.extratics = 0;
    doomcom.consoleplayer = 0;
    doomcom.numnodes = 2;
    doomcom.numplayers = 2;
    doomcom.drone = 0;

    //
    // allow override of automatic player ordering to allow a slower computer
    // to be set as player 1 always
    //
    if (force_player1)
    {
        doomcom.consoleplayer = 1;
    }

    if (dial_no != NULL || answer)
    {
	ReadModemCfg();
    }

    //
    // establish communications
    //
    InitPort(baudrate);

    if (dial_no != NULL)
    {
        Dial(dial_no);
    }
    else if (answer)
    {
        Answer();
    }

    Connect();

    //
    // launch DOOM
    //
    LaunchDOOM(args);

    Error(NULL);
}
