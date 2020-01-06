/*
 *
 *   BOOTP - Boot Protocol (RFC 854)
 *
 *   These extensions get called if _bootphost is set to an IP address or
 *   to 0xffffffff.
 *
 *   Version
 *
 *   0.4 : May 11, 1999 : S. Lawson - added DHCP
 *   0.3 : Feb  1, 1992 : J. Dent - patched up various things
 *   0.2 : May 22, 1991 : E.J. Sutcliffe - added RFC_1048 vendor fields
 *   0.1 : May  9, 1991 : E. Engelke - made part of the library
 *   0.0 : May  3, 1991 : E. Engelke - original program as an application
 *
 */

#include <copyright.h>
#include <stdio.h>
#include <wattcp.h>
#include <mem.h>
#include <bootp.h>
#include <conio.h>
#include <string.h>		// S. Lawson

/* global variables */
longword _bootphost = 0xffffffffL;
longword _dhcphost = 0xffffffffL;       // S. Lawson
word _bootptimeout = 30;
word _bootpon = 0;

static longword dhcp_life;	        // S. Lawson
static char dhcp_seen, dhcp_bind;	// S. Lawson
static char got_ns, got_gw, got_cy;	// S. Lawson
static udp_Socket bsock;		// S. Lawson

// S. Lawson - we now use these
char *getdomainname( char *name, int length );
char *setdomainname( char *string );
char *sethostname( char *name );

#define VM_RFC1048 0x63825363L		/* I think this is correct */

/*
 * _dobootpc - Checks global variables _bootptimeout, _bootphost
 *             if no host specified, the broadcast address
 *             returns 0 on success and sets ip address
 */
