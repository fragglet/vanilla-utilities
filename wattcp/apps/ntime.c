
/******************************************************************************
    NTIME - set dos clock from internet
            see RFC 868

    Copyright (C) 1991 Erick Engelke
    portions Copyright (C) 1990, National Center for Supercomputer Applications

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
#include <dos.h>
#include <time.h>
#include <tcp.h>

#define TCP_TIME 1


/* Notes:
 * The time is the number of seconds since 00:00 (midnight) 1 January 1900
 * GMT, such that the time 1 is 12:00:01 am on 1 January 1900 GMT; this
 * base will serve until the year 2036.
 *
 * For example:
 *
 *     2,208,988,800L corresponds to 00:00  1 Jan 1970 GMT, start of UNIX time
 *
 */

#define TIME_PORT 37
#define BASE_TIME 2208988800L

/*
 * ntime() given the host address, returns an Internet based time, not an
 *         UNIX or DOS time.  The UNIX time may be derived by subtracting
 *	   BASE_TIME from the returned value.
 */

long ntime( longword host )
{
    static tcp_Socket telsock;
    tcp_Socket *s;
    int status;
    long temptime;

    s = &telsock;
    status = 0;
    temptime = 0L;

#ifdef TCP_TIME
    if (!tcp_open( s, 0, host, TIME_PORT, NULL )) {
	puts("Sorry, unable to connect to that machine right now!");
	return( 1 );
    }
    printf("waiting...\r");
    sock_wait_established(s, sock_delay , NULL, &status);
    printf("connected \n");
#else
    if (!udp_open( s, 0, host, TIME_PORT, NULL )) {
	puts("Sorry, unable to connect to that machine right now!");
	return( 1 );
    }
    sock_write( s, "\n", 1 );
#endif TCP_TIME

    while ( 1 ) {
	sock_tick( s, &status );

	if (sock_dataready( s ) >= 4 ) {
	    sock_read( s, (byte *)&temptime, sizeof( long ));

	    temptime = ntohl( temptime );	/* convert byte orderring */
	    sock_close( s );
#if 1
return( temptime );             /* TODO: ??? */
#else
	    sock_wait_closed( s, sock_delay, NULL, &status );
	    break;
#endif
	}
    }

sock_err:
    switch (status) {
	case 1 : /* foreign host closed */
		 return( temptime );
	case -1: /* timeout */
		 printf("\nConnection timed out!");
		 return( 0 );
	default: printf("Aborting");
		 return( 0 );
    }
}


int main(int argc, char **argv )
{
    longword host;
    longword newtime;
    longword addminutes;
    struct date dstruct;
    struct time tstruct;

    if (argc < 2) {
	puts("   DAYTIME  server  [addminutes]");
	exit( 3 );
    }

    if (argc == 3 )
	addminutes = atol( argv[2] ) * 60L;
    else
	addminutes = 0L;

    sock_init();

    if ( (host = resolve( argv[1])) != 0uL ) {
	if ( (newtime = ntime( host )) != 0uL ) {
	    newtime = newtime - BASE_TIME + addminutes;	/* now in UNIX format */
	    unixtodos( newtime, &dstruct, &tstruct );
	    settime( &tstruct );
	    setdate( &dstruct );
	    printf("Time set to %s", ctime( (time_t *)&newtime ));
	    exit( 0 );
	}
	printf("Unable to get the time from that host\n");
	exit( 1 );
    }

    printf("Could not resolve host '%s'\n", argv[1]);
    exit( 3 );
    return (0);  /* not reached */
}
