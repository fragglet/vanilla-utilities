/* domain name server protocol
 *
 * This portion of the code needs some major work.  I ported it (read STOLE IT)
 * from NCSA and lost about half the code somewhere in the process.
 *
 * Note, this is a user level process.  We include <tcp.h> not <wattcp.h>
 *
 *  0.4 : Aug 28, 1999 - added small dns cache, resolve_fn, only whole domain
 *  0.3 : Jan  8, 1994 - hooks for different RR types; rev lookups. messy!
 *  0.2 : Apr 24, 1991 - use substring portions of domain
 *  0.1 : Mar 18, 1991 - improved the trailing domain list
 *  0.0 : Feb 19, 1991 - pirated by Erick Engelke
 * -1.0 :              - NCSA code
 */

#include <copyright.h>
#include <stdio.h>
#include <string.h>
#include <mem.h>
#include <conio.h>
#include <tcp.h>

/*
 * #include <elib.h>
 */

/* These next 'constants' are loaded from WATTCP.CFG file */

char *def_domain;
char *loc_domain;	/* current subname to be used by the domain system */

longword def_nameservers[ MAX_NAMESERVERS ];
int _last_nameserver;
word _domaintimeout = 0;

static longword timeoutwhen;

/*
longword def_nameserver;
longword def2_nameserver;
*/
static udp_Socket *dom_sock;

#define DOMSIZE 512				/* maximum domain message size to mess with */

/*
 *  Header for the DOMAIN queries
 *  ALL OF THESE ARE BYTE SWAPPED QUANTITIES!
 *  We are the poor slobs who are incompatible with the world's byte order
 */
struct dhead {
    word	ident,		/* unique identifier */
		flags,
		qdcount,	/* question section, # of entries */
		ancount,	/* answers, how many */
		nscount,	/* count of name server RRs */
		arcount;	/* number of "additional" records */
};

/*
 *  flag masks for the flags field of the DOMAIN header
 */
#define DQR		0x8000	/* query = 0, response = 1 */
#define DOPCODE		0x7100	/* opcode, see below */
#define DAA		0x0400	/* Authoritative answer */
#define DTC		0x0200	/* Truncation, response was cut off at 512 */
#define DRD		0x0100	/* Recursion desired */
#define DRA		0x0080	/* Recursion available */
#define DRCODE		0x000F	/* response code, see below */

/* opcode possible values: */
#define DOPQUERY	0	/* a standard query */
#define DOPIQ		1	/* an inverse query */
#define DOPCQM		2	/* a completion query, multiple reply */
#define DOPCQU		3     	/* a completion query, single reply */
/* the rest reserved for future */

/* legal response codes: */
#define DROK	0		/* okay response */
#define DRFORM	1		/* format error */
#define DRFAIL	2		/* their problem, server failed */
#define DRNAME	3		/* name error, we know name doesn't exist */
#define DRNOPE	4		/* no can do request */
#define DRNOWAY	5		/* name server refusing to do request */

#define DTYPEA		1	/* host address resource record (RR) */
#define DTYPEPTR	12	/* a domain name ptr */

#define DIN		1	/* ARPA internet class */
#define DWILD		255	/* wildcard for several of the classifications */

/*
 *  a resource record is made up of a compressed domain name followed by
 *  this structure.  All of these ints need to be byteswapped before use.
 */
struct rrpart {
    word   	rtype,		/* resource record type = DTYPEA */
		rclass;		/* RR class = DIN */
    longword	ttl;		/* time-to-live, changed to 32 bits */
    word	rdlength;	/* length of next field */
    byte 	rdata[DOMSIZE];	/* data field */
};

/*
 *  data for domain name lookup
 */
static struct useek {
    struct dhead h;
    byte         x[DOMSIZE];
} *question;

typedef void (*unpacker_funct)(void *data,struct rrpart *rrp,struct useek *qp);

/*
 * prototypes
 */
static int packdom(char *dst, char *src);
static int countpaths(char *pathstring);
static int unpackdom(char *dst, char *src, char *buf);
static longword ddextract( struct useek *qp, byte type,
      unpacker_funct unpacker, void *data );
