// sersetup.c

#include "sersetup.h"
#include "DoomNet.h"

extern que_t inque, outque;

void jump_start(void);
extern int uart;

int usemodem;
char startup[256], shutdown[256];

void ModemCommand(char *str);

/*
================
=
= write_buffer
=
================
*/

void write_buffer(char *buffer, unsigned int count)
{
    int i;

    // if this would overrun the buffer, throw everything else out
    if (outque.head - outque.tail + count > QUESIZE)
        outque.tail = outque.head;

    while (count--)
        write_byte(*buffer++);

    if (INPUT(uart + LINE_STATUS_REGISTER) & 0x40)
        jump_start();
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
        printf("\n");
        printf("\nDropping DTR\n");
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

    printf("Clean exit from SERSETUP\n");
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
int packetlen;
int inescape;
int newpacket;

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
        c = read_byte();
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

    write_buffer(localbuffer, b);
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
        //I_ColorBlack (0,0,63);
        WritePacket((char *)&doomcom.data, doomcom.datalength);
    }
    else if (doomcom.command == CMD_GET)
    {
        //I_ColorBlack (63,63,0);

        if (ReadPacket() && packetlen <= sizeof(doomcom.data))
        {
            doomcom.remotenode = 1;
            doomcom.datalength = packetlen;
            memcpy(&doomcom.data, &packet, packetlen);
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

void Connect(void)
{
    struct time time;
    int oldsec;
    int localstage, remotestage;
    char str[20];

    //
    // wait for a good packet
    //
    printf
        ("Attempting to connect across serial link, press escape to abort.\n");

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
    printf("Modem command : %s\n", str);
    write_buffer(str, strlen(str));
    write_buffer("\r", 1);
}

/*
==============
=
= ModemResponse
=
= Waits for OK, RING, CONNECT, etc
==============
*/

char response[80];

void ModemResponse(char *resp)
{
    int c;
    int respptr;

    do
    {
        printf("Modem response: ");
        respptr = 0;
        do
        {
            while (bioskey(1))
            {
                if ((bioskey(0) & 0xff) == 27)
                    Error("\nModem response aborted.");
            }
            c = read_byte();
            if (c == -1)
                continue;
            if (c == '\n' || respptr == 79)
            {
                response[respptr] = 0;
                printf("%s\n", response);
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
= ReadLine
=
=============
*/

void ReadLine(FILE *f, char *dest)
{
    int c;

    do
    {
        c = fgetc(f);
        if (c == EOF)
            Error("EOF in modem.cfg");
        if (c == '\r' || c == '\n')
            break;
        *dest++ = c;
    } while (1);
    *dest = 0;
}

/*
=============
=
= InitModem
=
=============
*/

void InitModem(void)
{
    int mcr;
    FILE *f;

    f = fopen("modem.cfg", "r");
    if (!f)
        Error("Couldn't read MODEM.CFG");
    ReadLine(f, startup);
    ReadLine(f, shutdown);
    fclose(f);

    ModemCommand(startup);
    ModemResponse("OK");
}

/*
=============
=
= Dial
=
=============
*/

void Dial(void)
{
    char cmd[80];
    int p;

    usemodem = true;
    InitModem();

    printf("\nDialing...\n\n");
    p = CheckParm("-dial");
    sprintf(cmd, "ATDT%s", _argv[p + 1]);

    ModemCommand(cmd);
    ModemResponse("CONNECT");
    if (strncmp(response + 8, "9600", 4))
        Error
            ("The connection MUST be made at 9600 baud, no error correction, no compression!\n"
             "Check your modem initialization string!");
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
    InitModem();
    printf("\nWaiting for ring...\n\n");

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

void main(void)
{
    int p;

    //
    // set network characteristics
    //
    doomcom.ticdup = 1;
    doomcom.extratics = 0;
    doomcom.numnodes = 2;
    doomcom.numplayers = 2;
    doomcom.drone = 0;

    printf("\n"
           "-------------------------\n"
           "DOOM SERIAL DEVICE DRIVER\n" "-------------------------\n");
    //
    // allow override of automatic player ordering to allow a slower computer
    // to be set as player 1 allways
    //
    if (CheckParm("-player1"))
        doomcom.consoleplayer = 1;
    else
        doomcom.consoleplayer = 0;

    //
    // establish communications
    //
    InitPort();

    if (CheckParm("-dial"))
        Dial();
    else if (CheckParm("-answer"))
        Answer();

    Connect();

    //
    // launch DOOM
    //
    LaunchDOOM();

#if 0
    {
        union REGS regs;

        delay(1000);
        doomcom.command = CMD_SEND;
        doomcom.datalength = 12;
        memcpy(doomcom.data, "abcdefghijklmnop", 12);
        int86(doomcom.intnum, &regs, &regs);

        delay(1000);
        doomcom.command = CMD_GET;
        doomcom.datalength = 0;
        int86(doomcom.intnum, &regs, &regs);
        printf("datalength: %i\n", doomcom.datalength);

        delay(1000);
        doomcom.command = CMD_SEND;
        doomcom.datalength = 12;
        memcpy(doomcom.data, "abcdefghijklmnop", 12);
        int86(doomcom.intnum, &regs, &regs);

        delay(1000);
        doomcom.command = CMD_GET;
        doomcom.datalength = 0;
        int86(doomcom.intnum, &regs, &regs);
        printf("datalength: %i\n", doomcom.datalength);

    }
#endif

    Error(NULL);
}
