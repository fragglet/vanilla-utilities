
#ifndef _wattcp_wattcp_h
#define _wattcp_wattcp_h

/*
 * Are we compiling the kernel?
 *    or an application using the kernel?
 */
#if !defined(__WATTCP_USER__)
#define __WATTCP_KERNEL__
#endif

/*
 * Note that some stuff is not available to user applications.
 *   This is generally detail you shouldn't need to worry about,
 *   and best stay away from to preserve the kernel integrity.
 * Note also that there is a lot of other stuff that should probably
 *   be protected but isn't.
 */

#define WATTCPH

/* these are visible for select.c routine return values */
#define SOCKESTABLISHED 1
#define SOCKDATAREADY   2
#define SOCKCLOSED      4

#if defined(__WATTCP_KERNEL__)

#define IP_TYPE     0x0008

/*
#define DEBUG
*/

#include <stdio.h>
#include <elib.h>

#define MAX_GATE_DATA 12
#define MAX_STRING 50	/* most strings are limited */
#endif  /* defined(__WATTCP_KERNEL__) */

#define MAX_NAMESERVERS 10
#define MAX_COOKIES 10

#if defined(__WATTCP_KERNEL__)

#define MAXVJSA     1440 /* 10 s */
#define MAXVJSD     360  /* 10 s */
#define SAFETYTCP  0x538f25a3L
#define SAFETYUDP  0x3e45e154L
#define TRUE        1
#define true        TRUE
#define FALSE       0
#define false       FALSE

#define EL_INUSE        0x0001
#define EL_DELAY        0x0002
#define EL_TCP          0x0004
#define EL_SERVER       0x0008
#define EL_ASCII        0x0010
#define EL_NEVER        0x0020

/* These are Ethernet protocol numbers but I use them for other things too */
#define UDP_PROTO  0x11
#define TCP_PROTO  0x06
#define ICMP_PROTO 0x01

#endif /* defined(__WATTCP_KERNEL__) */

#define TCP_MODE_BINARY  0       /* default mode */
#define TCP_MODE_ASCII   1
#define UDP_MODE_CHK     0       /* default to having checksums */
#define UDP_MODE_NOCHK   2       /* turn off checksums */
#define TCP_MODE_NAGLE   0       /* Nagle algorithm */
#define TCP_MODE_NONAGLE 4

typedef unsigned long longword;     /* 32 bits */
typedef unsigned short word;        /* 16 bits */
typedef unsigned char byte;         /*  8 bits */

typedef struct { byte eaddr[6]; } eth_address;    // 94.11.19 -- made an array

#if defined(__WATTCP_KERNEL__)

/* undesirable */
extern longword MsecClock();
#define clock_ValueRough() MsecClock()

#define TICKS_SEC 18

#define checksum( p, len) inchksum( p, len )

#define PD_ETHER 1
#define PD_SLIP  6

extern word sock_inactive;      /* in pcbootp.c */
extern word _pktdevclass;
extern word _mss;
extern word _bootptimeout;	/* in pcbootp.c */
extern longword _bootphost;	/* in pcbootp.c */
extern word _bootpon;

/* The Ethernet header */
typedef struct {
    eth_address     destination;
    eth_address     source;
    word            type;
} eth_Header;

/* The Internet Header: */
typedef struct {
    unsigned	    hdrlen  : 4;
    unsigned	    ver     : 4;
    byte	    tos;
    word            length;
    word            identification;
    word            frags;
    byte	    ttl;
    byte	    proto;
    word            checksum;
    longword        source;
    longword        destination;
} in_Header;


#define in_GetVersion(ip) ( (ip)->ver )
#define in_GetHdrlen(ip)  ( (ip)->hdrlen )  /* 32 bit word size */
#define in_GetHdrlenBytes(ip)  ( in_GetHdrlen(ip) << 2 ) /* 8 bit byte size */
#define in_GetTos(ip)      ( (ip)->tos)