int _dobootp( void )
{
// S. Lawson - moved    udp_Socket bsock;
    extern char defaultdomain[];	// S. Lawson - in pcconfig.c
    longword sendtimeout, bootptimeout;
    word magictimeout;
    word len, templen;
    struct bootp sendbootp;     /* outgoing data */
    struct bootp _bootp;        /* incoming data */
    int status;
    longword xid;
    unsigned char *p ,*t;       // R. Whitby - uncomment *t
    longword *l;       		// S. Lawson
/*
    const char dhcp_opt[]={DHCP_VN_TYPE,1,DHCP_TY_REQ,DHCP_VN_OPTS,6,1,
                           3,6,42,12,15,0}; */  /* S. Lawson - keep even! */
    const char dhcp_opt[]={DHCP_VN_OPTS,6,1,3,6,42,12,15,0}; /* Shaun Jackman 2000/3/10 */
//    if ( _pktdevclass == PD_SLIP ) return( -1 );


    /* We must get Waterloo TCP to use IP address 0 for sending */
    xid = set_timeout(1);   /* random... well, actually time based */
    my_ip_addr = 0;
    dhcp_seen=dhcp_bind=0;      		// S. Lawson
    got_ns=got_gw=got_cy=0;			// S. Lawson

    if (!udp_open( &bsock, IPPORT_BOOTPC, _bootphost, IPPORT_BOOTPS, NULL )) {
	outs("\n\rUnable to resolve bootp server\n\r");
	return( -1 );
    }

    bootptimeout = set_timeout( _bootptimeout );
    magictimeout = (xid & 7) + 7;  /* between 7 and 14 seconds */

    memset( &sendbootp, 0, sizeof( struct bootp ));
    sendbootp.bp_op = BOOTREQUEST;
    sendbootp.bp_htype = _pktdevclass;
    /* Copy into position the Magic Number used by Bootp */
    /* avoid static storage and pushf/call assembler instructions */
    *(longword *)(&sendbootp.bp_vend) = intel(VM_RFC1048);

    // S. Lawson - append DHCPDISCOVER to vendor field.  I'm expecting that
    // a BOOTP server configured with this machines MAC address will simply
    // ignore it and answer anyway, possibly a bad assumption?
    p=&sendbootp.bp_vend[4];
    *p++=DHCP_VN_TYPE;
    *p++=1;
    *p++=DHCP_TY_DSC;
    *p=255;

    if (_pktdevclass == PD_ETHER) sendbootp.bp_hlen = 6;

    sendbootp.bp_xid = xid;
    sendbootp.bp_secs = intel16( 1 );

//    movmem( &_eth_addr, &sendbootp.bp_chaddr, sizeof(eth_address));
// 94.11.19 ??
    movmem( &_eth_addr, sendbootp.bp_chaddr, sizeof(eth_address));

    while ( 1 ) {
	sock_fastwrite( (sock_type*)&bsock, (byte *)&sendbootp, sizeof( struct bootp ));
	sendbootp.bp_secs = intel16( intel16( sendbootp.bp_secs ) + magictimeout );      /* for next time */
	sendtimeout = set_timeout( magictimeout += (xid >> 5) & 7 );

	while ( !chk_timeout( sendtimeout )) {

	    if (chk_timeout( bootptimeout))
		goto give_up;
	    kbhit();
	    sock_tick( (sock_type*)&bsock, &status );
	    status = status;    /* get rid of warning */
	    if ((len = sock_dataready( (sock_type*)&bsock)) != 0 ) {

		/* got a response, lets consider it */
		templen = sock_fastread( (sock_type*)&bsock, (byte *)&_bootp, sizeof( struct bootp ));
// S. Lawson - vendor area increased (bootp: 64, dhcp: 312)
//		if ( templen < sizeof( struct bootp )) {
		if ( templen < sizeof( struct bootp ) - (312-64) ) {
		    /* too small, not a bootp packet */
		    memset( &_bootp, 0, sizeof( struct bootp ));
		    continue;
		}

		/* we must see if this is for us */
		if (_bootp.bp_xid != sendbootp.bp_xid) {
		    memset( &_bootp, 0, sizeof( struct bootp ));
		    continue;
		}

		/* we must have found it */
		my_ip_addr = intel( _bootp.bp_yiaddr );

		// S. Lawson - just in case
		if (!my_ip_addr) {
		  outs("Rejecting zero address\r\n");
		  memset( &_bootp, 0, sizeof( struct bootp ));
		  continue;
		}

		dhcp_seen = 0;          // S. Lawson
		dhcp_life=0L;		// S. Lawson

		if ( intel( *(longword*)(&_bootp.bp_vend)) == VM_RFC1048 ) {
		    /*RFC1048 complient BOOTP vendor field */
		    /* Based heavily on NCSA Telnet BOOTP */

		    p = &_bootp.bp_vend[4]; /* Point just after vendor field */

// S. Lawson - vendor area increased for dhcp (bootp: 64, dhcp: 312)
//		    while ((*p!=255) && (p <= &_bootp.bp_vend[63])) {
		    while ((*p!=255) && (p <= &_bootp.bp_vend[311])) {
			switch(*p) {
			  case 0: /* Nop Pad character */
				 p++;
				 break;
			  case 1: /* Subnet Mask */
				 sin_mask = intel( *(longword *)( &p[2] ));
				 /* and fall through */
			  case 2: /* Time offset */
				 p += *(p+1) + 2;
				 break;
			  case 3: /* gateways */
				  /* only add first */
				  if (!got_gw)  	// S. Lawson
				     _arp_add_gateway( NULL,
					intel( *(longword*)(&p[2])));
				  p +=*(p+1)+2;
				  got_gw=1;		// S. Lawson
				  break;
				  /* and fall through */
			  case 4: /*time servers */
				  /* fall through */
			  case 5: /* IEN=116 name server */
				 p +=*(p+1)+2;
				 break;
			  case 6: /* Domain Name Servers (BIND) */
				if (!got_ns) 	// S. Lawson
				  for ( len = 0; len < *(p+1) ; len += 4 )
				    _add_server( &_last_nameserver,
					MAX_NAMESERVERS, def_nameservers,
					    intel( *(longword*)(&p[2+len])));
				got_ns=1;	// S. Lawson
				/* and fall through */
			  case 7: /* log server */
				 p += *(p+1)+2;
				 break;
			  case 8: /* cookie server */
				 if (!got_cy) 	// S. Lawson
				   for ( len = 0; len < *(p+1) ; len += 4 )
				     _add_server( &_last_cookie, MAX_COOKIES,
					_cookie, intel( *(longword*)(&p[2+len])));
				  /* and fall through */
				  p +=*(p+1)+2;
				  got_cy=1;	// S. Lawson
				  break;
			  case 9: /* lpr server */
			  case 10: /* impress server */
			  case 11: /* rlp server */
				   p +=*(p+1)+2;
				   break;

#ifdef NOTUSED	// R. Whitby - handle bootp/dhcp setting of domain name too
			  case 12: /* Client Hostname */
				  movmem( &p[2] , _hostname, MAX_STRING );
				/* bootpfix - 94.09.30 mdurkin@tsoft.net */
		       /* hostname field is *not* null terminated; need to use
			    length byte.  Extra bytes used to be tacked on the
			    end, usually just a single 0xFF char (the end tag).
			    Technically should also probably make the
			    _hostname buffer 256 chars long so we'll never
			    truncate the name arbitrarily... see CMU bootpd. */
				  if( *(p+1) < MAX_STRING ) {
				      _hostname[ *(p+1) ] = '\0';
				  } else _hostname[ MAX_STRING - 1 ] = '\0';
				  p += *(p+1)+2;
				  break;
#else
			  case 12: /* Client Hostname */
				  len = *(++p);
				  movmem(p+1, p, len); *(p+len) = 0;
				  if ((t = strchr((char *)p, '.')) != 0) {
				     *t++ = 0;
				     if (!getdomainname(NULL, 0)) {
					strncpy(defaultdomain, (char *)t, MAX_STRING);
					defaultdomain[MAX_STRING-1] = 0;
					setdomainname(defaultdomain);
				     }
				  }
				  strncpy(_hostname, (char *)p, MAX_STRING);
				  _hostname[MAX_STRING-1] = 0;
				  sethostname(_hostname);
				  p += len+1;
				  break;

			  case 15: /* Client Domain */
				  len = *(++p);
				  if (!getdomainname(NULL, 0)) {
				     movmem(p+1, p, len); *(p+len) = 0;
				     strncpy(defaultdomain, (char *)p, MAX_STRING);
				     defaultdomain[MAX_STRING-1] = 0;
				     setdomainname(defaultdomain);
				  }
				  p += len+1;
				  break;
#endif

			  // S. Lawson - handle DHCP type
			  case DHCP_VN_TYPE:
				  dhcp_seen=1;
				  if (!dhcp_bind) {
				     if (*(p+2)!=DHCP_TY_OFR) my_ip_addr=0L;
				  } else {
				     if (*(p+2)!=DHCP_TY_ACK) my_ip_addr=0L;
				  }
				  p += *(p+1)+2;
				  break;

			  // S. Lawson - handle server id
			  case DHCP_VN_SRVRID:
				  _dhcphost=intel(*(longword*)(&p[2]));
				  p += *(p+1)+2;
				  break;

			  // S. Lawson - handle lease expiration time
			  case DHCP_VN_T2TIME:
				  dhcp_life=intel(*(longword*)(&p[2]));
				  p += *(p+1)+2;
				  break;

			  case 255:
				   break;
			  default:
				   p +=*(p+1)+2;
				   break;
			} /* end of switch */
		     } /* end of while */
		}/* end of RFC_1048 if */
		if (dhcp_seen && my_ip_addr==0L) continue;  // S. Lawson
		if (!dhcp_seen || dhcp_bind)                // S. Lawson
		   goto give_up;

		// S. Lawson - append DHCPREQUEST and server/offered IP
		// addresses to vendor field.  We should get get a DHCPACK
		// in response
		dhcp_bind=1;
		p=&sendbootp.bp_vend[4];
		*p++=DHCP_VN_TYPE;
		*p++=1;
		*p++=DHCP_TY_REQ;
		movmem(dhcp_opt,p,sizeof(dhcp_opt));
		p+=sizeof(dhcp_opt);
		*p++=DHCP_VN_REQIP;
		*p++=4;
		l=(longword *) p, *l++=intel(my_ip_addr), p=(char *) l;
		*p++=DHCP_VN_SRVRID;
		*p++=4;
		l=(longword *) p, *l++=intel(_dhcphost), p=(char *) l;
		*p=255;
		my_ip_addr=0L;
		break;

	    }
	}
    }
give_up:

    sock_close( (sock_type *)&bsock );

    return (my_ip_addr == 0 );  /* return 0 on success */

sock_err:
    /* major network error if UDP fails */
    sock_close( (sock_type *)&bsock );
    return( -1 );
}

