#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"
#include "net/serport.h"
#include "net/doomnet.h"

extern que_t inque, outque;

void JumpStart(void);
extern int uart;

static int force_player1 = 0, force_player2 = 0;
static doomcom_t doomcom;
static char *modem_config_file = "modem.cfg";
static char startup[256], shutdown[256];
static long baudrate = 9600;

void ModemCommand(char *str);

void WriteBuffer(char *buffer, unsigned int count)
{
    // if this would overrun the buffer, throw everything else out
    if (outque.head - outque.tail + count > QUESIZE)
    {
        outque.tail = outque.head;
    }

    while (count--)
    {
        WriteByte(*buffer++);
    }

    if (INPUT(uart + LINE_STATUS_REGISTER) & 0x40)
    {
        JumpStart();
    }
}

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
    if (inque.head - inque.tail > QUESIZE - 4)
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
        {
           // haven't read a complete packet
            return 0;
        }
        //printf ("%c",c);
        if (inescape)
        {
            inescape = 0;
            if (c != FRAMECHAR)
            {
                // got a good packet
                newpacket = 1;
                return 1;
            }
        }
        else if (c == FRAMECHAR)
        {
            // don't know yet if it is a terminator or a literal FRAMECHAR
            inescape = 1;
            continue;
        }

        if (packetlen >= MAXPACKET)
        {
            // oversize packet
            continue;
        }
        packet[packetlen] = c;
        packetlen++;
    } while (1);
}

void WritePacket(char *buffer, int len)
{
    int b;
    char static localbuffer[MAXPACKET * 2 + 2];

    b = 0;
    if (len > MAXPACKET)
    {
        return;
    }

    while (len--)
    {
        if (*buffer == FRAMECHAR)
        {
            // escape for literal
            localbuffer[b++] = FRAMECHAR;
        }
        localbuffer[b++] = *buffer++;
    }

    localbuffer[b++] = FRAMECHAR;
    localbuffer[b++] = 0;

    WriteBuffer(localbuffer, b);
}

static void NetCallback(void)
{
    if (doomcom.command == CMD_SEND)
    {
        WritePacket((char *)doomcom.data, doomcom.datalength);
    }
    else if (doomcom.command == CMD_GET)
    {
        if (ReadPacket() && packetlen <= sizeof(doomcom.data))
        {
            doomcom.remotenode = 1;
            doomcom.datalength = packetlen;
            memcpy(doomcom.data, packet, packetlen);
        }
        else
        {
            doomcom.remotenode = -1;
        }
    }
}

static void MakeLocalID(char *buf)
{
    uint32_t id;

    if (force_player1)
    {
        id = 0;
    }
    else if (force_player2)
    {
        id = 999999UL;
    }
    else
    {
        id = (rand() << 16) | rand();
        id = id % 1000000L;
    }
    sprintf(buf, "%.6ld", id);
}