#define in_GetTTL(ip)      ((ip)->ttl)
#define in_GetProtocol(ip) ((ip)->proto )

typedef struct {
    word	    srcPort;
    word	    dstPort;
    word	    length;
    word	    checksum;
} udp_Header;

#define UDP_LENGTH ( sizeof( udp_Header ))

typedef struct {
    word            srcPort;
    word            dstPort;
    longword        seqnum;
    longword        acknum;
    word            flags;
    word            window;
    word            checksum;
    word            urgentPointer;
} tcp_Header;

#define tcp_FlagFIN     0x0001
#define tcp_FlagSYN     0x0002
#define tcp_FlagRST     0x0004
#define tcp_FlagPUSH    0x0008
#define tcp_FlagACK     0x0010
#define tcp_FlagURG     0x0020
#define tcp_FlagDO      0xF000
#define tcp_GetDataOffset(tp) (intel16((tp)->flags) >> 12)

#endif /* defined(__WATTCP_KERNEL__) */

/* The TCP/UDP Pseudo Header */
typedef struct {
    longword    src;
    longword    dst;
    byte        mbz;
    byte        protocol;
    word        length;
    word        checksum;
} tcp_PseudoHeader;


/* A datahandler for tcp or udp sockets */
typedef int (*dataHandler_t)( void *s, byte *data, int len, tcp_PseudoHeader *pseudohdr, void *protohdr );
/* A socket function for delay routines */
typedef int (*sockfunct_t)( void *s );

#if defined(__WATTCP_KERNEL__)
/*
 * TCP states, from tcp manual.
 * Note: close-wait state is bypassed by automatically closing a connection
 *       when a FIN is received.  This is easy to undo.
 */
#define tcp_StateLISTEN  0      /* listening for connection */
#define tcp_StateSYNSENT 1      /* syn sent, active open */
#define tcp_StateSYNREC  2      /* syn received, synack+syn sent. */
#define tcp_StateESTAB   3      /* established */
#define tcp_StateESTCL   4      /* established, but will FIN */
#define tcp_StateFINWT1  5      /* sent FIN */
#define tcp_StateFINWT2  6      /* sent FIN, received FINACK */
#define tcp_StateCLOSWT  7      /* received FIN waiting for close */
#define tcp_StateCLOSING 8      /* sent FIN, received FIN (waiting for FINACK) */
#define tcp_StateLASTACK 9      /* fin received, finack+fin sent */
#define tcp_StateTIMEWT  10     /* dally after sending final FINACK */
#define tcp_StateCLOSEMSL 11
#define tcp_StateCLOSED  12     /* finack received */

#define tcp_MaxBufSize 2048         /* maximum bytes to buffer on input */

#endif


#if defined(__WATTCP_KERNEL__)
/*
 * UDP socket definition
 */
typedef struct _udp_socket {
    struct _udp_socket *next;
    word	    ip_type;		/* always set to UDP_PROTO */
    char	   *err_msg;		/* null when all is ok */
    char           *usr_name;
    void	  (*usr_yield)( void );
    byte            rigid;
    byte            stress;
    word	    sock_mode;	        /* a logical OR of bits */
    longword	    usertimer;		/* ip_timer_set, ip_timer_timeout */
    dataHandler_t  dataHandler;
    eth_address     hisethaddr;		/* peer's ethernet address */
    longword        hisaddr;		/* peer's internet address */
    word	    hisport;		/* peer's UDP port */
    longword        myaddr;
    word	    myport;
    word            locflags;

    int             queuelen;
    byte           *queue;

    int             rdatalen;           /* must be signed */
    word            maxrdatalen;
    byte           *rdata;
    byte            rddata[ tcp_MaxBufSize + 1];         /* if dataHandler = 0, len == 512 */
    longword        safetysig;
} udp_Socket;
#else /* __WATTCP_USER */
/*
 * Don't give users access to the fields.
 */
