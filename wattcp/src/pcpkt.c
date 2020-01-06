
// FRAGSUPPORT enables support for packet reassembly of fragmented packets
#define FRAGSUPPORT

#include <copyright.h>
#include <wattcp.h>
#include <elib.h>
#include <dos.h>
#ifdef __BORLANDC__
#include <mem.h>
#endif
#include <string.h>
#include <stdlib.h>


#ifdef LARGE
#define MAXBUFS        25       /* maximum number of Ethernet buffers */
#else
#define MAXBUFS        10
#endif
#define BUFSIZE     1600

#define DOS         0x21
#define GETVECT     0x35

#define INT_FIRST 0x60
#define INT_LAST  0x80
#define PD_DRIVER_INFO	0x1ff
#define PD_ACCESS 	0x200
#define PD_RELEASE	0x300
#define PD_SEND		0x400
#define PD_GET_ADDRESS	0x600
#define  CARRY 		1	     /* carry bit in flags register */

word _pktipofs = 0;			/* offset from header to start of pkt */
word pkt_interrupt;
word pkt_ip_type = 0x0008;		/* these are intelled values */
word pkt_arp_type = 0x608;
#ifdef LARGE
byte far pktbuf[MAXBUFS][ BUFSIZE + 2 ];    /* first byte is busy flag, 2nd spare */
#else
byte pktbuf[ MAXBUFS][ BUFSIZE + 2];
#endif
word  pkt_ip_handle;
word  pkt_arp_handle;
//byte eth_addr[ 6 ] ;     // 94.11.19
eth_address eth_addr;
longword far *interrupts = 0L;
char *pkt_line = "PKT DRVR";

// fragfix -- just a note this is intel
#define IP_DF       0x0040      // Don't fragment bit set for FRAG Flags

// forwards and externs
int pkt_init( void );
extern void _pktentry();                          /* see asmpkt.asm */
extern void _pktasminit( void far *, int, int );  /* see asmpkt.asm */

static int pkt_init( void )       /* 94.11.27 -- made static */
{
    struct REGPACK regs, regs2;
    char far *temp;
    int pd_type;	/* packet driver type */
    int class;

    _pktasminit( pktbuf, MAXBUFS, BUFSIZE );
    for (pkt_interrupt = INT_FIRST; pkt_interrupt <= INT_LAST; ++pkt_interrupt ) {

        temp = (char far *)getvect( pkt_interrupt );
        if ( ! _fmemcmp( &(temp[3]), pkt_line, strlen( pkt_line )))
            break;
    }
    if ( pkt_interrupt > INT_LAST ) {
        outs("NO PACKET DRIVER FOUND\r\n");
	return( 1 );
    }

    /* lets find out about the driver */
    regs.r_ax = PD_DRIVER_INFO;
    intr( pkt_interrupt, &regs );

    /* handle old versions, assume a class and just keep trying */
    if (regs.r_flags & CARRY ) {
	for ( class = 0; class < 2; ++class ) {
	    _pktdevclass = (class) ? PD_SLIP : PD_ETHER;

	    for (pd_type = 1; pd_type < 128; ++pd_type ) {
		regs.r_ax = PD_ACCESS | _pktdevclass;  /* ETH, SLIP */
		regs.r_bx = pd_type;		/* type */
		regs.r_dx = 0;			/* if number */
                regs.r_cx = (_pktdevclass == PD_SLIP ) ? 0 : sizeof( pkt_ip_type);
		regs.r_ds = FP_SEG( &pkt_ip_type );
		regs.r_si = FP_OFF( &pkt_ip_type );
                regs.r_es = FP_SEG( _pktentry);
		regs.r_di = FP_OFF( _pktentry);
		intr( pkt_interrupt, &regs );
		if ( ! (regs.r_flags & CARRY) ) break;
	    }

	    if (pd_type == 128 ) {
		outs("ERROR initializing packet driver\n\r");
		return( 1 );
	    }
	    /* we have found a working type, so kill it */
	    regs.r_bx = regs.r_ax;	/* handle */
	    regs.r_ax = PD_RELEASE;
	    intr( pkt_interrupt, &regs );
	}
    } else {
	pd_type = regs.r_dx;
	switch ( _pktdevclass = (regs.r_cx >> 8)) {
	    case PD_ETHER : _pktipofs = 14;

            case PD_SLIP  : break;
            default       : outs("ERROR: only Ethernet or SLIP packet drivers allowed\n\r");
			    return( 1 );
	}
    }
    regs.r_ax = PD_ACCESS | _pktdevclass;
    regs.r_bx = 0xffff;                 /* any type */
    regs.r_dx = 0;			/* if number */
    regs.r_cx = (_pktdevclass == PD_SLIP) ? 0 : sizeof( pkt_ip_type );
    regs.r_ds = FP_SEG( &pkt_ip_type );
    regs.r_si = FP_OFF( &pkt_ip_type );
    regs.r_es = FP_SEG( _pktentry);
    regs.r_di = FP_OFF( _pktentry);
    memcpy( &regs2, &regs, sizeof( regs ));
    regs2.r_si = FP_OFF( &pkt_arp_type );
    regs2.r_ds = FP_SEG( &pkt_arp_type );

    intr( pkt_interrupt, &regs );
    if ( regs.r_flags & CARRY ) {
	outs("ERROR # 0x");
	outhex( regs.r_dx >> 8 );
	outs(" accessing packet driver\n\r" );
	return( 1 );
    }
    pkt_ip_handle = regs.r_ax;

    if (_pktdevclass != PD_SLIP) {
	intr( pkt_interrupt, &regs2 );
	if ( regs2.r_flags & CARRY ) {
	    regs.r_ax = PD_RELEASE;
	    regs.r_bx = pkt_ip_handle;
	    intr( pkt_interrupt, &regs );

	    outs("ERROR # 0x");
	    outhex( regs2.r_dx >> 8 );
	    outs(" accessing packet driver\n\r" );
	    return( 1 );
	}
	pkt_arp_handle = regs2.r_ax;
    }

    /* get ethernet address */
    regs.r_ax = PD_GET_ADDRESS;
    regs.r_bx = pkt_ip_handle;
    regs.r_es = FP_SEG( &eth_addr );
    regs.r_di = FP_OFF( &eth_addr );
    regs.r_cx = sizeof( eth_addr );
    intr( pkt_interrupt, &regs );
    if ( regs.r_flags & CARRY ) {
	outs("ERROR # reading ethernet address\n\r" );
	return( 1 );
    }

    return( 0 );
}

