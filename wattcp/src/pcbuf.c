#include <copyright.h>
#include <wattcp.h>
#include <mem.h>

int sock_rbsize( sock_type *s )
{
    switch( _chk_socket( s )) {
#ifdef NOTUSED	// S. Lawson - wrong if application supplied buffer
	case 1 : return( tcp_MaxBufSize );
	case 2 : return( tcp_MaxBufSize );
#else
	case 1 : return( s->udp.maxrdatalen );
	case 2 : return( s->tcp.maxrdatalen );
#endif
     /* case 0 : */
	default: return( 0 );
    }
}

int sock_rbused( sock_type *s )
{
    switch( _chk_socket( s )) {
	case 1 : return( s->udp.rdatalen );
	case 2 : return( s->tcp.rdatalen );
     /* case 0 : */
	default: return( 0 );
    }
}

int sock_rbleft( sock_type *s )
{
    switch( _chk_socket( s )) {
#ifdef NOTUSED	// S. Lawson - wrong if application supplied buffer
	case 1 : return( tcp_MaxBufSize - s->udp.rdatalen );
	case 2 : return( tcp_MaxBufSize - s->tcp.rdatalen );
#else
	case 1 : return( s->udp.maxrdatalen - s->udp.rdatalen );
	case 2 : return( s->tcp.maxrdatalen - s->tcp.rdatalen );
#endif
     /* case 0 : */
	default: return( 0 );
    }
}

int sock_tbsize( sock_type *s )
{
    switch( _chk_socket( s )) {
	case 2 : return( tcp_MaxBufSize );
	default: return( 0 );
    }
}

int sock_tbused( sock_type *s )
{
    switch( _chk_socket( s )) {
	case 2 : return( s->tcp.datalen );
	default: return( 0 );
    }
}

int sock_tbleft( sock_type *s )
{
    switch( _chk_socket( s )) {
	case 2 : return( tcp_MaxBufSize - s->tcp.datalen );
	default: return( 0 );
    }
}

int sock_preread( sock_type *s, byte *dp, int len )
{
    int count;

    if ( (count = s->udp.rdatalen) < 1)    /* 0 : no data, -1 : error */
	return( count );

    if ( count > len ) count = len;
    movmem( s->udp.rdata, dp, count );
    return( count );
}