typedef struct {
    byte undoc[ 2200 ];
} udp_Socket;
#endif /* __WATTCP_USER__ */

#if defined(__WATTCP_KERNEL__)
/*
 * TCP Socket definition
 */
typedef struct _tcp_socket {
    struct _tcp_socket *next;
    word	    ip_type;	    /* always set to TCP_PROTO */
    char 	   *err_msg;
    char           *usr_name;
    void          (*usr_yield)(void);
    byte            rigid;
    byte            stress;
    word	    sock_mode;	    /* a logical OR of bits */

    longword	    usertimer;	    /* ip_timer_set, ip_timer_timeout */
    dataHandler_t   dataHandler;    /* called with incoming data */
    eth_address     hisethaddr;     /* ethernet address of peer */
    longword        hisaddr;        /* internet address of peer */
    word            hisport;	    /* tcp ports for this connection */
    longword        myaddr;
    word	    myport;
    word            locflags;

    int             queuelen;
    byte           *queue;

    int             rdatalen;       /* must be signed */
    word            maxrdatalen;
    byte           *rdata;
    byte            rddata[tcp_MaxBufSize+1];    /* received data */
    longword        safetysig;
    word	    state;          /* connection state */

    longword        acknum;
    longword	    seqnum; 	    /* data ack'd and sequence num */
    long            timeout;        /* timeout, in milliseconds */
    byte            unhappy;        /* flag, indicates retransmitting segt's */
    byte            recent;         /* 1 if recently transmitted */
    word            flags;          /* tcp flags word for last packet sent */

    word	    window;	    /* other guy's window */
    int 	    datalen;        /* number of bytes of data to send */
				    /* must be signed */
    int             unacked;        /* unacked data */

    byte	    cwindow;	    /* Van Jacobson's algorithm */
    byte	    wwindow;

    word	    vj_sa;	    /* VJ's alg, standard average */
    word	    vj_sd;	    /* VJ's alg, standard deviation */
    longword	    vj_last;	    /* last transmit time */
    word	    rto;
    byte	    karn_count;	    /* count of packets */
    byte            tos;            /* priority */
    /* retransmission timeout proceedure */
    /* these are in clock ticks */
    longword        rtt_lasttran;       /* last transmission time */
    longword        rtt_smooth;         /* smoothed round trip time */
    longword        rtt_delay;          /* delay for next transmission */
    longword        rtt_time;           /* time of next transmission */

    word            mss;
    longword        inactive_to;           /* for the inactive flag */
    int             sock_delay;

    byte            data[tcp_MaxBufSize+1]; /* data to send */
    longword        datatimer;          /* EE 99.08.23 note broken connections */
    longword	    frag[2];		/* S. Lawson - handle one dropped segment */
} tcp_Socket;
#else /* __WATTCP_USER */
/*
 * Don't give users access to the fields.
 */
typedef struct {
    byte undoc[ 4300 ];
} tcp_Socket;
#endif /* __WATTCP_USER__ */

#if defined(__WATTCP_KERNEL__)
/* sock_type used for socket io */
typedef union {
    udp_Socket udp;
    tcp_Socket tcp;
} sock_type;
#else /* __WATTCP_USER__ */
typedef void sock_type;
#endif /* __WATTCP_USER__ */

/* similar to UNIX */
typedef struct sockaddr {
    word        s_type;
    word        s_port;
    longword    s_ip;
    byte        s_spares[6];    /* unused in TCP realm */
} sockaddr;
#define sockaddr_in sockaddr

        /*
         * TCP/IP system variables - do not change these since they
         *      are not necessarily the source variables, instead use
         *      ip_Init function
         */
extern longword my_ip_addr;
extern longword sin_mask;       /* eg.  0xfffffe00L */
extern word sock_delay;
extern word sock_data_timeout;  /* EE 99.08.23 */

