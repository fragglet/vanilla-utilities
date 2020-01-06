
/******************************************************************************
    DAYTIME - read and print time of day string from internet

    Copyright (C) 1991 Erick Engelke
    Portions Copyright (C) 1990, National Center for Supercomputer Applications

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

    Returns:
        0   - success

        2   - some failure in the connection (port unavailable,
                no response, etc.)
        3   - unable to reach it - local host or first router is down

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <tcp.h>



#define DAYTIME_PORT 13

int daytime(longword host)
{
#ifdef TCP_DAYTIME
    static tcp_Socket sock;
    tcp_Socket *s;
#else
    static udp_Socket sock;
    udp_Socket *s;
#endif
    char buffer[ 513 ];
    int retcode = 3;
    int status;
    int udpretries = 3;
    long udpretrytime;
/*    int len; */

    s = &sock;
    status = 0;
#ifdef TCP_DAYTIME
    if (!tcp_open( s, 0, host, DAYTIME_PORT, NULL )) {
	puts("Sorry, unable to connect to that machine right now!");
        return( 3 );
    }
    printf("waiting...\r");
    sock_wait_established(s, sock_delay , NULL, &status);
    printf("connected \n");
#else
    if (!udp_open( s, 0, host, DAYTIME_PORT, NULL )) {
	puts("Sorry, unable to connect to that machine right now!");
        return( 3 );
    }
    sock_write( s, "\n", 1 );
    udpretrytime = set_timeout( 2 );
#endif TCP_DAYTIME

    while ( 1 ) {
        sock_tick( s, &status );

#ifndef TCP_DAYTIME
        if ( chk_timeout( udpretrytime )) {
            if ( udpretries-- == 0 ) break;
            udpretrytime = set_timeout( 2 );
            sock_write( s, "\n", 1 );
        }
#endif
	if (sock_dataready( s ) ) {
	    sock_gets( s, buffer, sizeof( buffer ));
	    puts( buffer );
            retcode = 0;
	    break;
	}
    }
    sock_close( s );
    sock_wait_closed( s, sock_delay, NULL, &status );
    return(0);
    
sock_err:
    switch (status) {
	case 1 : /* foreign host closed */
                 return(retcode);
	case -1: /* timeout */
		 printf("\nConnection timed out!");
                 return(2);
	default: printf("Aborting");
                 return(2);
    }
}


int main(int argc, char **argv )
{
    int status;
    longword host;

    if (argc != 2) {
	puts("   DAYTIME server");
	exit( 3 );
    }

    sock_init();

    if ( (host = resolve( argv[1])) != 0uL )
	status = daytime( host );
    else {
	printf("Could not resolve host '%s'\n", argv[1]);
	status = 3;
    }

    exit( status );
    return (0);  /* not reached */
}