// S. Lawson - countdown the DHCP lease, renew or expire it as needed (mostly
//             copied from above and trimmed down alot)
int dhcp_expired(void) {
   static char nested=false;
   static char expired=false;
   static char sockopen=false;
   static longword timeout;
   static longword xid=0;
   static longword seconds=0, renew=0;
   longword ltime;
   word len;
   char acknak;				// -1 NAK, 0 unseen, 1 ACK
   struct bootp _bootp;
   unsigned char *p;
   longword *l;

   // once expired, stop talking
   if (expired) return true;

   // skip if nested, not dhcp, or the lease is permanent
   if (nested || !dhcp_bind || dhcp_life==0xFFFFFFFF) return false;
   nested=true;					// avoid tcp_tick() recursion

   // on first pass, init the renewal time and start the timer
   if (seconds==0) {
      renew=dhcp_life>>1;			// renew at 1/2 lifetime
      if ((seconds=renew)>3600) seconds=3600;
      timeout=set_timeout((int) seconds);
      nested=false;
      return false;
   }

   // if the bootp socket is open, check for a response
   if (sockopen && (len = sock_dataready( (sock_type*)&bsock)) != 0 ) {
     memset( &_bootp, 0, sizeof( struct bootp ));
     len = sock_fastread( (sock_type*)&bsock, (byte *)&_bootp, sizeof( struct bootp ));
     // (312-64) gives size of original bootp packet - struct bigger for dhcp
     if ( len >= sizeof( struct bootp ) - (312-64) &&
	  _bootp.bp_xid == xid &&
	  my_ip_addr == intel( _bootp.bp_yiaddr ) &&
	  intel( *(longword*)(&_bootp.bp_vend)) == VM_RFC1048 ) {
       acknak=0;
       ltime=0;
       p=&_bootp.bp_vend[4];
       while (*p!=255 && p<=&_bootp.bp_vend[311]) {
	 switch(*p) {
	   case 0: 			/* Nop Pad character */
	     p++;
	     break;

	   case DHCP_VN_TYPE:  		/* DHCP type */
	     if (*(p+2)==DHCP_TY_ACK) acknak=1;
	     if (*(p+2)==DHCP_TY_NAK) acknak=-1;
	     p += *(p+1)+2;
	     break;

	   case DHCP_VN_T2TIME:		/* expiration time */
	     ltime=intel(*(longword*)(&p[2]));
	     p += *(p+1)+2;
	     break;

	   default:
	     p +=*(p+1)+2;
	     break;
	 } /* end of switch */
       } /* end of while */
       if (acknak==-1) {			// NAK!
	 dhcp_life=renew=seconds=0;		//   fall through and expire
       } else if (acknak==1 && ltime) {		// ACK!
	 sock_close((sock_type*)&bsock);
	 sockopen=false;
	 dhcp_life=ltime;
	 renew=dhcp_life>>1;			//   renew at 1/2 lifetime
	 if ((seconds=renew)>3600) seconds=3600;
	 timeout=set_timeout(seconds);
	 nested=false;
	 return false;
       }
     }
   }

   // count down the timeout till it expires
   if (!chk_timeout(timeout)) {
      nested=false;
      return false;
   }

   // keep counting every hour (or less) till renewal time
   renew-=seconds;
   dhcp_life-=seconds;
   if (renew>0) {
      if ((seconds=renew)>3600) seconds=3600;
      timeout=set_timeout((int) seconds);
      nested=false;
      return false;
   }

   // expire if dhcp_life gets below 2 seconds
   if (dhcp_life<2) {
      outs("\n\rDHCP lease expired, network disabled\n\r");
      if (sockopen) {
	 sock_close((sock_type*)&bsock);
	 sockopen=false;
      }
      // we really should should terminate all connections here
      expired=true;
      nested=false;
      return true;
   }

   // send out a DHCPREQUEST to renew the lease and restart timer
   if (!sockopen && !udp_open( &bsock, IPPORT_BOOTPC, _dhcphost, IPPORT_BOOTPS, NULL )) {
     outs("\n\rUnable to resolve dhcp server\n\r");
   } else {
     sockopen=true;
     memset( &_bootp, 0, sizeof( struct bootp ));
     _bootp.bp_op = BOOTREQUEST;
     _bootp.bp_htype = _pktdevclass;
     if (_pktdevclass == PD_ETHER) _bootp.bp_hlen = 6;
     _bootp.bp_ciaddr=intel(my_ip_addr);
     if (xid==0) xid=intel(my_ip_addr);
     _bootp.bp_xid = ++xid;
     _bootp.bp_secs = intel16( 1 );
     *(longword *)(&_bootp.bp_vend) = intel(VM_RFC1048);
     p=&_bootp.bp_vend[4];
     *p++=DHCP_VN_TYPE;
     *p++=1;
     *p++=DHCP_TY_REQ;
     *p++=0;
     *p++=DHCP_VN_REQIP;
     *p++=4;
     l=(longword *) p, *l++=intel(my_ip_addr), p=(char *) l;
     *p++=DHCP_VN_SRVRID;
     *p++=4;
     l=(longword *) p, *l++=intel(_dhcphost), p=(char *) l;
     *p=255;
     movmem( &_eth_addr, _bootp.bp_chaddr, sizeof(eth_address));
     sock_fastwrite( (sock_type*)&bsock, (byte *)&_bootp, sizeof( struct bootp ));
   }
   renew=dhcp_life>>1;				// renew at 1/2 lifetime
   if ((seconds=renew)>3600) seconds=3600;
   timeout=set_timeout(seconds);
   nested=false;
   return false;
}