#if defined(__WATTCP_KERNEL__)
extern eth_address _eth_addr;
extern eth_address _eth_brdcast;
#endif


#if defined(__WATTCP_KERNEL__)
/*
 * ARP definitions
 */
#define arp_TypeEther  0x100		/* ARP type of Ethernet address */

/* arp op codes */
#define ARP_REQUEST 0x0100
#define ARP_REPLY   0x0200

/*
 * Arp header
 */
typedef struct {
    word            hwType;
    word            protType;
    word            hwProtAddrLen;  // hw and prot addr len
    word            opcode;
    eth_address     srcEthAddr;
    longword        srcIPAddr;
    eth_address     dstEthAddr;
    longword        dstIPAddr;
} arp_Header;

#if !defined(ETH_MSS)   // S. Lawson - allow setting in makefile
#define ETH_MSS 1500  // MSS for Ethernet
#endif                  // S. Lawson

byte *fragment( in_Header * ip );
void timeout_frags( void );

#endif


/*
 * Ethernet interface -- pcsed.c
 */
int  _eth_init( void );			// S. Lawson
byte *_eth_formatpacket( eth_address *eth_dest, word eth_type );
int   _eth_send( word len );
void  _eth_free( void *buf );
byte *_eth_arrived( word *type_ptr );
void  _eth_release( void );
#if defined(__WATTCP_KERNEL__)
extern void *_eth_hardware( byte *p );
#endif


/*
 * timers -- pctcp.c
 */
void ip_timer_init( sock_type *s, int delayseconds );
int ip_timer_expired( sock_type *s );
longword MsecClock( void );


/*
 * sock_init()  -- initialize wattcp libraries -- sock_ini.c
 */
void sock_init(void);
int sock_init_noexit(void);     // S. Lawson
void sock_exit( void );   /* normally called via atexit() in sock_init() */


        /*
         * tcp_init/tcp_shutdown -- pctcp.c
         *      - init/kill all tcp and lower services
         *      - only call if you do not use sock_init
         * (NOT RECOMMENDED)
         */
void tcp_shutdown(void);
int tcp_init_noexit(void);      // S. Lawson
void tcp_init(void);
void tcp_set_ports(word tcp_base, word udp_base);	// S. Lawson
void tcp_get_ports(word *tcp_base, word *udp_base);	// S. Lawson

/*
 * things you probably won't need to know about
 */
	/*
	 * sock_debugdump -- sock_dbu.c
	 *	- dump some socket control block parameters
	 * used for testing the kernal, not recommended
	 */
void sock_debugdump( sock_type *s );
        /*
         * tcp_config - read a configuration file
         *            - if special path desired, call after sock_init()
         *            - null reads path from executable
         * see sock_init();
         */
int tcp_config( char *path );
	/* S. Lawson
	 * tcp_config_file - sets the configuration filename
	 *                 - null silently skips config file processing
	 */
void tcp_config_file( const char *fname );
	/*
         * tcp_tick - called periodically by user application in sock_wait_...
         *          - returns 1 when our socket closes
         */
int tcp_tick( sock_type *s );
        /*
         * Retransmitter - called periodically to perform tcp retransmissions
         *          - normally called from tcp_tick, you have to be pretty
         *            low down to use this one
         */
void tcp_Retransmitter(void);
        /*
         * tcp_set_debug_state - set 1 or reset 0 - depends on what I have done
         */
void tcp_set_debug_state( int x );

/*
 * check for bugs -- pctcp.c
 */
int tcp_checkfor( sock_type *t );

/*
 * Timeout routines.
 */
unsigned long set_timeout( unsigned int seconds );
unsigned long set_ttimeout( unsigned int ticks );
int chk_timeout( unsigned long timeout );

/*
 * socket macros
 */

