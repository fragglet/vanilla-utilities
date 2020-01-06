#include <copyright.h>
#include <wattcp.h>
#include <mem.h>
#include <dos.h>

/*
 * ICMP - RFC 792 & 1122
 */

static char *unreach[] = {
	"Network Unreachable",
	"Host Unreachable",
	"Protocol Unreachable",
	"Port Unreachable",
	// R. Whitby
	"Fragmentation Needed and DF set",
	"Source Route Failed",
	"Destination Network Unknown",
	"Destination Host Unknown",
	"Source Host Isolated",
	"Destination Network Prohibited",
	"Destination Host Prohibited",
	"Network TOS Unreachable",
	"Host TOS Unreachable" };

static char *exceed[] = {
	// R. Whitby
	"TTL Exceeded in Transit",
	"Fragment Reassembly Time Exceeded" };

static char *redirect[] = {
	"Redirect for Network",
	"Redirect for Host",
	"Redirect for TOS and Network",
	"Redirect for TOS and Host" };

/* constants */

#include <icmp.h>

// R. Whitby
extern void (*_dbugxmit)( sock_type *s, in_Header *inp, void *phdr,
	     unsigned line );
extern void (*_dbugrecv)( sock_type *s, in_Header *inp, void *phdr,
	     unsigned line );

static word icmp_id = 0;

static longword ping_hcache = 0;	/* host */
static longword ping_tcache = 0;	/* time */
static longword ping_number = 0;
extern word multihomes;

/* handler called in icmp_handler if this isn't null */
static icmp_handler_type user_icmp_handler = NULL;

longword _chk_ping( longword host, longword *ptr )
{
    if ( ping_hcache == host ) {
	ping_hcache = 0xffffffffL;
	*ptr = ping_number;
	return( ping_tcache );
    }
    return( 0xffffffffL );
}

static void icmp_print( icmp_pkt *icmp, char *msg )
{
icmp=icmp;     // get rid of warning
//    outs("\n\rICMP: ");
//    outs( msg );
//    outs("\n\r");
}

#ifdef NOTUSED		// R. Whitby
/*
 *
 */
static struct _pkt *icmp_Format( longword destip )
{
    eth_address dest;
/*    char *temp; */

    /* we use arp rather than supplied hardware address */
    /* after first ping this will still be in cache */

    if ( !_arp_resolve( destip , &dest, 0 ))
	return( NULL );				/* unable to find address */
    return( (struct _pkt*)_eth_formatpacket( &dest, 8 ));    /* &dest okay? */
}
#endif 			// R. Whitby

/*
 * icmp_Reply - format and send a reply packet
 *  	      - note that src and dest are NETWORK order not host!!!!
 */
static void icmp_Reply( struct _pkt *p, longword src, longword dest, int icmp_length )
{
    in_Header *ip;
    icmp_pkt *icmp;

    ip = &p->in;
    memset( ip, 0, sizeof( in_Header ));
    icmp = &p->icmp;

    /* finish the icmp checksum portion */
    icmp->unused.checksum = 0;
    icmp->unused.checksum = ~checksum( icmp, icmp_length );

    /* encapsulate into a nice ip packet */
    ip->ver = 4;
    ip->hdrlen = 5;
    ip->length = intel16( sizeof( in_Header ) + icmp_length);
    ip->tos = 0;
    ip->identification = intel16( icmp_id ++);	/* not using ip id */
//    ip->frag = 0;
    ip->ttl = 250;
    ip->proto = ICMP_PROTO;
    ip->checksum = 0;
    ip->source = src;
    ip->destination = dest;
    ip->checksum = ~ checksum( ip, sizeof( in_Header ));

    if (_dbugxmit) (*_dbugxmit)( NULL, ip, icmp, 0);	// R. Whitby

    _eth_send( intel16( ip->length ));
}

// S. Lawson - send ICMP port unreachable
void icmp_Unreach(in_Header *ip) {
    icmp_pkt *icmp;
    struct _pkt *pkt;
    word len;

    len = in_GetHdrlenBytes(ip)+8;
    pkt = (struct _pkt*)(_eth_formatpacket( _eth_hardware((byte*)ip), 8));
    icmp = &pkt->icmp;
    icmp->unused.type = ICMPTYPE_UNREACHABLE;
    icmp->unused.code = ICMP_UNREACH_PORT;
    icmp->unused.unused = 0L;
    movmem(ip, &icmp->unused.ip, len);
    icmp_Reply( pkt,ip->destination, ip->source, 8+len );
}

// S. Lawson - BC 2.0 doesn't have the underscore variety
#ifndef _disable
#define _disable disable
#define _enable enable
#endif

/*
 * Register the user ICMP handler.  Only one at a time...
 * To disable user handler, call  set_icmp_handler(NULL);
 */
void set_icmp_handler( icmp_handler_type user_handler )
{
   _disable();
   user_icmp_handler = user_handler;
   _enable();
}