// Figure out who is player 0 and 1
void Connect(void)
{
    clock_t last_time = 0, now;
    int localstage, remotestage;
    char localid[7], remoteid[7];
    int remoteplayer;
    int new_protocol = 1;
    char str[20];

    // allow override of automatic player ordering
    if (force_player1)
    {
        doomcom.consoleplayer = 0;
    }
    else if (force_player2)
    {
        doomcom.consoleplayer = 1;
    }

    MakeLocalID(localid);

    // wait for a good packet
    LogMessage("Attempting to connect across serial link");
    localstage = 0;
    remotestage = 0;

    do
    {
        CheckAbort("Serial port synchronization");

        while (ReadPacket())
        {
            packet[packetlen] = 0;

            if (sscanf(packet, "ID%6c_%d", remoteid, &remotestage) == 2)
            {
                new_protocol = 1;
                remoteid[6] = '\0';
                if (!memcmp(localid, remoteid, 6))
                {
                    Error("Duplicate ID string received");
                }
            }
            else if (sscanf(packet, "PLAY%d_%d",
                            &remoteplayer, &remotestage) == 2)
            {
                new_protocol = 0;

                // The original sersetup code would swap the player number when
                // detecting a conflict; however, this is not an algorithm that
                // is guaranteed to ever terminate. In our case since we only
                // ever use the old protocol when the other side needs to, we
                // can use this asymmetry as a way of resolving the deadlock:
                // we stick to our guns and do not change player, safe in the
                // knowledge that the other side will adapt to us.
                if (remoteplayer == doomcom.consoleplayer)
                {
                    remotestage = 0;
                }
            }
            else
            {
                continue;
            }

            // We got a packet successfully. Trigger a response with new state.
            localstage = remotestage + 1;
            last_time = 0;
        }

        now = clock();
        if (now - last_time >= CLOCKS_PER_SEC)
        {
            last_time = now;
            if (new_protocol)
            {
                sprintf(str, "ID%6s_%d", localid, localstage);
            }
            else
            {
                sprintf(str, "PLAY%i_%i", doomcom.consoleplayer, localstage);
            }
            WritePacket(str, strlen(str));
        }
    } while (remotestage < 1);

    if (new_protocol)
    {
        doomcom.consoleplayer = memcmp(localid, remoteid, sizeof(localid)) > 0;
    }

    // flush out any extras
    while (ReadPacket())
    {
    }
}

void ModemCommand(char *str)
{
    LogMessage("Modem command: %s", str);
    WriteBuffer(str, strlen(str));
    WriteBuffer("\r", 1);
}

// Wait for OK, RING, CONNECT, etc
void ModemResponse(char *resp)
{
    static char response[80];
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

static void ReadModemCfg(void)
{
    char baudline[16];
    FILE *f;

    f = fopen(modem_config_file, "r");
    if (f == NULL)
    {
        LogMessage("Couldn't read modem config from '%s'. Using generic modem "
                   "settings; if these don't work well, you should supply a "
                   "configuration file with '-modemcfg'.", modem_config_file);
        // These are the settings from modem.str for "GENERIC 14.4 MODEM":
        strncpy(startup, "AT &F &C1 &D2 &Q5 &K0 S46=0", sizeof(startup));
        strncpy(shutdown, "AT Z H0", sizeof(shutdown));
        baudrate = 14400;
        return;
    }
    if (fgets(startup, sizeof(startup), f) == NULL
     || fgets(shutdown, sizeof(shutdown), f) == NULL
     || fgets(baudline, sizeof(baudline), f) == NULL)
    {
        Error("Unexpected error reading from '%s'", modem_config_file);
    }
    fclose(f);

    baudrate = strtol(baudline, NULL, 10);
    if (baudrate == 0)
    {
        Error("Error parsing baud rate '%s'", baudline);
    }
}

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

void main(int argc, char *argv[])
{
    int answer = 0;
    char *dial_no = NULL;
    char **args;

    srand(GetEntropy());

    SetHelpText("Doom serial port network device driver",
                "%s -dial 555-1212 doom.exe -deathmatch -nomonsters");
    BoolFlag("-answer", &answer, "listen for incoming call");
    StringFlag("-dial", &dial_no, "phone#",
               "dial the given phone number");
    StringFlag("-modemcfg", &modem_config_file, "filename",
               "specify config file for modem");
    BoolFlag("-player1", &force_player1, "(and -player2) force player#");
    BoolFlag("-player2", &force_player2, NULL);
    SerialRegisterFlags();
    NetRegisterFlags();

    args = ParseCommandLine(argc, argv);
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    // set network characteristics
    doomcom.ticdup = 1;
    doomcom.extratics = 0;
    doomcom.consoleplayer = 0;
    doomcom.numnodes = 2;
    doomcom.numplayers = 2;
    doomcom.drone = 0;

    if (dial_no != NULL || answer)
    {
        ReadModemCfg();
    }

    // establish communications
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

    // launch DOOM
    NetLaunchDoom(&doomcom, args, NetCallback);
}
