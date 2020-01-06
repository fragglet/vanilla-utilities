#include <copyright.h>
#include <wattcp.h>

/*
 * sock_sselect - returns one of several constants indicating
 *                SOCKESTABLISHED - tcp connection has been established
 *                SOCKDATAREAY    - tcp/udp data ready for reading
 *                SOCKCLOSED      - socket has been closed
 */

int sock_sselect( sock_type *s, int waitstate )
{
    /* are we connected ? */
    if ( waitstate == SOCKDATAREADY )
        if ( s->tcp.rdatalen ) return( SOCKDATAREADY );
    if ( s->tcp.ip_type == 0 ) return( SOCKCLOSED );
    if ( waitstate == SOCKESTABLISHED ) {
        if ( s->tcp.ip_type == UDP_PROTO ) return( SOCKESTABLISHED );
        if ( s->tcp.state == tcp_StateESTAB ||
             s->tcp.state == tcp_StateESTCL ||
             s->tcp.state == tcp_StateCLOSWT )
             return( SOCKESTABLISHED );

    }
    return( 0 );
}

