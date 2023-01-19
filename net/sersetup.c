#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/serarb.h"
#include "net/serport.h"

static struct
{
    int (*poll_func)(void);
    void (*callback)(void);
    const char *check_abort_message;
} eventloop;

static struct
{
    const char *expected_response;
    char buf[80];
    int buf_len;
    int complete;
} modemresp;

extern que_t inque, outque;

void JumpStart(void);
extern int uart;

static doomcom_t doomcom;
static char *modem_config_file = "modem.cfg";
static char startup[256], shutdown[256];
static long baudrate = 9600;

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

static void PollEventLoop(void)
{
    if (eventloop.poll_func() != 0)
    {
        eventloop.poll_func = NULL;
        if (eventloop.callback != NULL)
        {
            eventloop.callback();
        }
    }
}

static void EventLoop(void)
{
    while (eventloop.poll_func != NULL)
    {
        CheckAbort(eventloop.check_abort_message);
        PollEventLoop();
    }
}

static void CallOnSuccess(const char *message, int (*poll_func)(void),
                          void (*callback)(void))
{
    eventloop.check_abort_message = message;
    eventloop.poll_func = poll_func;
    eventloop.callback = callback;
}

static void ModemCommand(char *str)
{
    LogMessage("Modem command: %s", str);
    WriteBuffer(str, strlen(str));
    WriteBuffer("\r", 1);
}

// Modem response code, that waits until a particular answer is received
// from the modem in response to a command (eg. OK, RING, CONNECT).

static int PollModemResponse(void)
{
    int c;

    while (!modemresp.complete)
    {
        for (;;)
        {
            c = ReadByte();
            if (c == -1)
            {
                // No more bytes to process.
                return 0;
            }
            if (c == '\n' || modemresp.buf_len == sizeof(modemresp.buf) - 1)
            {
                modemresp.buf[modemresp.buf_len] = '\0';
                break;
            }
            if (c >= ' ')
            {
                modemresp.buf[modemresp.buf_len] = c;
                ++modemresp.buf_len;
            }
        }

        LogMessage("Modem response: %s", modemresp.buf);
        modemresp.complete =
            !strncmp(modemresp.buf, modemresp.expected_response,
                     strlen(modemresp.expected_response));
        modemresp.buf_len = 0;
    }

    return 1;
}

static void OnModemResponse(const char *resp, void (*and_then)(void))
{
    modemresp.expected_response = resp;
    modemresp.buf_len = 0;
    modemresp.complete = 0;
    CallOnSuccess("Modem response", PollModemResponse, and_then);
}

// Blocking version that waits until the reply is received.
static void ModemResponse(char *resp)
{
    OnModemResponse(resp, NULL);
    EventLoop();
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

static void Connected(void)
{
    StartArbitratePlayers(&doomcom, NetCallback);
    CallOnSuccess("Connection", PollArbitratePlayers, NULL);
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

static void RingDetected(void)
{
    ModemCommand("ATA");
    OnModemResponse("CONNECT", Connected);
}

void Answer(void)
{
    atexit(HangupModem);

    ModemCommand(startup);
    ModemResponse("OK");
    LogMessage("Waiting for ring...");

    OnModemResponse("RING", RingDetected);
    EventLoop();

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
    RegisterArbitrationFlags();
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
        ArbitratePlayers(&doomcom, NetCallback);
    }
    else if (answer)
    {
        Answer();
    }
    else
    {
        // Null modem / direct serial connection
        ArbitratePlayers(&doomcom, NetCallback);
    }

    // launch DOOM
    NetLaunchDoom(&doomcom, args, NetCallback);
}
