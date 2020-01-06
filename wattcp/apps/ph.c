/*
 * PH.EXE - CSO NameServer Client
 *
 * Don Genner (acddon@vm.uoguelph.ca) wrote this clever little CSO
 * client.  You may wish to use it and a DOS batch file to set up a
 * simple phonebook command:

 *  eg.     ph  localserver  "query name=%1"
 *
 * or something much more complex.
 *
 * To get more details on the protocol and local features, use something
 * like:    ph  localserver  "help"
 *
 *
 * Syntax:  Stand-alone:    PH  CSO-NameServer Request
 *                          --------------------------
 *
 *   eg:  PH csaserver.uoguelph.ca "query name=genner return name email"
 *               {Results}
 *
 *        PH csaserver.uoguelph.ca "query name=g*"
 *              {Results}
 *
 *        PH csaserver.uoguelph.ca "query g*"
 *              {Results}
 *
 *
 * Syntax:  Conversational:   PH  CSO-NameServer
 *                            ------------------
 *
 *   or:  PH csaserver.uoguelph.ca
 *            Server : csaserver.uoguelph.ca
 *              {Results}
 *
 *           Request : query genner
 *              {Results}
 *
 *           Request : siteinfo
 *              {Results}
 *
 *           Request : fields
 *              {Results}
 *
 *           Request : quit
 *              {Results}
 *
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcp.h>

#define PH_PORT 105

tcp_Socket  phsock;
char        buffer[515];

int ph( char *cmd, longword host /*, char *hoststring*/)
{
    tcp_Socket     *s;
    int         status;
    int         len,
            prsw,
            crsw,
            i;


    s = &phsock;
    if (!tcp_open(s, 0, host, PH_PORT, NULL)) {
        puts("Sorry, unable to connect to that machine right now!");
        return (1);
    }

    printf("waiting...\r");
    sock_wait_established(s, sock_delay, NULL, &status);

    strcpy(buffer, cmd);
    rip(buffer);
    strcat(buffer, "\r\n");

    sock_puts(s, buffer);

    prsw = -1;
    crsw = -1;
    while (prsw) {
        sock_wait_input(s, 30, NULL, &status);
        len = sock_fastread(s, buffer, 512);
        for (i = 0; i < len; i++) {
            if (crsw && (buffer[i] != '-'))
            putchar(' ');
            if (crsw && (buffer[i] >= '2')) {
                prsw = 0;
            }
            putchar(buffer[i]);
            crsw = 0;
            if (buffer[i] == 0x0a) crsw = -1;
        }
    }
    sock_close( s );
    sock_wait_closed( s, sock_delay, NULL, &status );

  sock_err:
    switch (status)
    {
    case 1:         /* foreign host closed */
            break;
    case -1:            /* timeout */
            printf("ERROR: %s\n", sockerr(s));
    break;
    }
    printf("\n");
    return ( (status == -1) ? 2 : status );
}


int main(int argc, char **argv)
{
    char       *cmd,
           *server;
    char        lcmd[255];
    longword        host;
    int         status;

    sock_init();

    if ((argc < 2) || (argc > 3))
    {
    puts("   Usage: PH server [request]");
    exit(3);
    }

    server = cmd = NULL;

    server = argv[1];
    if ( (host = resolve(server)) != 0uL )
    {
    printf(" Server  : %s\n\n", server);
    }else{
    printf("Could not resolve host '%s'\n", server);
    exit(0);
    }

    if (argc == 3)
    {
    cmd = argv[2];
    status = ph(cmd, host /*, server */);
    }
    else
    {
    while(1)
    {
        printf(" Request : ");
        gets(cmd = lcmd);
        if (!*cmd)
        {
        puts("No command given\n");
        exit(2);
        }
        status = ph(cmd, host /*, server*/);
        if(strncmp(cmd , "quit" , 4) == 0)break;
        if(strncmp(cmd , "stop" , 4) == 0)break;
        if(strncmp(cmd , "exit" , 4) == 0)break;

    }
    }

    exit(status);
    return (0);  /* not reached */
}
