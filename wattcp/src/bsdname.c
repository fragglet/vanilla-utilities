#include <copyright.h>
#include <stdio.h>
#include <wattcp.h>
#include <stdlib.h>    /* itoa */
#include <string.h>
#include <elib.h>

int getpeername( sock_type *s, void *dest, int *len )
{
    struct sockaddr temp;
    int ltemp;

    memset( &temp, 0, sizeof( struct sockaddr ));
    temp.s_ip = s->tcp.hisaddr;
    temp.s_port = s->tcp.hisport;

    if (!s->tcp.hisaddr || !s->tcp.hisport || ! _chk_socket( s )) {
        if (len) *len = 0;
        return( -1 );
    }

    /* how much do we move */
    ltemp = (len) ? *len : sizeof( struct sockaddr );
    if (ltemp > sizeof( struct sockaddr)) ltemp = sizeof( struct sockaddr );
    qmove( &temp, dest, ltemp );

    if (len) *len = ltemp;
    return( 0 );
}

int getsockname(  sock_type *s, void *dest, int *len )
{
    struct sockaddr temp;
    int ltemp;

    memset( &temp, 0, sizeof( struct sockaddr ));
    temp.s_ip = s->tcp.myaddr;
    temp.s_port = s->tcp.myport;

    if (!s->tcp.hisaddr || !s->tcp.hisport || ! _chk_socket( s )) {
        if (len) *len = 0;
        return( -1 );
    }

    /* how much do we move */
    ltemp = (len) ? *len : sizeof( struct sockaddr );
    if (ltemp > sizeof( struct sockaddr)) ltemp = sizeof( struct sockaddr );
    qmove( &temp, dest, ltemp );

    if (len) *len = ltemp;
    return( 0 );
}

char *getdomainname( char *name, int length )
{
    if ( length ) {
	if ( length < strlen( def_domain ))
	    *name = 0;
	else
	    strcpy( name, def_domain );
	return( name );
    }
    return( ( def_domain && *def_domain ) ? def_domain : NULL );
}

char *setdomainname( char *string )
{
    return( def_domain = string );
}

char *gethostname( char *name, int len )
{
    if ( len ) {
	if (len < strlen( _hostname ))
	    *name = 0;
	else
	    strcpy( name, _hostname );
	return( name );
    }
    return( ( _hostname && *_hostname ) ?  _hostname  : NULL );
}

char *sethostname( char *name )
{
    return( _hostname = name );
}

void psocket( sock_type *s )
{
    char buffer[255];

    outch( '[' );
    outs( inet_ntoa( buffer, s->tcp.hisaddr) );
    outch( ':' );
    itoa( s->tcp.hisport, buffer, 10 );
    outs( buffer );
    outch( ']' );

}
