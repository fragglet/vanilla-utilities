#include <copyright.h>
#include <stdio.h>
#include <string.h>
#include <tcp.h>

/* domain name server protocol
 *
 * This portion of the code needs some major work.  I ported it (read STOLE IT)
 * from NCSA and lost about half the code somewhere in the process.
 *
 * Note, this is a user level process.  We include <tcp.h> not <wattcp.h>
 *
 *  0.2 : Apr 24, 1991 - use substring portions of domain
 *  0.1 : Mar 18, 1991 - improved the trailing domain list
 *  0.0 : Feb 19, 1991 - pirated by Erick Engelke
 * -1.0 :              - NCSA code
 */


/* These next 'constants' are loaded from WATTCP.CFG file */

extern char *def_domain;
extern char *loc_domain;       /* current subname to be used by the domain system */

extern longword def_nameservers[ MAX_NAMESERVERS ];
extern int _last_nameserver;
extern word _domaintimeout = 0;

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
#define DOPIQ           1       /* an inverse query */
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


/*********************************************************************/
/*  packdom
*   pack a regular text string into a packed domain name, suitable
*   for the name server.
*
*   returns length
*/
static packdom(dst,src)
char *src,*dst;
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

	i = p - src;
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
    return(q-savedst);			/* length of packed string */
}