static char *getpath(char *pathstring, int whichone);
static int sendom(char *s, longword towho, word num, byte dtype);
static int udpdom(byte dtype, unpacker_funct unpacker, void *data );
static int Sdomain(char *mname, byte dtype, unpacker_funct unpacker,
      void *data, int adddom, longword nameserver, byte *timedout, sockfunct_t fn);  // S. Lawson
static int do_ns_lookup(char *name, byte dtype, unpacker_funct unpacker,
      void *data, sockfunct_t fn );		// S. Lawson

static void qinit( void )
{
    question->h.flags = intel16(DRD);
    question->h.qdcount = intel16(1);
    question->h.ancount = 0;
    question->h.nscount = 0;
    question->h.arcount = 0;
}

/*********************************************************************/
/*  packdom
*   pack a regular text string into a packed domain name, suitable
*   for the name server.
*
*   returns length
*/
static packdom( char *dst,char *src )
{
    char *p,*q,*savedst;
    int i,dotflag,defflag;

    p = src;
    dotflag = defflag = 0;
    savedst = dst;

    do {			/* copy whole string */
	*dst = 0;
	q = dst + 1;
	while (*p && (*p != '.'))
	    *q++ = *p++;

	i = (int) (p - src);
	if (i > 0x3f)
	    return(-1);
	*dst = i;
	*q = 0;

	if (*p) {					/* update pointers */
	    dotflag = 1;
	    src = ++p;
	    dst = q;
	}
	else if (!dotflag && !defflag && loc_domain) {
	    p = loc_domain;		/* continue packing with default */
	    defflag = 1;
	    src = p;
	    dst = q;
	}
    }
    while (*p);
    q++;
    return((int) (q-savedst));			/* length of packed string */
}

/*********************************************************************/
/*  unpackdom
*  Unpack a compressed domain name that we have received from another
*  host.  Handles pointers to continuation domain names -- buf is used
*  as the base for the offset of any pointer which is present.
*  returns the number of bytes at src which should be skipped over.
*  Includes the NULL terminator in its length count.
*/
static int unpackdom( char *dst, char *src, char *buf )
{
    int i,j,retval;
    char *savesrc;

    savesrc = src;
    retval = 0;

    while (*src) {
	j = *src;

	while ((j & 0xC0) == 0xC0) {
	    if (!retval)
		retval = (int) (src-savesrc+2);
	    src++;
	    src = &buf[(j & 0x3f)*256+*src];		/* pointer dereference */
	    j = *src;
	}

	src++;
	for (i=0; i < (j & 0x3f) ; i++)
	    *dst++ = *src++;

	*dst++ = '.';
    }

    *(--dst) = 0;			/* add terminator */
    src++;					/* account for terminator on src */

    if (!retval)
	retval = (int) (src-savesrc);

    return(retval);
}

/*********************************************************************/
/*  sendom
*   put together a domain lookup packet and send it
*   uses port 53
*	num is used as identifier
*/
static int sendom( char *s, longword towho, word num, byte dtype )
{
    word i,ulen;
    byte *psave,*p;

    psave = (byte*)&(question->x);
    i = packdom((char *)&(question->x),s);

    p = &(question->x[i]);
    *p++ = 0;				/* high byte of qtype */
    *p++ = dtype;			/* number is < 256, so we know high byte=0 */
    *p++ = 0;				/* high byte of qclass */
    *p++ = DIN;				/* qtype is < 256 */

    question->h.ident = intel16(num);
    ulen = sizeof(struct dhead)+(p-psave);

    udp_open( dom_sock, 997, towho, 53, NULL );    /* divide err */

    sock_write( (sock_type*)dom_sock, (byte*)question, ulen );
    return( ulen);
}

static int countpaths( char *pathstring )
{
    int     count = 0;
    char    *p;

    for(p=pathstring; (*p != 0) || (*(p+1) != 0); p++) {
	if(*p == 0)
	    count++;
    }
    return(++count);
}

