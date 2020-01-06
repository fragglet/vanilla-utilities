#include <mem.h>
#include <copyright.h>
#include <wattcp.h>

#include <icmp.h>

// R. Whitby
extern void (*_dbugxmit)( sock_type *s, in_Header *inp, void *phdr,
	     unsigned line );

int _send_ping( longword host, longword countnum, byte ttl, byte tos, longword *theid )
{
    eth_address dest;
    struct _pkt *p;
    in_Header *ip;
    struct icmp_echo *icmp;
    static word icmp_id = 0;

    if ((host & 0xff) == 0xff ) {
	outs( "\r\nPING: Cannot ping a network!\n\r");
	return( -1 );
    }
    if ( ! _arp_resolve( host, &dest, 0 )) {
	outs( "\r\nPING: Cannot resolve host's hardware address\n\r");
	return( -1 );
    }

    if (debug_on) {
	outs("\n\rPING: Destination Hardware :");
	outhexes( (char *)&dest, 6 );
	outs("\n\r");
    }

    p = (struct _pkt*)_eth_formatpacket( &dest, 8 );

    ip = &p->in;
    memset( ip, 0, sizeof( in_Header ));
    icmp = &(p->icmp.echo);

    icmp->type = 8;
    icmp->code = 0;
    icmp->index = countnum;
    *(longword *)(&icmp->identifier) = set_timeout( 1 );
    if( theid ) *theid = *(longword *)(&icmp->identifier);
/*
    icmp->identifier = ++icmp_id;
    icmp->sequence = icmp_id;
*/
    /* finish the icmp checksum portion */
    icmp->checksum = 0;
    icmp->checksum = ~checksum( icmp, sizeof(struct icmp_echo) );

    /* encapsulate into a nice ip packet */
    ip->ver = 4;
    ip->hdrlen = 5;
    ip->length = intel16( sizeof( in_Header ) + sizeof( struct icmp_echo));
    ip->tos = tos;
    ip->identification = intel16( icmp_id ++);	/* not using ip id */
//    ip->frag = 0;
    ip->ttl = ttl;
    ip->proto = ICMP_PROTO;
    ip->checksum = 0;
    ip->source = intel( my_ip_addr );
    ip->destination = intel( host );
    ip->checksum = ~ checksum( ip, sizeof( in_Header ));

    if (_dbugxmit) (*_dbugxmit)( NULL, ip, icmp, 0);	// R. Whitby

    return( _eth_send( intel16( ip->length )));
}