/*
 * sock_wait_established()
 *	- waits then aborts if timeout on s connection
 * sock_wait_input()
 *	- waits for received input on s
 * - may not be valid input for sock_gets... check returned length
 * sock_tick()
 *	- do tick and jump on abort
 * sock_wait_closed();
 *	- discards all received data
 *
 * jump to sock_err with contents of *statusptr set to
 *	 1 on closed
 *	-1 on timeout
 *
 */

int _ip_delay0( sock_type *s, int timeoutseconds, sockfunct_t fn, int *statusptr );
int _ip_delay1( sock_type *s, int timeoutseconds, sockfunct_t fn, int *statusptr);
int _ip_delay2( sock_type *s, int timeoutseconds, sockfunct_t fn, int *statusptr);


#if defined(__WATTCP_KERNEL__)
#define set_mstimeout( x ) (set_timeout(0)+ (x / 55))
#endif  /* defined(__WATTCP_KERNEL__) */

#define sock_wait_established( s, seconds, fn, statusptr ) \
    if (_ip_delay0( s, seconds, fn, statusptr )) goto sock_err;
#define sock_wait_input( s, seconds, fn , statusptr ) \
    if (_ip_delay1( s, seconds, fn, statusptr )) goto sock_err;
#define sock_tick( s, statusptr ) \
    if ( !tcp_tick(s)) { *statusptr = 1 ; goto sock_err; }
#define sock_wait_closed(s, seconds, fn, statusptr )\
    if (_ip_delay2( s, seconds, fn, statusptr )) goto sock_err;

/*
 * TCP or UDP specific stuff, must be used for open's and listens, but
 * sock stuff is used for everything else -- pctcp.c
 */
int tcp_open( tcp_Socket *s, word lport, longword ina, word port, dataHandler_t datahandler );
int udp_open( udp_Socket *s, word lport, longword ina, word port, dataHandler_t datahandler );
int tcp_listen( tcp_Socket *s, word lport, longword ina, word port, dataHandler_t datahandler, word timeout );
int tcp_established( tcp_Socket *s );

/*
 * Clean up a string -- pctcp.c
 */
char *rip( char *s);

/*
 * Name service / name lookup routines -- udp_dom.c
 */
longword resolve( char *name );
longword resolve_fn( char *name, sockfunct_t fn );     // S. Lawson
int reverse_addr_lookup( longword ipaddr, char *name );
int reverse_addr_lookup_fn( longword ipaddr, char *name, sockfunct_t fn );  // S. Lawson

/*
 * less general functions
 */
longword intel( longword x );
word intel16( word x );
longword MsecClock( void );

/*
 * Ctrl-break handling -- pc_cbrk.c
 */
void tcp_cbrk( int mode );

void outs( char far * string );

#if defined(__WATTCP_KERNEL__)
/* icmp handler -- pcicmp.c */
int icmp_handler( in_Header *ip );
void icmp_Unreach( in_Header *ip);	// S. Lawson
#endif


/*
 * ARP -- pcarp.c
 */
#if defined(__WATTCP_KERNEL__)
void _arp_add_gateway( char *data, longword ip );
int _arp_handler( arp_Header *in );
#endif
void _arp_register( longword use, longword instead_of );
void _arp_tick( longword ip );      /* kernel only? */
int _arp_resolve( longword ina, eth_address *ethap, int nowait );



/*
 * Packet -- pcpkt.c
 *
 * These probably shouldn't be visible to user app code.
 */
eth_address *_pkt_eth_init( void );
int pkt_send( char *buffer, int length );
void pkt_buf_wipe( void );
void pkt_buf_release( char *ptr );
void * pkt_received( void );
void pkt_release( void );

#if defined(__WATTCP_KERNEL__)
void _add_server( int *counter, int max, longword *array, longword value );
extern word debug_on;
#endif


/*
 * pcbsd.c
 */