void pkt_release( void )
{
    struct REGPACK regs;
//    int error;

    if ( _pktdevclass != PD_SLIP ) {
	regs.r_ax = PD_RELEASE;
	regs.r_bx = pkt_arp_handle;
	intr( pkt_interrupt, &regs );
	if (regs.r_flags & CARRY ) {
	    outs("ERROR releasing packet driver for ARP\n\r");
	}
    }

    regs.r_ax = PD_RELEASE;
    regs.r_bx = pkt_ip_handle;
    intr( pkt_interrupt, &regs );
    if (regs.r_flags & CARRY )
	outs("ERROR releasing packet driver for IP\n\r");

    return;
}

int pkt_send( char *buffer, int length )
{
    struct REGPACK regs;
    int retries;

    retries = 5;
    while (retries--) {
        regs.r_ax = PD_SEND;
        regs.r_ds = FP_SEG( buffer );
        regs.r_si = FP_OFF( buffer );
        regs.r_cx = length;
        intr( pkt_interrupt, &regs );
        if ( regs.r_flags & CARRY )
            continue;
        return( 0 );
    }
    return( 1 );
}

/* return a buffer to the pool */
void pkt_buf_wipe( void )
{
    memset( pktbuf, 0, sizeof( byte ) * MAXBUFS * (BUFSIZE+ 2));
}

void pkt_buf_release( char *ptr )
{
    *(ptr - (2 + _pktipofs)) = 0;
}

void * pkt_received( void )
{
    word old;
    int i;
    word oldin, newin;	/* ip sequence numbers */
    eth_Header  * temp_e = NULL;
    in_Header   * temp;
    byte        * t_buf;
    extern int active_frags;

    /* check if there are any */
    old = oldin = 0xffff;

    // Do frag timeout bit sad if we got the bit of one we're about to kill
#ifdef FRAGSUPPORT
    if ( active_frags ) timeout_frags();
#endif // FRAGSUPPORT
    for ( i = 0 ; i < MAXBUFS; ++i ) {
	if ( *pktbuf[i] != 1 ) continue;

        // check if fragmented - SLIP supported
	temp = (in_Header *) &pktbuf[ i ][ 2 ];
        if ( _pktdevclass == PD_ETHER ) {
            temp_e = (eth_Header *) temp;
// fragfix -- next line did pointer arith so incorrectly added
//               ... * sizeof(typeof(*temp)) instead of ... * 1
//            temp += sizeof( eth_Header );
            temp = (in_Header *)((byte*)temp + sizeof(eth_Header));
        }

#ifdef FRAGSUPPORT
        if ((( _pktdevclass == PD_SLIP ) || ( temp_e->type == IP_TYPE ))
// fragfix -- next line, need ~ to clear DF bit, not all others
//              ... check is either MF set or frag offset not 0
                && ( temp->frags & ~IP_DF )) {
//            && ( temp->frags & IP_DF )) {

            if ( ( t_buf = fragment( temp )) == NULL )
                // pass pointer to ip section of buffer
                continue;
            else
                return( t_buf );
        }
#endif // FRAGSUPPORT
        newin = *(word *)( &pktbuf[i][ _pktipofs + 4 + 2 ]);
        if ( newin <= oldin ) {
            oldin = newin;
            old = i;
        }
    }

    return( (old == 0xffff) ? NULL : &pktbuf[old][2] );
}

eth_address *_pkt_eth_init( void )
{
    if ( pkt_init() ) {
       return NULL;		// S. Lawson
// S. Lawson        outs("Program halted\r\n");
// S. Lawson	exit( 1 );
    }
    return( &eth_addr );
}