static char *getpath( char *pathstring, int whichone )
            /* the path list to search      */
            /* which path to get, starts at 1 */
{
    char    *retval;

    if(whichone > countpaths(pathstring))
	return(NULL);
    whichone--;
    for(retval = pathstring;whichone ; retval++ ) {
	if(*retval == 0)
	    whichone--;
    }
    return(retval);
}

/*
 * Unpack a DTYPA RR (address record)
 */
static void typea_unpacker( void *data,   /* where to put IP */
                    struct rrpart *rrp, struct useek *qp )
{
   qp = qp;
   movmem(rrp->rdata,data,4);	/* save IP # 		*/
}

/*
 * Unpack a DTYPEPTR RR
 *
 * assumes: data buffer long enough for the name.
 *          256 chars should be enough(?)
 */
static void typeptr_unpacker( void *data,
                     struct rrpart *rrp, struct useek *qp )
{
   unpackdom((char*)data,(char*)rrp->rdata,(char*)qp);
}

/*********************************************************************/
/*  ddextract
*   extract the data from a response message.
*   returns the appropriate status code and if the data is available,
*   copies it into data
*   dtype is the RR type;  unpacker is the function to get the data
*         from the RR.
*/
static longword ddextract( struct useek *qp, byte dtype,
                        unpacker_funct unpacker, void *data )
{
    word i,j,nans,rcode;
    struct rrpart *rrp;
    byte *p,space[260];

    nans = intel16(qp->h.ancount);		/* number of answers */
    rcode = DRCODE & intel16(qp->h.flags);	/* return code for this message*/
    if (rcode > 0)
	return(rcode);

    if (nans > 0 &&								/* at least one answer */
    (intel16(qp->h.flags) & DQR)) {			/* response flag is set */
    p = (byte *)&qp->x;                 /* where question starts */
	i = unpackdom((char*)space,(char*)p,(char*)qp);    /* unpack question name */
	/*  spec defines name then  QTYPE + QCLASS = 4 bytes */
	p += i+4;
/*
 *  at this point, there may be several answers.  We will take the first
 *  one which has an IP number.  There may be other types of answers that
 *  we want to support later.
 */
	while (nans-- > 0) {					/* look at each answer */
	    i = unpackdom((char*)space,(char*)p,(char*)qp);	/* answer name to unpack */
	    /*			n_puts(space);*/
	    p += i;								/* account for string */
	    rrp = (struct rrpart *)p;			/* resource record here */
 /*
  *  check things which might not align on 68000 chip one byte at a time
  */
	    if (!*p && *(p+1) == dtype && 		/* correct type and class */
	    !*(p+2) && *(p+3) == DIN) {
		unpacker(data,rrp,qp);     /* get data we're looking for */
		return(0);						/* successful return */
	    }
	    movmem(&rrp->rdlength,&j,2);	/* 68000 alignment */
	    p += 10+intel16(j);				/* length of rest of RR */
	}
    }

    return(-1);						/* generic failed to parse */
}

/*********************************************************************/
/*  getdomain
*   Look at the results to see if our DOMAIN request is ready.
*   It may be a timeout, which requires another query.
*/

static int udpdom( byte dtype, unpacker_funct unpacker, void *data )
{
    int i,uret;

    uret = sock_fastread((sock_type*)dom_sock, (byte*)question, sizeof(struct useek ));
    /* this does not happen */
    if (uret < 0) {
	/*		netputevent(USERCLASS,DOMFAIL,-1);  */
   return(0);  /* ?? why'd I change this? -md */
//	return(-1);
    }

 /* num = intel16(question->h.ident); */     /* get machine number */
/*
 *  check to see if the necessary information was in the UDP response
 */

    i = (int) ddextract(question, dtype, unpacker, data);
    switch (i) {
        case 0: return(1); /* we got a response */
        case 3:	return(0);		/* name does not exist */
        case -1:return( 0 );		/* strange return code from ddextract */
	default:return( 0 );            /* dunno */
    }
}


