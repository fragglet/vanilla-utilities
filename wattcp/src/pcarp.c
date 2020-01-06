/*
 * Address Resolution Protocol
 *
 *  Externals:
 *  _arp_handler( pb ) - returns 1 on handled correctly, 0 on problems
 *  _arp_resolve - rets 1 on success, 0 on fail
 *               - does not return hardware address if passed NULL for buffer
 *
 */
#include <copyright.h>
#include <wattcp.h>
#include <string.h>
#include <mem.h>

#define MAX_ARP_DATA 40
#define MAX_ARP_ALIVE  300 /* five minutes */
#define MAX_ARP_GRACE  100 /* additional grace upon expiration */

extern word wathndlcbrk;
extern word watcbroke;
extern word multihomes;

typedef struct {
    longword		ip;
    eth_address		hardware;
    byte		flags;
    byte		bits;		/* bits in network */
    longword		expiry;
} arp_tables;

typedef struct {
    longword		gate_ip;
    longword		subnet;
    longword		mask;
} gate_tables;

#define ARP_FLAG_NEED	0
#define ARP_FLAG_FOUND  1
#define ARP_FLAG_FIXED  255	/* cannot be removed */


/*
 * arp resolution cache - we zero fill it to save an initialization routine
 */
static arp_tables arp_data[ MAX_ARP_DATA ] =
 { {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL},
   {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL},
   {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL},
   {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL},
   {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL},
   {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL},
   {0uL,{{0,0,0,0,0,0}},0,0,0uL}, {0uL,{{0,0,0,0,0,0}},0,0,0uL}};

gate_tables _arp_gate_data[ MAX_GATE_DATA ];
word _arp_last_gateway;

extern void (*system_yield)();      /* from pctcp.c 2000.4.14 EE */



/*
 * _arp_add_gateway - if data is NULL, don't use string
 */
void _arp_add_gateway( char *data, longword ip )
{
    int i;
    char *subnetp, *maskp;
    longword subnet, mask;

    subnet = mask = 0;
    if ( data ) {
	maskp = NULL;
	if ( (subnetp = strchr( data, ',' )) != NULL ) {
	    *subnetp++ = 0;
	    if ( (maskp = strchr( subnetp, ',' )) != NULL ) {
		*maskp++ = 0;
		mask = aton( maskp );
		subnet = aton( subnetp );
	    } else {
		subnet = aton( subnetp );
		switch ( subnet >> 30 ) {
		    case 0 :
		    case 1 : mask = 0xff000000L; break;
		    case 2 : mask = 0xfffffe00L; break;	/* minimal class b */
		    case 3 : mask = 0xffffff00L; break;
		}
	    }
	}
        ip = aton( data );
    }

    if ( _arp_last_gateway < MAX_GATE_DATA ) {
        for ( i = 0 ; i < _arp_last_gateway ; ++i ) {
            if ( _arp_gate_data[i].mask < mask ) {
                movmem( &_arp_gate_data[i], &_arp_gate_data[i+1],
                    (_arp_last_gateway - i) * sizeof( gate_tables ));
                break;
            }
        }
        _arp_gate_data[i].gate_ip = ip;
        _arp_gate_data[i].subnet = subnet;
        _arp_gate_data[i].mask = mask;
        ++_arp_last_gateway;    /* used up another one */
    }
}

static void _arp_request( longword ip )
{
    arp_Header *op;

    op = (arp_Header *)_eth_formatpacket(&_eth_brdcast, 0x608);
    op->hwType = arp_TypeEther;
    op->protType = 0x008;		/* IP */
    op->hwProtAddrLen = sizeof(eth_address) + (sizeof(longword)<<8);
    op->opcode = ARP_REQUEST;
    op->srcIPAddr = intel( my_ip_addr );
    movmem(&_eth_addr, &op->srcEthAddr, sizeof(eth_address));
    op->dstIPAddr = intel( ip );

    /* ...and send the packet */
    _eth_send( sizeof(arp_Header) );
}

static word arp_index = 0;		/* rotates round-robin */

static arp_tables *_arp_search( longword ip, int create )
{
    int i;
    arp_tables *arp_ptr;

    for ( i = 0; i < MAX_ARP_DATA; ++i ) {
	if ( ip == arp_data[i].ip )
	    return( &arp_data[i] );
    }

    /* didn't find any */
    if ( create ) {
	/* pick an old or empty one */
	for ( i = 0; i < MAX_ARP_DATA ; ++i ) {
	    arp_ptr = &arp_data[i];
	    if ( ! arp_ptr->ip || chk_timeout(arp_ptr->expiry+MAX_ARP_GRACE))
		return( arp_ptr );
	}

	/* pick one at pseudo-random */
	return( &arp_data[ arp_index = ( arp_index + 1 ) % MAX_ARP_DATA ] );
    }
    return( NULL );
}

void _arp_register( longword use, longword instead_of )
{
/*    word i; */
    arp_tables *arp_ptr;

    if ( (arp_ptr = _arp_search( instead_of, 0 )) != NULL) {
	/* now insert the address of the new guy */
	arp_ptr->flags = ARP_FLAG_NEED;
        _arp_resolve( use, &arp_ptr->hardware, 0);
	arp_ptr->expiry = set_timeout( MAX_ARP_ALIVE );
	return;
    }

    arp_ptr = _arp_search( use , 1 );	/* create a new one */
    arp_ptr->flags = ARP_FLAG_NEED;
/* but now is this right?  if 'use' was already in the arp cache,
     we're now nuking it... at worst an efficiency problem */
    arp_ptr->ip = instead_of;  /* use; */               /* 94.11.30 */
    _arp_resolve( use, &arp_ptr->hardware, 0);
    arp_ptr->expiry = set_timeout( MAX_ARP_ALIVE );
}