int icmp_handler( in_Header *ip )
{
    icmp_pkt *icmp, *newicmp;
    struct _pkt *pkt;
    int len, code;
    in_Header *ret;

    len = in_GetHdrlenBytes( ip );
    icmp = (icmp_pkt*)((byte *)ip + len);

    if (_dbugrecv) (*_dbugrecv)( NULL, ip, icmp, 0);	// R. Whitby

    len = intel16( ip->length ) - len;
    if ( checksum( icmp, len ) != 0xffff ) {
// R. Whitby	outs("ICMP received with bad checksum\n\r");
	if (debug_on > 0) icmp_print(icmp, "Bad Checksum");  // R. Whitby
	return( 1 );
    }

   /*
    * If there's a user handler installed, call the user's handler;
    *     return of anything but 0 and this handler will continue
    *     processing the message after the user is done with it.
    * Otherwise, stop processing it now.
    */
    if( user_icmp_handler )
    {
	if( (user_icmp_handler)( ip ) == 0 )   /* don't continue processing? */
		return( 1 );
    }

    code = icmp->unused.code;
    ret = & (icmp->ip.ip);

    switch ( icmp->unused.type) {
	case 0 : /* icmp echo reply received */
		/* icmp_print( icmp, "received icmp echo receipt"); */

		if (debug_on > 0) icmp_print( icmp, "Echo Reply");  // R. Whitby

		/* check if we were waiting for it */
		ping_hcache = intel( ip->source );
		ping_tcache = set_timeout( 1 ) - *(longword *)(&icmp->echo.identifier );
		if (ping_tcache > 0xffffffffL)
		    ping_tcache += 0x1800b0L;
		ping_number = *(longword*)( ((byte*)(&icmp->echo.identifier)) + 4 );
		/* do more */
		break;

	case 3 : /* destination unreachable message */
// R. Whitby		if (code < 6) {
		if (code < 13) { 		// R. Whitby
		    icmp_print( icmp, unreach[ code ]);

		    /* handle udp or tcp socket */
		    if (ret->proto == TCP_PROTO)
			_tcp_cancel( ret, 1, unreach[ code ], 0 );
		    if (ret->proto == UDP_PROTO)
			_udp_cancel( ret );
		}
		break;

	case 4  : /* source quench */
		if (debug_on > 0 ) icmp_print( icmp, "Source Quench");
		if (ret->proto == TCP_PROTO)
		    _tcp_cancel( ret, 2, NULL, 0 );
		break;

	case 5  : /* redirect */
		if (code < 4) {
		    if (ret->proto == TCP_PROTO)
			/* do it to some socket guy */
			_tcp_cancel( ret, 5, NULL, icmp->ip.ipaddr );

		    if (debug_on > 0 ) icmp_print( icmp, redirect[ code ]);
		}
		break;

	case 8  : /* icmp echo request */
		/* icmp_print( icmp, "PING requested of us"); */
		if (debug_on > 0) icmp_print( icmp, "Echo Request"); // R. Whitby

		// don't reply if the request was made by ourselves
		// such as a problem with Etherslip pktdrvr
                if  ( (longword) (intel(ip->destination) - my_ip_addr) > multihomes )
		    return( 1 );

		// do arp and create packet
		/* format the packet with the request's hardware address */
		pkt = (struct _pkt*)(_eth_formatpacket( _eth_hardware((byte*)ip), 8));

		newicmp = &pkt->icmp;

		movmem( icmp, newicmp, len );
		newicmp->echo.type = 0;
		newicmp->echo.code = code;

		/* use supplied ip values in case we ever multi-home */
		/* note that ip values are still in network order */
		icmp_Reply( pkt,ip->destination, ip->source, len );

		/* icmp_print( newicmp, "PING reply sent"); */

		break;

	case 11 : /* time exceeded message */
		if (code < 2 ) {
		    icmp_print( icmp, exceed[ code ]);
		    if ((ret->proto == TCP_PROTO) && (code != 1))
			_tcp_cancel( ret, 1, NULL, 0 );
		}
		break;

	case 12 : /* parameter problem message */
		icmp_print( icmp, "IP Parameter Problem");
		break;

	// R. Whitby - added debug_on test to remaining cases
	case 13 : /* timestamp message */
		if (debug_on > 0) icmp_print( icmp, "Timestamp Request");
		/* send reply */
		break;

	case 14 : /* timestamp reply */
		if (debug_on > 0) icmp_print( icmp, "Timestamp Reply");
		/* should store */
		break;

	case 15 : /* info request */
		if (debug_on > 0) icmp_print( icmp,"Info Request");
		/* send reply */
		break;

	case 16 : /* info reply */
		if (debug_on > 0) icmp_print( icmp,"Info Reply");
		break;

	// R. Whitby
	default : /* unknown */
		if (debug_on > 0) icmp_print( icmp,"Unknown Type");
		break;

    }
    return( 1 );
}
