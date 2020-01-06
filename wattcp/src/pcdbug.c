#include <stdio.h>
#include <wattcp.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include <string.h>
#include <mem.h>
#include <icmp.h>				// R. Whitby

extern void (*_dbugxmit)( sock_type *s, in_Header *inp, void *phdr, unsigned line );
extern void (*_dbugrecv)( sock_type *s, in_Header *inp, void *phdr, unsigned line );
#define DEBUGNAME "WATTCP.DBG"

char debugname[ 128 ];
int debugheaders, debugdump, debugudp, debugtcp, debugicmp;   // R. Whitby

static char localbuf[ 128 ];
static int localhandle = 0;

void db_write( char *msg )
{
    if (localhandle)				// R. Whitby
       write( localhandle, msg, strlen(msg));
}

void db_open( void )
{
    if (!localhandle) {
	localhandle = _creat( debugname, 0 );
	if (localhandle < 0 ) {
	    outs("ERROR:unable to open debug file!\n");
	    exit(3);
	}
    }
}

void db_close( void )
{
    int i;
    if ( (i = dup( localhandle )) != -1 )
	close(i);
}

void dbug_printf( char *format, ... )
{
    va_list argptr;
    static char localspace[ 256 ];
    if ( localhandle ) {
	db_write( "\n > ");
	va_start( argptr, format );
	vsprintf( localspace, format, argptr );
	va_end( argptr );
	db_write( localspace );
	db_write( "\n" );
	db_close();
    }
}

static char *tcpflag[] =
    /*  7  ,   6 ,  5  ,   4 ,   3 ,   2 ,   1 ,   0 */
      {"??7","??6","URG","ACK","PSH","RST","SYN","FIN" };
static char *tcpmode[] = {
    "LISTEN","SYNSENT","SYNREC","ESTAB","ESTCLOSE","FINWT1","FINWT2",
	"CLOSWT","CLOSING","LASTACK","TIMEWT","CLOSEMSL","CLOSED" };