int _chk_socket( sock_type *s );
char *inet_ntoa( char *s, longword x );
longword inet_addr( char *s );
char *sockerr( sock_type *s );
char *sockstate( sock_type *s );
longword gethostid( void );
longword sethostid( longword ip );
word ntohs( word a );
word htons( word a );
longword ntohl( longword x );
longword htonl( longword x );


#if defined(__WATTCP_KERNEL__)
void *_tcp_lookup( longword hisip, word hisport, word myport );

void _tcp_cancel( in_Header *ip, int code, char *msg, longword dummyip );
void _udp_cancel( in_Header *ip );

int _dobootp(void);
#endif


/*
 * General socket I/O -- pctcp.c
 */
word sock_mode( sock_type *, word);        /* see TCP_MODE_... */
void sock_abort( sock_type *);
void tcp_sendsoon( tcp_Socket *s );
int sock_fastwrite( sock_type *, byte *, int );
int sock_write( sock_type *, byte *, int );
int sock_read( sock_type *, byte *, int );
int sock_fastread( sock_type *, byte *, int );
int sock_gets( sock_type *, byte *, int );
void sock_close( sock_type *s );

byte sock_putc( sock_type  *s, byte c );
int sock_getc( sock_type  *s );
int sock_puts( sock_type  *s, byte *dp );
int sock_gets(sock_type *, byte *, int );

int sock_setbuf( sock_type *s, byte *dp, int len );

int sock_yield( tcp_Socket *s, void (*fn)( void ) );


/*
 *   s is the pointer to a udp or tcp socket
 */


/*
 * Socket text output/input routines -- sock_prn.c
 */
int sock_printf( sock_type  *s, char *format, ... );
int sock_scanf( sock_type  *s, char *format, ... );


/*
 * Misc. socket I/O -- pctcp.c
 */
int sock_setbuf( sock_type *s, byte *dp, int len );
int sock_enqueue( sock_type  *s, byte *dp, int len );
void sock_noflush( sock_type *s );
void sock_flush( sock_type  *s );
void sock_flushnext( sock_type  *s);
int sock_dataready( sock_type  *s );
int sock_established( sock_type *s );
void sock_sturdy( sock_type *s, int level );


/*
 * Debug output -- pcdbug.c
 */
void db_write( char *msg );
void db_open( void );
void db_close( void );
void dbug_printf( char *, ... );
void dbug_init( void );
void dbug_init( void );



/*
 * Socket Select -- select.c
 */
int sock_sselect( sock_type *s, int waitstate );



/*
 * recv routines -- pcrecv.c
 */
int sock_recv_init( sock_type *s, void *space, word len );
int sock_recv_from( sock_type *s, long *hisip, word *hisport, char *buffer, int len, word flags );
int sock_recv( sock_type *s, char *buffer, int len, word flags );


/*
 * bsd-similar stuff -- pcbuf.c
 */
int sock_rbsize( sock_type *s );
int sock_rbused( sock_type *s );
int sock_rbleft( sock_type *s );
int sock_tbsize( sock_type *s );
int sock_tbused( sock_type *s );
int sock_tbleft( sock_type *s );
int sock_preread( sock_type *s, byte *dp, int len );


/*
 * Name conversion stuff -- udp_nds.c
 */
longword aton( char *text );
int isaddr( char *text );

/*
 * Configuration -- pcconfig.c
 */
char *_inet_atoeth( char *src, byte *eth );
void _add_server( int *counter, int max, longword *array, longword value );
int tcp_config( char *path );

        /*
         * name domain constants
         */

extern char *def_domain;
extern longword def_nameservers[ MAX_NAMESERVERS ];

extern word wathndlcbrk;
extern word watcbroke;

/* user initialization file */
extern void (*usr_init)(char *name, char *value);

extern int _survivebootp;

extern int _last_cookie;
extern longword _cookie[MAX_COOKIES];

extern int _last_nameserver;
extern char *_hostname;

#endif /* ndef _wattcp_wattcp_h */
