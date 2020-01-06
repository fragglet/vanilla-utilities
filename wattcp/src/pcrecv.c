#include <wattcp.h>
#include <mem.h>

#define RECV_USED    0xf7e3d2b1L
#define RECV_UNUSED  0xe6d2c1afL

typedef struct {
    longword 	 recv_sig;
    byte	*recv_bufs;
    word	 recv_bufnum;	/* for udp */
    word	 recv_len;	/* for tcp */
} recv_data;

typedef struct {
    longword	 buf_sig;
    longword	 buf_hisip;
    word         buf_hisport;
    int		 buf_len;
    byte	 buf_data[ 1500 ];
} recv_buf;


/*
 *  recvd - gets upcalled when data arrives
 */
static int _recvdaemon( sock_type *s, byte *data, int len, tcp_PseudoHeader *ph, void *h)
{
    recv_data *r;
    recv_buf *p;
    tcp_Socket *t;
    int i;
    udp_Header *uh;

    /* check for length problem */
    if ( !len || (len >= 1500 )) return( 0 );

    switch ( s->udp.ip_type ) {
	case UDP_PROTO :r = (recv_data*)(s->udp.rdata);
                        uh = (udp_Header *) h;
			if (r->recv_sig != RECV_USED ) {
			    outs("ERROR: udp recv data conflict");
			    return( 0 );
			}

			p = (recv_buf *)(r->recv_bufs);

			/* find a buffer */
			for ( i = 0; i < r->recv_bufnum ; ++i , ++p) {
			    switch ( p->buf_sig ) {
				case RECV_USED   : break;
				case RECV_UNUSED :
					/* take this one */
					p->buf_sig = RECV_USED;
					p->buf_len = len;
					p->buf_hisip = ph->src;
                                        p->buf_hisport = uh->srcPort;
					movmem( data, p->buf_data, len );
					return( 0 );
				default	   :
					outs("ERROR: sock_recv_init data err");
					return( 0 );
			    }
			}
                        break;
    case TCP_PROTO :
            t= (tcp_Socket*)s;
            r = (recv_data*)(t->rdata);
			if (r->recv_sig != RECV_USED ) {
			    outs("ERROR: udp recv data conflict");
			    return( 0 );
			}

			/* stick it on the end if you can */
            i = t->maxrdatalen - t->rdatalen;
			if ( i > 1 ) {
			    /* we can accept some of this */
			    if ( len > i ) len = i;
			    movmem( data, &r->recv_bufs[t->rdatalen], len );
			    t->rdatalen += len;
			    return( len );
			}
                        break;
    }
    return( 0 );	/* didn't take none */
}


int sock_recv_init( sock_type *s, void *space, word len )
{
/*    tcp_Socket *t; */
    recv_buf *p;
    recv_data *r;
    int i;

    /* clear data area */
    memset( p = space, 0, len );

    s->udp.dataHandler = (dataHandler_t)_recvdaemon;
    /* clear table */
    memset( r = (recv_data*) s->udp.rddata, 0, tcp_MaxBufSize );

    r->recv_sig = RECV_USED;
    r->recv_bufs = (byte *) p;
    r->recv_bufnum = len / sizeof( recv_buf );
    r->recv_len = len;
    if (s->udp.ip_type == UDP_PROTO )
	for ( i = 0 ; i < r->recv_bufnum ; ++i, ++p )
	    p->buf_sig = RECV_UNUSED;
    return( 0 );
}

int sock_recv_from( sock_type *s, long *hisip, word *hisport, char *buffer, int len, word flags )
{
    tcp_Socket *t;
    recv_data *r;
    recv_buf *p;
    int i;
flags=flags;            /* get rid of warning */
    r = (recv_data *) s->udp.rdata;
    if (r->recv_sig != RECV_USED )
	return( -1 );

    switch ( s->udp.ip_type ) {
	case UDP_PROTO :
	    p = (recv_buf *) r->recv_bufs;

	    /* find a buffer */
	    for ( i = 0; i < r->recv_bufnum ; ++i , ++p) {
		switch ( p->buf_sig ) {
		    case RECV_UNUSED:
			break;
		    case RECV_USED  :
			p->buf_sig = RECV_USED;
			if ( len > p->buf_len ) len = p->buf_len;
			movmem( p->buf_data, buffer, len );
                        if (hisip) *hisip = p->buf_hisip;
                        if (hisport) *hisport = p->buf_hisport;
			p->buf_sig = RECV_UNUSED;
			return( len );
		    default	  :
			outs("ERROR: sock_recv_init data err");
			return( 0 );
		}
	    }
	    return( 0 );
	case TCP_PROTO :
	    t = (tcp_Socket *) s;
	    if ( len > t->rdatalen ) len = t->rdatalen;
	    if ( len )
		movmem( r->recv_bufs, buffer, len );
	    return( len );
    }
    return( 0 );
}

int sock_recv( sock_type *s, char *buffer, int len, word flags )
{
    tcp_Socket *t;
    recv_data *r;
    recv_buf *p;
    int i;

flags=flags;            /* get rid of warning */
    r = (recv_data *) s->udp.rdata;
    if (r->recv_sig != RECV_USED )
	return( -1 );

    switch ( s->udp.ip_type ) {
	case UDP_PROTO :
	    p = (recv_buf *) r->recv_bufs;

	    /* find a buffer */
	    for ( i = 0; i < r->recv_bufnum ; ++i , ++p) {
		switch ( p->buf_sig ) {
		    case RECV_UNUSED:
			break;
		    case RECV_USED  :
			p->buf_sig = RECV_USED;
			if ( len > p->buf_len ) len = p->buf_len;
			movmem( p->buf_data, buffer, len );
			p->buf_sig = RECV_UNUSED;
			return( len );
		    default	  :
			outs("ERROR: sock_recv_init data err");
			return( 0 );
		}
	    }
	    return( 0 );
	case TCP_PROTO :
	    t = (tcp_Socket*) s;
	    if ( len > t->rdatalen ) len = t->rdatalen;
	    if ( len )
		movmem( r->recv_bufs, buffer, len );
	    return( len );
    }
    return( 0 );
}