/**************************************************************************/
/*  Sdomain
*   DOMAIN based name lookup
*   query a domain name server to get resource record data
*	Returns the data on a machine from a particular RR type.
*   Events generated will have this number tagged with them.
*   Returns various negative numbers on error conditions.
*
*   if adddom is nonzero, add default domain
*
*   Returns true if we got data, false otherwise.
*/
static int Sdomain( char *mname, byte dtype, unpacker_funct unpacker,
	    void *data, int adddom, longword nameserver, byte *timedout,
	    sockfunct_t fn ) 		// S. Lawson
/* int *timedout; set to 1 on timeout */
{
    char namebuff[512];
/*    int domainsremaining; */
    int /*status,*/ i;
    int result;
    int fnbroke;		// S. Lawson

    result = 0;
    fnbroke = 0;		// S. Lawson
    *timedout = 1;

    if (!nameserver) {	/* no nameserver, give up now */
	outs("No nameserver defined!\n\r");
	return(0);
    }

    while (*mname && *mname < 33) mname ++;   /* kill leading spaces */

    if (!(*mname))
	return(0);

    qinit();				/* initialize some flag fields */

    strcpy( namebuff, mname );

    if ( adddom ) {
	if(namebuff[strlen(namebuff)-1] != '.') {       /* if no trailing dot */
	    if(loc_domain) {             /* there is a search list */
//		domainsremaining = countpaths( loc_domain );    // why this here? -md

		strcat(namebuff,".");
		strcat(namebuff,getpath(loc_domain,1));
	    }
	} else
	    namebuff[ strlen(namebuff)-1] = 0;	/* kill trailing dot */
    }
    /*
     * This is not terribly good, but it attempts to use a binary
     * exponentially increasing delays.
     */

     for ( i = 2; i < 17; i *= 2) {
	sendom(namebuff,nameserver, 0xf001, dtype);	/* try UDP */

	ip_timer_init( dom_sock, i );
	do {
	    kbhit();
	    tcp_tick( dom_sock );
	    if (ip_timer_expired( dom_sock )) break;
	    if ( watcbroke ) {
		break;
	    }
	    if (chk_timeout( timeoutwhen ))
		break;

	    // S. Lawson - call the idle/break function
	    if (fn && fn(NULL)!=0) {
	       result=fnbroke=-1;
	       *timedout=1;
	       break;
	    }

	    if ( sock_dataready( dom_sock )) *timedout = 0;
	} while ( *timedout );

	if ( !*timedout ) break;	/* got an answer */
    }

    if ( !*timedout && !fnbroke)	// S. Lawson
	result = udpdom(dtype, unpacker, data);	/* process the received data */

    sock_close( dom_sock );
    return( result );
}

/*
 * nextdomain - given domain and count = 0,1,2,..., return next larger
 *		domain or NULL when no more are available
 */
static char *nextdomain( char *domain, int count )
{
    char *p;
    int i;

    p = domain;

// S. Lawson
#ifdef SCANDOMAIN		// scan all domain pieces
    for (i = 0; i < count; ++i) {
	p = strchr( p, '.' );
	if (!p) return( NULL );
	++p;
    }
#else                          // whole domain name only
    if (count > 0) return NULL;
#endif
    return( p );
}

/*
 * Perform a nameserver lookup of specified type using specified
 *    unpacker to extract data, if found.
 */
static int do_ns_lookup( char *name, byte dtype,
		     unpacker_funct unpacker, void *data, sockfunct_t fn )  // S. Lawson
{
/*    longword temp; */
    int result = 0;             // init to false
    int count, i;
    byte timeout[ MAX_NAMESERVERS ];
    struct useek qp;        /* temp buffer */
    udp_Socket ds;          /* temp buffer */
    word oldhndlcbrk;

    question = &qp;
    dom_sock = &ds;
    if (!name) return( 0 );
    rip( name );

    if (!_domaintimeout) _domaintimeout = sock_delay << 2;
    timeoutwhen = set_timeout( _domaintimeout );

    count = 0;
    memset( &timeout, 0, sizeof( timeout ));

    oldhndlcbrk = wathndlcbrk;
    wathndlcbrk = 1;        /* enable special interrupt mode */
    watcbroke = 0;
    do {
	if ( (loc_domain = nextdomain( def_domain, count )) == NULL )
		count = -1;	/* use default name */

	for ( i = 0; i < _last_nameserver ; ++i ) {
	    if (!timeout[i])
		if ((result = Sdomain( name, dtype, unpacker, data, count != -1 ,
			def_nameservers[i], &timeout[i], fn)) == 1)	// S. Lawson
		    break;	/* got name, bail out of loop */
		if (result==-1) break;		// S. Lawson
	}

	if (count == -1) break;
	count++;
    } while (!result);
    watcbroke = 0;          /* always clean up */
    wathndlcbrk = oldhndlcbrk;

    if (result==-1) result=0;		// S. Lawson
    return( result );
}