static void db_msg( char *msg, sock_type *sock, in_Header *ip, tcp_Header *tp, int line )
{
    int i,j,datalen, protocol;
    byte ch, *data;
    udp_Header *up;
    icmp_pkt *icmp;				// R. Whitby

    switch ( protocol = ip->proto ) {

	case ICMP_PROTO :
	    if (!debugicmp) return;
	    icmp = (icmp_pkt*)(tp);
	    i = in_GetHdrlenBytes(ip);
	    data = (byte *)(icmp);
	    datalen = intel16(ip->length) - i;
	    break;

	case UDP_PROTO :
	    if (!debugudp) return;
	    up = (udp_Header*)(tp);
	    datalen = intel16(up->length);
	    data = (char *)(up) + sizeof( udp_Header );
	    break;

	case TCP_PROTO :
	    if (!debugtcp) return;
	    i = (tcp_GetDataOffset(tp) << 2); /* qwords to bytes */
	    j = in_GetHdrlenBytes(ip);
	    data = (char*)(tp) + i;
	    datalen = intel16(ip->length) - j - i;
	    break;

	default :
	    return;
    }
    db_open();
    /* skip packet if no data and that was all we were looking for */
    if (!debugheaders && !datalen) return;
    db_write( msg );
    if (!sock) {
	db_write( inet_ntoa( localbuf, intel( ip->source) ));
	if (protocol != ICMP_PROTO) {		// R. Whitby
	    db_write( ":" );
	    db_write( itoa( intel16(tp->srcPort), localbuf, 10));
	}
	db_write( "   ");
	if (protocol != ICMP_PROTO) {		// R. Whitby
	    db_write( inet_ntoa( localbuf, intel( ip->destination ) ));
	    db_write( ":" );
	    db_write( itoa( intel16(tp->dstPort),  localbuf, 10));
	}
	db_write( " (NO SOCKET)");
/*
	return;
*/
    } else {
	db_write( inet_ntoa( localbuf, sock->tcp.hisaddr ));
	db_write( ":" );
	db_write( itoa( sock->tcp.hisport, localbuf, 10));
	db_write( "   0.0.0.0:");
	db_write( itoa( sock->tcp.myport,  localbuf, 10));
    }
    db_write("\n");
    if (debugheaders) {
	switch (protocol) {
	    // R. Whitby
	    case ICMP_PROTO :
		db_write("ICMP PACKET : ");
		switch ( icmp->unused.type) {
		    case 0 : /* icmp echo reply received */
			db_write("ECHO REPLY");
			break;
		    case 3 : /* destination unreachable message */
			db_write("DESTINATION UNREACHABLE");
			break;
		    case 4  : /* source quench */
			db_write("SOURCE QUENCH");
			break;
		    case 5  : /* redirect */
			db_write("REDIRECT");
			break;
		    case 8  : /* icmp echo request */
			db_write("ECHO REQUEST");
			break;
		    case 11 : /* time exceeded message */
			db_write("TIME EXCEEDED");
			break;
		    case 12 : /* parameter problem message */
			db_write("PARAMETER PROBLEM");
			break;
		    case 13 : /* timestamp message */
			db_write("TIMESTAMP REQUEST");
			break;
		    case 14 : /* timestamp reply */
			db_write("TIMESTAMP REPLY");
			break;
		    case 15 : /* info request */
			db_write("INFORMATION REQUEST");
			break;
		    case 16 : /* info reply */
			db_write("INFORMATION REPLU");
			break;
		    default : /* unknown */
			db_write("UNKNOWN TYPE");
			break;
		}
		break;

	    case UDP_PROTO :
		db_write("UDP PACKET");
		break;
	    case TCP_PROTO :
		db_write("    TCP PACKET : ");
		db_write( tcpmode[ sock->tcp.state ] );
		db_write("  (LSEQ: 0x");
		db_write(ltoa(sock->tcp.seqnum,localbuf,16));
		db_write("  LACK: 0x");
		db_write(ltoa(sock->tcp.acknum,localbuf,16));
		db_write(") NOW: ");
		db_write( ltoa( set_timeout(0), localbuf,10));
		db_write("\n    TCP FLAGS : ");
		for ( i = 0; i < 8 ; ++i ) {
		    if ( intel16(tp->flags) & ( 0x80 >> i )) {
			db_write( tcpflag[i] );
			db_write(" ");
		    }
		}

		db_write("  SEQ : 0x");
		db_write(ltoa(intel(tp->seqnum),localbuf,16));
		db_write("  ACK : 0x");
		db_write(ltoa(intel(tp->acknum),localbuf,16));
		db_write("  WINDOW : ");
		db_write(itoa(intel16(tp->window),localbuf,10));
		db_write("\n K_C : ");
		db_write(itoa(sock->tcp.karn_count,localbuf,10 ));
		db_write("  VJ_SA : ");
		db_write(itoa(sock->tcp.vj_sa ,localbuf,10 ) );
		db_write("  VJ_SD : ");
		db_write(itoa(sock->tcp.vj_sd,localbuf,10 ) );
		db_write("  RTO : ");
		db_write(itoa(sock->tcp.rto ,localbuf,10 ));
		db_write(" RTT : ");
		db_write(ltoa(sock->tcp.rtt_time ,localbuf,10 ));
		db_write(" RTTDIFF : ");
		db_write(ltoa(sock->tcp.rtt_time - set_ttimeout(0),localbuf,10 ));
		db_write(" UNHAPPY : ");
		db_write(itoa(sock->tcp.unhappy,localbuf,10 ));
		if (line) {
		    db_write(" LINE : ");
		    db_write(itoa(line, localbuf, 10 ));
		}
		break;
	}
    db_write("\n");
    }
    if (debugdump) {
	for (i = 0; i < datalen ; i+= 16 ) {
	    sprintf(localbuf,"%04x : ", i );
	    db_write( localbuf );
	    for (j = 0 ; (j < 16) && (j +i < datalen) ; ++j ) {
                sprintf( localbuf, "%02x%c", (unsigned) data[j+i], (j==7)?'-':' ');
		db_write( localbuf );
	    }
	    for ( ; j < 16 ; ++j )
		db_write("   ");

	    memset( localbuf, 0, 17 );
	    for ( j = 0; (j<16) && (j+i<datalen) ; ++j ) {
		ch = data[j+i];
		if ( !isprint(ch) ) ch = '.';
                localbuf[j] = ch;
            }
            db_write( localbuf);
            db_write("\n");
        }
    }
    db_write("\n");
    db_close();
}

static void _dbxmit( sock_type *sock, in_Header *ip, void *prot, unsigned line )
{
    db_msg("Tx:",sock,ip,prot,line);
}

static void _dbrecv( sock_type *sock, in_Header *ip, void *prot, unsigned line )
{
    db_msg("Rx:",sock,ip,prot, line);
}

static void (*otherinit)( char *name, char *value );

static void ourinit( char *name, char *value )
{
    if (!strcmp(name,"DEBUG.FILE")) {
	strncpy(debugname, value, sizeof(debugname)-2);
	debugname[sizeof(debugname) -1] = 0;
	db_open();
    } else if (!strcmp(name,"DEBUG.MODE")) {
	if (!stricmp( value, "DUMP" )) debugdump = 1;
	if (!stricmp( value, "HEADERS")) debugheaders =1;
	if (!stricmp( value, "ALL")) debugheaders = debugdump = 1;
    } else if (!strcmp(name,"DEBUG.PROTO")) {
	if (!stricmp( value, "ICMP")) debugicmp = 1;	// R. Whitby
	if (!stricmp( value, "TCP")) debugtcp = 1;
	if (!stricmp( value, "UDP")) debugudp =1;
	if (!stricmp( value, "ALL")) debugudp = debugtcp = 1;
    } else if (otherinit)
	(*otherinit)(name,value);
}

extern void (*usr_init)( char *name, char *value );

void dbug_init( void )
{
    strcpy(debugname,DEBUGNAME );
    otherinit = usr_init;
    usr_init = ourinit;
    _dbugxmit = _dbxmit;
    _dbugrecv = _dbrecv;
    debugheaders = debugdump = debugicmp = debugudp = debugtcp = 0; // R. Whitby
}
