// sersetup.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <stdarg.h>
#include <time.h>
#include "lib/inttypes.h"

#include "lib/flag.h"
#include "lib/log.h"
#include "net/sersetup.h"
#include "net/doomnet.h"

extern que_t inque, outque;

void JumpStart(void);
extern int uart;

static char *modem_config_file = "modem.cfg";
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

int ReadPacket(void)
{
    int c;

    // if the buffer has overflowed, throw everything out

    if (inque.head - inque.tail > QUESIZE - 4)  // check for buffer overflow
    {
        inque.tail = inque.head;
        newpacket = 1;
        return 0;
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
            return 0;       // haven't read a complete packet
        //printf ("%c",c);
        if (inescape)
        {
            inescape = 0;
            if (c != FRAMECHAR)
            {
                newpacket = 1;
                return 1;    // got a good packet
            }
        }
        else if (c == FRAMECHAR)
        {
            inescape = 0;
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
    clock_t last_time = 0, now;
    int localstage, remotestage;
    char str[20];

    //
    // wait for a good packet
    //
    LogMessage("Attempting to connect across serial link");

    localstage = remotestage = 0;

    do
    {
        CheckAbort("Serial port synchronization");

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
            last_time = 0;
        }
 badpacket:

        now = clock();
        if (now - last_time >= CLOCKS_PER_SEC)
        {
            last_time = now;
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
            CheckAbort("Modem response");
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

static void HangupModem(void)
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

    f = fopen(modem_config_file, "r");
    if (!f)
    {
        Error("Couldn't read '%s'", modem_config_file);
    }
    if (fgets(startup, sizeof(startup), f) == NULL
     || fgets(shutdown, sizeof(shutdown), f) == NULL
     || fgets(baudline, sizeof(baudline), f) == NULL)
    {
        Error("Unexpected error reading from '%s'", modem_config_file);
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

    atexit(HangupModem);

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
    atexit(HangupModem);

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
    StringFlag("-modemcfg", &modem_config_file, "filename",
               "specify config file for modem");
    BoolFlag("-player1", &force_player1, "force this side to be player 1");
    SerialRegisterFlags();
    NetRegisterFlags();

    args = ParseCommandLine(argc, argv);
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

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
    atexit(ShutdownPort);

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
}