// S. Lawson - keep a resolve(char *) version
longword resolve( char *name )
{
    return (resolve_fn(name,NULL));
}


/*
 * resolve()
 * 	convert domain name -> address resolution.
 * 	returns 0 if name is unresolvable right now
 */
longword resolve_fn( char *name, sockfunct_t fn )	// S. Lawson
{
    longword ipaddr;
    // S. Lawson
#define DNSCACHESIZE 4			// cache up to 4 names
#define DNSCACHELENGTH 32		//   up to 32 characters
#define DNSCACHETIMEOUT 120             //     for up to 2 minutes
    static char DNScacheName[DNSCACHESIZE][DNSCACHELENGTH];
    static longword DNScacheIP[DNSCACHESIZE];
    static longword DNScacheTimeout[DNSCACHESIZE]={0,0,0,0};
    static char DNScacheNext=0;
    int DNScacheScan;

    if( !name ) return 0L;

    rip( name );			// S. Lawson - trim for cache scan
    if ( isaddr( name ))
	 return( aton( name ));

    // S. Lawson
    for (DNScacheScan=0 ; DNScacheScan<DNSCACHESIZE ; DNScacheScan++) {
       if (DNScacheTimeout[DNScacheScan]==0L) continue;
       if (chk_timeout(DNScacheTimeout[DNScacheScan])) {
	  DNScacheTimeout[DNScacheScan]=0L;
	  continue;
       }
       if(!strcmpi(DNScacheName[DNScacheScan],name))
	  return DNScacheIP[DNScacheScan];
    }

#ifdef NOTUSED	// S. Lawson
    if( do_ns_lookup(name, DTYPEA, typea_unpacker, &ipaddr) )
       return (intel(ipaddr));
    else return (0L);
#else	// S. Lawson
    if( do_ns_lookup(name, DTYPEA, typea_unpacker, &ipaddr, fn) ) {
       strncpy(DNScacheName[DNScacheNext], name, DNSCACHELENGTH);
       DNScacheName[DNScacheNext][DNSCACHELENGTH-1]='\0';
       DNScacheIP[DNScacheNext]=intel(ipaddr);
       DNScacheTimeout[DNScacheNext]=set_timeout(DNSCACHETIMEOUT);
       if (++DNScacheNext>=DNSCACHESIZE) DNScacheNext=0;
       return (intel(ipaddr));
    }
    return (0L);
#endif	// S. Lawson
}

// S. Lawson - keep version w/o fn
int reverse_addr_lookup( longword ipaddr, char *name )
{
    return(reverse_addr_lookup_fn(ipaddr, name, NULL));
}

/*
 * reverse_addr_lookup()
 *    lookup a hostname based on IP address
 *          (PTR lookups in the .in-addr.arpa domain)
 *
 * Returns true if the info was found, false otherwise.
 *
 * assumes: name buffer is big enough for the longest hostname
 */
int reverse_addr_lookup_fn( longword ipaddr, char *name, sockfunct_t fn )
{
   char revname[64];

   inet_ntoa( revname, ntohl(ipaddr) );   /* ntohl flips order */
   strcat(revname, ".IN-ADDR.ARPA.");

   return ( do_ns_lookup(revname, DTYPEPTR, typeptr_unpacker, (void*)name, fn) );   // S. Lawson
}