/*********************************************************************/
/*  unpackdom
*  Unpack a compressed domain name that we have received from another
*  host.  Handles pointers to continuation domain names -- buf is used
*  as the base for the offset of any pointer which is present.
*  returns the number of bytes at src which should be skipped over.
*  Includes the NULL terminator in its length count.
*/
static unpackdom(dst,src,buf)
char *src,*dst,buf[];
{
    int i,j,retval;
    char *savesrc;

    savesrc = src;
    retval = 0;

    while (*src) {
	j = *src;

	while ((j & 0xC0) == 0xC0) {
	    if (!retval)
		retval = src-savesrc+2;
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
	retval = src-savesrc;

    return(retval);
}

/*********************************************************************/
/*  sendom
*   put together a domain lookup packet and send it
*   uses port 53
*	num is used as identifier
*/
static sendom(s,towho,num)
char *s;
longword towho;
word num;
{
    word i,ulen;
    byte *psave,*p;

    psave = (byte*)&(question->x);
    i = packdom(&(question->x),s);

    p = &(question->x[i]);
    *p++ = 0;				/* high byte of qtype */
    *p++ = DTYPEA;			/* number is < 256, so we know high byte=0 */
    *p++ = 0;				/* high byte of qclass */
    *p++ = DIN;				/* qtype is < 256 */

    question->h.ident = intel16(num);
    ulen = sizeof(struct dhead)+(p-psave);

    udp_open( dom_sock, 997, towho, 53, NULL );    /* divide err */

    sock_write( dom_sock, question, ulen );
    return( ulen);
}


/*********************************************************************/
/*  senrdom
*   put together a reverse domain lookup packet and send it
*   uses port 53
*	num is used as identifier
*/
static sendrdom(char *s, longword towhom, word num )
{
    word i,ulen;
    byte *psave,*p;



    psave = (byte*)&(question->x);
    i = packdom(&(question->x),s);

    p = &(question->x[i]);
    *p++ = 0;				/* high byte of qtype */
    *p++ = DTYPEA;                      /* want host address */
    *p++ = 0;				/* high byte of qclass */
    *p++ = DIN;				/* qtype is < 256 */

    question->h.ident = intel16(num);
    ulen = sizeof(struct dhead)+(p-psave);

    udp_open( dom_sock, 997, towho, 53, NULL );    /* divide err */

    sock_write( dom_sock, question, ulen );
    return( ulen);
}

static int countpaths(pathstring)
char *pathstring;
{
    int     count = 0;
    char    *p;

    for(p=pathstring; (*p != 0) || (*(p+1) != 0); p++) {
	if(*p == 0)
	    count++;
    }
    return(++count);
}

static char *getpath(pathstring,whichone)
char *pathstring;            /* the path list to search      */
int   whichone;               /* which path to get, starts at 1 */
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

/*********************************************************************/
/*  ddextract
*   extract the ip number from a response message.
*   returns the appropriate status code and if the ip number is available,
*   copies it into mip
*/
static longword ddextract(qp,mip)
struct useek *qp;
unsigned char *mip;
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
	i = unpackdom(space,p,qp);				/* unpack question name */
	/*  spec defines name then  QTYPE + QCLASS = 4 bytes */
	p += i+4;
printf(" NAME : ? %s\n", space );
        
/*
 *  at this point, there may be several answers.  We will take the first
 *  one which has an IP number.  There may be other types of answers that
 *  we want to support later.
 */
	while (nans-- > 0) {					/* look at each answer */
	    i = unpackdom(space,p,qp);			/* answer name to unpack */
	    /*			n_puts(space);*/
	    p += i;								/* account for string */
	    rrp = (struct rrpart *)p;			/* resource record here */
 /*
  *  check things which might not align on 68000 chip one byte at a time
  */
	    if (!*p && *(p+1) == DTYPEA && 		/* correct type and class */
	    !*(p+2) && *(p+3) == DIN) {
		movmem(rrp->rdata,mip,4);	/* save IP # 		*/
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

static longword udpdom()
{
    int i,uret;
    longword desired;

    uret = sock_fastread(dom_sock, question, sizeof(struct useek ));
    /* this does not happen */
    if (uret < 0) {
	/*		netputevent(USERCLASS,DOMFAIL,-1);  */
	return(-1);
    }

 /* num = intel16(question->h.ident); */     /* get machine number */
/*
 *  check to see if the necessary information was in the UDP response
 */

    i = ddextract(question, &desired);
    switch (i) {
        case 3:	return(0);		/* name does not exist */
        case 0: return(intel(desired)); /* we found the IP number */
        case -1:return( 0 );		/* strange return code from ddextract */
        default:return( 0 );            /* dunno */
    }
}


/**************************************************************************/
/*  Sdomain
*   DOMAIN based name lookup
*   query a domain name server to get an IP number
*	Returns the machine number of the machine record for future reference.
*   Events generated will have this number tagged with them.
*   Returns various negative numbers on error conditions.
*
*   if adddom is nonzero, add default domain
*/
static longword Sdomain(mname, adddom, nameserver, timedout )
char *mname;
int adddom;
longword nameserver;
int *timedout;	/* set to 1 on timeout */
{
    char namebuff[512];
    int isaname;
    int domainsremaining;
    int status, i;
    longword response;

    isaname = !isaddr( mname );
    response = 0;
    *timedout = 1;

    if (!nameserver) {	/* no nameserver, give up now */
	outs("No nameserver defined!\n\r");
	return(0);
    }

    while (*mname && *mname < 33) mname ++;   /* kill leading spaces */

    if (!(*mname))
	return(0L);

    question->h.flags = intel16(DRD);   /* use recursion */
    question->h.qdcount = 0;
    question->h.ancount = 0;
    question->h.nscount = 0;
    question->h.arcount = 0;

    qinit();				/* initialize some flag fields */

    strcpy( namebuff, mname );

    if ( isaname ) {
        /* forward lookup */
        /* only add these things if we are doing a real name */

        question->h.qdcount = intel16(1);
        if ( adddom ) {
            if(namebuff[strlen(namebuff)-1] != '.') {       /* if no trailing dot */
                if(loc_domain) {             /* there is a search list */
                    domainsremaining = countpaths( loc_domain );

                    strcat(namebuff,".");
                    strcat(namebuff,getpath(loc_domain,1));
                }
            } else
                namebuff[ strlen(namebuff)-1] = 0;  /* kill trailing dot */
        }
    } else {
        /* reverse lookup */
        question->h.ancount = intel16(1);

    }
    /*
     * This is not terribly good, but it attempts to use a binary
     * exponentially increasing delays.
     */

     for ( i = 2; i < 17; i *= 2) {
        if ( isaname ) sendom(namebuff,nameserver, 0xf001);    /* try UDP */
        else sendrdom( namebuff, nameserver, 0xf001 );

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
            if ( sock_dataready( dom_sock )) *timedout = 0;
        } while ( *timedout );

	if ( !*timedout ) break;	/* got an answer */
    }

    if ( !*timedout )
	response = udpdom();	/* process the received data */

    sock_close( dom_sock );
    return( response );
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

    for (i = 0; i < count; ++i) {
	p = strchr( p, '.' );
	if (!p) return( NULL );
	++p;
    }
    return( p );
}


/*
 * newresolve()
 * 	convert domain name -> address resolution.
 * 	returns 0 if name is unresolvable right now
 */
longword newresolve(name)
char *name;
{
    longword ip_address, temp;
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
	if (!(loc_domain = nextdomain( def_domain, count )))
		count = -1;	/* use default name */

	for ( i = 0; i < _last_nameserver ; ++i ) {
	    if (!timeout[i])
		if (ip_address = Sdomain( name , count != -1 ,
			def_nameservers[i], &timeout[i] ))
		    break;	/* got name, bail out of loop */
	}

	if (count == -1) break;
	count++;
    } while (!ip_address);
    watcbroke = 0;          /* always clean up */
    wathndlcbrk = oldhndlcbrk;

    return( ip_address );
}



main(int argc, char **argv )
{
    char *name;
    longword host;
    int status,i;
    char buffer[ 20];

    if (argc < 2) {
        puts("HOST  hostname");
        exit(3);
    }

    sock_init();

    for ( i = 1 ; i < argc ; ++i ) {
        if ((*argv[i] == '/') || (*argv[i] == '-' )) {
        }
        else {
            name = strdup( argv[i] );

            printf("resolving \"%s\"...", name );
            host = newresolve( name );
            printf("\r");
            clreol();
        }
    }
    if ( host ) {
        printf("\r");
        clreol();
        printf("%s : %s\n", name, inet_ntoa( buffer , host ));
    }
    exit( host ? 0 : 1 );
}
/* udpdom &&  ddextract(question, &desired); */