void _arp_tick( longword ip )
{
    arp_tables *arp_ptr;

    if ( (arp_ptr = _arp_search( ip , 0)) != NULL )
	arp_ptr->expiry = set_timeout( MAX_ARP_ALIVE );
}

/*
 * _arp_handler - handle incomming ARP packets
 */
int _arp_handler( arp_Header *in)
{
    arp_Header *op;
    longword his_ip;
    arp_tables *arp_ptr;

    if ( in->hwType != arp_TypeEther ||      /* have ethernet hardware, */
	in->protType != 8 )     	     /* and internet software, */
	return( 0 );

    /* continuously accept data - but only for people we talk to */
    his_ip = intel( in->srcIPAddr );

    if ( (arp_ptr = _arp_search( his_ip, 0)) != NULL ) {
	arp_ptr->expiry = set_timeout( MAX_ARP_ALIVE );
	movmem( &in->srcEthAddr, &arp_ptr->hardware, sizeof( eth_address ));
	arp_ptr->flags = ARP_FLAG_FOUND;
    }

    /* does someone else want our Ethernet address ? */
    if ( in->opcode == ARP_REQUEST &&        /* and be a resolution req. */
	 ((longword)(intel(in->dstIPAddr) - my_ip_addr ) <= multihomes )
       )  {
	op = (arp_Header *)_eth_formatpacket(&in->srcEthAddr, 0x0608);
	op->hwType = arp_TypeEther;
	op->protType = 0x008;			/* intel for ip */
	op->hwProtAddrLen = sizeof(eth_address) + (sizeof(longword) << 8 );
	op->opcode = ARP_REPLY;

	op->dstIPAddr = in->srcIPAddr;
	op->srcIPAddr = in->dstIPAddr;
	movmem(&_eth_addr, &op->srcEthAddr, sizeof(eth_address));
	movmem(&in->srcEthAddr, &op->dstEthAddr, sizeof(eth_address));
	_eth_send(sizeof(arp_Header));
	return ( 1 );
    }
    return( 1 );
}


/*
 * _arp_resolve - resolve IP address to hardware address
 */
int _arp_resolve( longword ina, eth_address *ethap, int nowait )
{
    static arp_tables *arp_ptr;
    int i, oldhndlcbrk;
    longword timeout, resend;
/*    int packettype; */

    if ( _pktdevclass == PD_SLIP ) {
	/* we are running slip or somthing which does not use addresses */
	return( 1 );
    }

    if ( (longword)(ina - my_ip_addr) < multihomes) {
	if (ethap)
	    movmem( &_eth_addr, ethap, sizeof( eth_address ));
	return( 1 );
    }

    /* attempt to solve with ARP cache */
    /* fake while loop */
    while ( (arp_ptr = _arp_search( ina, 0)) != NULL ) {
	if ( arp_ptr->flags != ARP_FLAG_NEED ) {
	    /* has been resolved */
#ifdef NEW_EXPIRY
	    if ( chk_timeout( arp_ptr->timeout ) {
		if ( ! chk_timeout( arp_ptr->timeout + MAX_ARP_GRACE ) {
		    /* we wish to refresh it asynchronously */
		    _arp_request( ina );
		else
		    break;	/* must do full fledged arp */
#endif NEW_EXPIRY
	    if (ethap)
		movmem( &arp_ptr->hardware, ethap, sizeof(eth_address));
	    return( 1 );
	}
        break;
    }

    /* make a new one if necessary */
    if (! arp_ptr )
	arp_ptr = _arp_search( ina, 1 );

    /* we must look elsewhere - but is it on our subnet??? */
    if (( ina ^ my_ip_addr ) & sin_mask ) {

	/* not of this network */
	for ( i = 0; i < _arp_last_gateway ; ++i ) {

            //  is the gateway on our subnet
            //  or if mask is ff...ff, we assume any gateway must succeed
            //  because we are on 'no' network

            if ((((_arp_gate_data[i].gate_ip ^ my_ip_addr ) & sin_mask ) == 0)
             || ( sin_mask == 0xffffffffL )) {
                /* compare the various subnet bits */
                if ( (_arp_gate_data[i].mask & ina ) == _arp_gate_data[i].subnet ) {
                    if ( _arp_resolve( _arp_gate_data[i].gate_ip , ethap, nowait ))
                        return( 1 );
                }
	    }
	}
        return( 0 );
    }

    /* return if no host, or no gateway */
    if (! ina )
	return( 0 );

    /* is on our subnet, we must resolve */
    timeout = set_timeout( 5 );		/* five seconds is long for ARP */
    oldhndlcbrk = wathndlcbrk;
    wathndlcbrk = 1;
    watcbroke = 0;
    while ( !chk_timeout( timeout )) {
	/* do the request */
	_arp_request( arp_ptr->ip = ina );
	resend = set_timeout( 1 ) - 14L;	/* 250 ms */
	while (!chk_timeout( resend )) {
            if (watcbroke) goto fail;
	    tcp_tick( NULL );
	    if ( arp_ptr->flags) {
		if (ethap)
		    movmem( &arp_ptr->hardware, ethap, sizeof(eth_address));
		arp_ptr->expiry = set_timeout( MAX_ARP_ALIVE );
                watcbroke = 0;
                wathndlcbrk = oldhndlcbrk;
		return ( 1 );
	    }
            /* EE 2000.4.14 */
            if ( system_yield != NULL )
                (*system_yield)();
	}
        if ( nowait ) goto fail;
    }
fail:
    watcbroke = 0;
    wathndlcbrk = oldhndlcbrk;
    return ( 0 );
}
