#include <copyright.h>
#include <stdio.h>
#include <wattcp.h>
#include <stdlib.h>    /* itoa */
#include <string.h>
#include <elib.h>

/*
 * PCBSD - provide some typical BSD UNIX functionality
 * Erick Engelke, Feb 22, 1991
 */

/*
 * chk_socket - determine whether a real socket or not
 *
 */
int _chk_socket( sock_type *s )
{
    if ( s->tcp.ip_type == TCP_PROTO ) {
	if ( s->tcp.state <= tcp_StateCLOSED)	/* skips invalid data */
	    return( 2 );
    }
    if ( s->udp.ip_type == UDP_PROTO ) return( 1 );
    return( 0 );
}

char *inet_ntoa( char *s, longword x )
{

    itoa( (int) (x >> 24), s, 10 );
    strcat( s, ".");
    itoa( (int) (x >> 16) & 0xff, strchr( s, 0), 10);
    strcat( s, ".");
    itoa( (int) (x >> 8) & 0xff, strchr( s, 0), 10);
    strcat( s, ".");
    itoa( (int) (x) & 0xff, strchr( s, 0), 10);
    return( s );
}

longword inet_addr( char *s )
{
    return( isaddr( s ) ? aton( s ) : 0 );
}

char *sockerr( sock_type *s )
{
    if ( strlen( s->tcp.err_msg ) < 80 )
	return( s->tcp.err_msg );
    return( NULL );
}

#ifdef NOTUSED		// S. Lawson - not even close anymore!
static char *sock_states[] = {
    "Listen","SynSent","SynRec","Established","FinWt1","FinWt2","ClosWt","LastAck"
    "TmWt","Closed"};
#else
static char *sock_states[] = {
    "Listen","SynSent","SynRcvd","Established","EstClosing","FinWait1",
    "FinWait2","CloseWait","Closing","LastAck","TimeWait","CloseMSL",
    "Closed"};
#endif

char *sockstate( sock_type *s )
{
    switch ( _chk_socket( s )) {
       case  1 : return( "UDP Socket" );
       case  2 : return( sock_states[ s->tcp.state ] );
       default : return( "Not an active socket");
    }
}

longword gethostid(void)
{
    return( my_ip_addr );
}

longword sethostid( longword ip )
{
    return( my_ip_addr = ip );
}

word ntohs( word a )
{
    return( intel16(a) );
}

word htons( word a )
{
    return( intel16(a) );
}

longword ntohl( longword x )
{
    return( intel( x ));
}

longword htonl( longword x )
{
    return( intel( x ));
}

