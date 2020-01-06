/******************************************************************************

    popdump.c - dump mail from popmail3 into spool subdirectory

    Copyright (C) 1991 Erick Engelke

    This program is free software; you can redistribute it and/or modify
    it, but you may not sell it.

    This program is distributed in the hope that it will be useful,
    but without any warranty; without even the implied warranty of
    merchantability or fitness for a particular purpose.

        Erick Engelke                   or via E-Mail
        Faculty of Engineering
        University of Waterloo          Erick@development.watstar.uwaterloo.ca
        200 University Ave.,
        Waterloo, Ont., Canada
        N2L 3G1

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <tcp.h>

#define POP3_PORT 110


long localdiskspace( void )
{
    struct dfree d;
    getdfree( 0, &d );

    return( (longword) d.df_avail * (longword)d.df_bsec * (longword)d.df_sclus );
}

tcp_Socket popsock;
char buffer[ 513 ];

/* getnumbers - returns the count of numbers received */
int getnumbers( char *ascii, long *d1, long *d2 )
{
    char *p;
    /* it must return a number after the white space */
    if (( p = strchr( ascii, ' ')) == NULL ) return( 0 );

    /* skip space */
    while ( *p == ' ') p++;
    *d1 = atol( p );

    if (( p = strchr( p, ' ')) == NULL ) return( 1 );

    /* skip space */
    while ( *p == ' ') p++;
    *d2 = atol( p );
    return( 2 );
}


int popdump( char *userid, char *password, longword host)
    /*, char *hoststring, char *dumpfile)  /* 94.11.19 -- removed extra params */
{
    tcp_Socket *s;
    int status;
/*    int len;  */
/*    char *p;  */
    long process = 0, count, totallength, locallength, dummy;
/*    FILE *f; */

    s = &popsock;
    if (!tcp_open( s, 0, host, POP3_PORT, NULL )) {
	puts("Sorry, unable to connect to that machine right now!");
	return (1);
    }

    printf("waiting...\r");

    sock_mode( s, TCP_MODE_ASCII );
    sock_wait_established(s, sock_delay, NULL, &status);
    sock_wait_input( s, sock_delay, NULL, &status );
    sock_gets( s, buffer, sizeof( buffer ));
    puts(buffer);
    if ( *buffer != '+' ) goto quit;

    sock_printf( s, "USER %s", userid);
    sock_wait_input( s, sock_delay, NULL, &status );
    sock_gets( s, buffer, sizeof( buffer ));
    puts(buffer);
    if ( *buffer != '+' ) goto quit;

    sock_printf( s, "PASS %s", password );
    sock_wait_input( s, sock_delay, NULL, &status );
    sock_gets( s, buffer, sizeof( buffer ));
    puts(buffer);
    if ( *buffer != '+' ) goto quit;

    sock_printf(s, "STAT");
    printf("STAT\n");
    sock_wait_input( s, sock_delay, NULL, &status );
    sock_gets( s, buffer, sizeof( buffer ));
    puts(buffer);
    if ( *buffer != '+' ) goto quit;

    /* it must return two valid numbers */
    if ( getnumbers( buffer, &count, &totallength ) < 2 ) {
        printf("protocol error on STAT\n");
        goto quit;
    }

    printf("Attempting to download %lu messages (%lu bytes)\n",
        count, totallength );

    while ( process++ < count ) {
        printf("Getting file # %lu\n", process );

        sock_printf( s, "LIST %lu", process );
        sock_wait_input( s, sock_delay, NULL, &status );
        sock_gets( s, buffer, sizeof( buffer ));
        if ( getnumbers( buffer, &dummy, &locallength ) < 2 ) {
            printf("protocol error on LIST %lu\n", process );
            goto quit;
        }

        if ( localdiskspace() < locallength * 2 ) {
            printf("Skipping file # %lu, too big for disk space available\n",
                process );
            continue;
        }
        sock_printf( s, "RETR %lu", process );
        sock_wait_input( s, sock_delay, NULL, &status );
        sock_gets( s, buffer, sizeof( buffer ));
        if (*buffer != '+' ) goto quit;

/*
        sprintf( buffer, "%s%s%lu.mai",
            dumpfile, dumpfile ? "\\":".\\", index

        if (( f = fopen( dumpfile , "wt" )) == NULL ) {
            printf("Unable to open %s\n", dumpfile );
            return;
        }
*/
        do {
            sock_wait_input( s, sock_delay, NULL, &status );
            sock_gets( s, buffer, sizeof( buffer ));
            puts( buffer );
        } while ( buffer[0] != '.' || buffer[1] != 0 );
        sock_printf(s,"DELE %lu", process );
        sock_wait_input( s, sock_delay, NULL, &status );
        sock_gets( s, buffer, sizeof( buffer ));
        puts(buffer);
        if ( *buffer != '+' ) goto quit;
    }
quit:
    sock_puts(s,"QUIT");
    sock_close( s );
    sock_wait_closed( s, sock_delay, NULL, &status );

sock_err:
    switch (status) {
	case 1 : /* foreign host closed */
		 break;
	case -1: /* timeout */
                 printf("ERROR: %s\n", sockerr(s));
		 break;
    }
    printf("\n");
    return ( (status == -1) ? 2 : status );
}


int main(int argc, char **argv )
{
    char user[128], password[64], *server;
    longword host;
    int status;

    if ( argc < 2 ) {
        puts("popdump userid@server password");
        exit(3);
    }

    sock_init();

    strncpy( user, argv[1], sizeof(user)-1 );
    user[ sizeof(user) -1 ] = 0;
    strncpy( password, argv[2], sizeof(password)-1 );
    password[ sizeof(password) -1 ] = 0;

    if ( (server = strchr( user, '@' ))== NULL) {
        printf("missing @server part of userid: %s\n", user );
        exit( 3 );
    }

    *server++ = 0;
    if ( (host = resolve( server )) != 0uL ) {
        status = popdump( user, password, host /*, server*/);
    } else {
	printf("Could not resolve host '%s'\n", server );
	exit( 3 );
    }
    exit( status );
    return (0);  /* not reached */
}

