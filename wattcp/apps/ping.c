
/******************************************************************************
    PING - internet diagnostic tool
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
#include <string.h>
#include <conio.h>
#include <tcp.h>

longword sent = 0L;
longword received = 0L;
longword tot_delays = 0L;
longword last_rcvd = 0L;
char *name;

void stats(void)
{
    longword temp;

    puts("\nPing Statistics");
    printf("Sent        : %lu \n", sent );
    printf("Received    : %lu \n", received );
    if (sent)
	printf("Success     : %lu \%\n", (100L*received)/sent);
    if (!received)
	printf("There was no response from %s\n", name );
    else {
        temp = ( tot_delays * 2813L)/(512L*received + 1);
	printf("Average RTT : %lu.%02lu seconds\n", temp / 100L, temp % 100L);
    }
    exit( received ? 0 : 1 );
}

void help(void)
{
    puts("PING [-s|/s] [-d|/d] hostname [number]");
    exit( 3 );
}


int main(int argc, char **argv)
{
    longword host, timer, new_rcvd;
    longword tot_timeout = 0L, itts = 0L, send_timeout = 0L;
    word i;
    word sequence_mode = 0, is_new_line = 1;
    word debug = 0;
    unsigned char tempbuffer[255];

    sock_init();

    if ( argc < 2 )
	help();

    name = NULL;
    for ( i = 1; i < argc ; ++i ) {
	if ( !stricmp( argv[i], "-d") || !stricmp( argv[i],"/d")) {
	    puts("Debug mode activated");
            debug = 1;
	    tcp_set_debug_state( 1 );
	} else if ( !stricmp( argv[i], "-s") || !stricmp( argv[i],"/s"))
	    sequence_mode = 1;
	else if ( !name )
	    name = argv[i];
	else {
	    sequence_mode = 1;
	    itts = atol( argv[i] );
	}
    }
    if (!name)
	help();

    if (!(host = resolve( name ))) {
	printf("Unable to resolve '%s'\n", name );
	exit( 3 );
    }
    if ( isaddr( name ))
	printf("Pinging [%s]",inet_ntoa(tempbuffer, host));
    else
	printf("Pinging '%s' [%s]",name, inet_ntoa(tempbuffer, host));

    if (itts) printf(" %u times", itts);
    else
	itts = sequence_mode ? 0xffffffffL : 1;

    if (sequence_mode) printf(" once per_second");
    printf("\n");

    tot_timeout = set_timeout( itts + 10 );

    _arp_resolve( host, (eth_address *)tempbuffer, 0 );   /* resolution it before timer starts */
    if ( debug ) printf("ETH -> %x %x %x %x %x %x\n",
        tempbuffer[0],tempbuffer[1],tempbuffer[2],tempbuffer[3],
        tempbuffer[4],tempbuffer[5]);


    do {
	/* once per second - do all this */
	if ( chk_timeout( send_timeout ) || !send_timeout ) {
	    send_timeout = set_timeout( 1 );
	    if ( chk_timeout( tot_timeout ) && tot_timeout )
		stats();
	    if ( sent < itts ) {
		sent++;
		if (_ping( host , sent ))
		    stats();
		if (!is_new_line) printf("\n");
		printf("sent PING # %lu ", sent );
		is_new_line = 0;
	    }
	}

	if ( kbhit() ) {
	    getch();		/* trash the character */
	    stats();
	}

	tcp_tick(NULL);
	if ((timer = _chk_ping( host , &new_rcvd)) != 0xffffffffL) {
	    tot_delays += timer;
	    ++received;
	    if ( new_rcvd != last_rcvd + 1 ) {
		if (!is_new_line) printf("\n");
		puts("PING receipt received out of order!");
		is_new_line = 1;
	    }
	    last_rcvd = new_rcvd;
	    if (!is_new_line) printf(", ");
	    printf("PING receipt # %lu : response time %lu.%02lu seconds\n", received, timer / 18L, ((timer %18L)*55)/10 );
	    is_new_line = 1;
	    if ( received == itts )
		stats();
	}
    } while (1);
}
