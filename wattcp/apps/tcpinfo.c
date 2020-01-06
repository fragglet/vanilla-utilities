/******************************************************************************

    TCPINFO - display configuration info to the screen

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
#include <stdarg.h>
#include <string.h>
#include <conio.h>
#include <tcp.h>

void mprintf( char *format, ... )
{
    static linecount = 1;

    char buffer[ 512 ];
    char *s, *p, pchar;
    va_list *argptr;

    va_start( argptr, format );
    vsprintf( s = buffer, format, argptr );
    va_end( argptr );

    do {
	if ( (p = strchr( s, '\n' )) != NULL ) {
	    pchar = *(++p);
	    *p = 0;
	}

	fputs( s , stdout );

	if ( (s = p) != NULL ) {
	    *s = pchar;
	    if (++linecount == 24 ) {
		fputs( "<press any key to continue>", stdout);
		getch();
        fputs("\r\r", stdout);
        clreol();
		linecount = 1;
	    }
	}
    } while ( s );
}


int unused = 0;
int extrahelp = 0;

static void (*other)( char *name, char *value );

static void mine(char *name, char *value)
{
    if ( !extrahelp ) {
	unused = 1;
	return;
    }

    if (!unused) {
	unused = 1;
	mprintf("\nSome extra parameters were found in your configuration file.");
	mprintf("These values may be extensions used by applications, but are");
	mprintf("not used by the Waterloo TCP kernal.\n");
    }
    mprintf("   unknown: %s = %s\n", name,value );
}

/* undocumented */
extern byte _eth_addr[];
extern word _pktdevclass;
extern word _mss;
extern longword _bootphost;
extern word _bootptimeout;
extern word _bootpon;
extern word _arp_last_gateway;
extern longword _arp_gate_data[];
extern int _last_nameserver;
extern word multihomes;


char buffer[ 512 ], buf2[512];


int main( int argc, char **argv)
{
    int i;

    while ( argc > 1 ) {
	if ( argc == 2 )
	    if (!stricmp( argv[ 1 ], "ALL")) {
		extrahelp = 1;
		break;
	    }
	mprintf("TCPINFO [ALL]");
	exit( 3 );
    }

    mprintf("Reading Waterloo TCP configuration file.\n");

    other = usr_init;
    usr_init = mine;
    _survivebootp = 1;	/* needed to not exit if bootp fails */

    sock_init();

    if ( unused && extrahelp )
	mprintf("\nThat is the end of the extra parameters\n");

    switch ( _pktdevclass ) {
	case 1 : mprintf("\nEthernet Address : %hx:%hx:%hx:%hx:%hx:%hx\n",
		     _eth_addr[0], _eth_addr[1], _eth_addr[2],
		     _eth_addr[3], _eth_addr[4], _eth_addr[5] );
		 break;
	case 6 : mprintf("Protocol         : SLIP");
		 break;
    }


    if (multihomes)
        mprintf("\nIP Addresses     : %s - %s\n", inet_ntoa( buf2, gethostid()),
            inet_ntoa( buffer, gethostid()+multihomes));
    else
        mprintf("\nIP Address       : %s\n", inet_ntoa( buffer, gethostid()));
    mprintf("Network Mask     : %s\n\n", inet_ntoa( buffer, sin_mask ));

    mprintf("Gateways         : ");
    if ( ! _arp_last_gateway ) mprintf("NONE");
    else mprintf("GATEWAY'S IP     SUBNET           SUBNET MASK\n");

    for ( i = 0 ; i < _arp_last_gateway * 3;) {
	printf("                 : %-15s  ", inet_ntoa(buffer,_arp_gate_data[i++] ));
	if ( !_arp_gate_data[i] ) {
	    mprintf("DEFAULT\n\r");
	    i += 2;
	} else {
	    mprintf("%-15s  ", inet_ntoa(buffer,_arp_gate_data[i++]));
	    mprintf("%-15s\n", inet_ntoa(buffer,_arp_gate_data[i++]));
	}
    }
    mprintf("\n");

    if ( gethostname( NULL, 0 ) ) {
	mprintf("Host name        : %s", gethostname(NULL, 0));
	if ( getdomainname( NULL, 0))
	    mprintf(".%s", getdomainname( NULL, 0));
	mprintf("\n");
    }

    mprintf("Cookieserver%c    : ", ( _last_cookie < 2 ) ? ' ' : 's');
    if ( !_last_cookie ) mprintf("NONE DEFINED\n");

    for ( i = 0 ; i < _last_cookie ; ++i ) {
	if (i) mprintf("                 : ");
	mprintf("%s\n", inet_ntoa( buffer, _cookie[i] ));
    }
    mprintf("\n");

    mprintf("Nameserver%c      : ", ( _last_nameserver < 2 ) ? ' ' : 's');
    if ( !_last_nameserver ) mprintf("NONE DEFINED\n\n");

    for ( i = 0 ; i < _last_nameserver ; ++i ) {
	if (i) mprintf("                 : ");
	mprintf("%s\n", inet_ntoa( buffer, def_nameservers[i] ));
    }
    mprintf("Domain           : \"%s\"\n\n", getdomainname( NULL, 0));

    if (_bootpon || extrahelp ) {
	mprintf("BOOTP            : %s\n", (_bootpon) ? "USED": "NOT USED");
	if (_bootpon) mprintf("                 : %s\n", gethostid() ?
		"SUCCEEDED" : "FAILED" );

	mprintf("BOOTP Server     : %s\n", ( _bootphost == 0xffffffffL ) ?
		"BROADCAST" :
		inet_ntoa( buffer, _bootphost ));
	mprintf("BOOTP Timeout    : %i seconds\n\n", _bootptimeout );
    }

    if (extrahelp) {
	mprintf("Default Timeout  : %u seconds\n", sock_delay );
    mprintf("Max Seg Size MSS : %u bytes\n\n", _mss );
    debugpsocketlen();
    }

    if ( unused && !extrahelp ) {
        mprintf("\nAdditional non-standard parameters were found in your configuration file\n");
        mprintf("If you would like to see them, try the command:\n");
        mprintf("    TCPINFO ALL\n");
    } else if ( !extrahelp ) {
        mprintf("\nAdditional but more obscure information can be found using the command:\n");
        mprintf("    TCPINFO ALL\n");
    }
    exit( 0 );
    return (0);  /* not reached */
}
