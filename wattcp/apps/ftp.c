/*
 * BACK - turns on background processing.  Makes for faster FTP, but
 *        makes this a difficult program to run in a debugger.
 */
//#define BACK

/******************************************************************************

    FTP - file transfer protocol based program

    By Erick Engelke and Dean Roth

    This program is distributed in the hope that it will be useful,
    but without any warranty; without even the implied warranty of
    merchantability or fitness for a particular purpose.

        Erick Engelke
        erick@development.watstar.uwaterloo.ca

******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dos.h>
#include <io.h>
#include <dir.h>
#include <mem.h>
#include <bios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <process.h>
#include <conio.h>


#include "tcp.h"
char *version = "Version 0.70";

#define FTPCTL_PORT 21

#define DATA_BINARY 0
#define DATA_ASCII  1

word ftpctl_port = FTPCTL_PORT;
int connected = 0;
tcp_Socket ftpctl, ftpdata;
char responsebuf[ 1024 ];
char buffer[ 1024 ];
char linebuffer[ 1024 ];

#define BIGBUFLEN   32256
#define BIGBUF2LEN  32256
#define MAXSTRING   127
char *bigbuf, *bigbuf2;


extern unsigned _stklen = (16*1024);



int outstanding;
extern int debug_on;
int quiet = 0;

FILE *infile = (FILE *)NULL;

int commandbox = 1;
int databox = 2;
int message = 3;
int iscolour = 0;


void lprintf( int code, char *format, ... )
{
    va_list argptr;
    static char tempbuf[ 1024 ];
    char *p, *q;
    va_start( argptr, format );
    vsprintf( tempbuf, format, argptr );
    va_end( argptr );

    q = tempbuf;
    if ( iscolour ) {
        textcolor( WHITE );
    }
    while ( q && *q ) {
        p = strchr( q, '\n' );
        if ( p ) *p++ = 0;

        cputs( q );
        if ( p ) cputs("\r\n");
        q = p;
    }
}

void lputs( char *text )
{
    if ( iscolour ) textcolor( LIGHTGRAY );
    cputs( text );
    cputs("\r\n");
    if ( iscolour ) textcolor( WHITE );
}

void eputs( char *text )
{
    if ( iscolour ) textcolor( MAGENTA );
    cputs( text );
    cputs("\r\n");
    if ( iscolour ) textcolor( WHITE );
}

void dosmessage( int x, char *msg )
{
    if ( x )
        lprintf( databox, "Unable to %s, %s\n",
            msg, sys_errlist[ errno ]);
    else
        lprintf( databox, "%s successful\n", msg );
}


char unixbuf[ 256 ];
char dosbuf[  100 ];
/*
 * tounix - if this is to be found from dos name, stip path, etc
 */
char *tounix( char *filename )
{
    char *p;
    int oldlen;

    oldlen = strlen( filename );
    strcpy( unixbuf, filename );
    if ( (p = strchr( filename = unixbuf, ':' )) != NULL ) filename = p + 1;
    while (( p = strchr( filename, '\\' )) != NULL ) filename = p + 1;
    if ( strlen( filename ) != oldlen )
        lprintf( commandbox, "Truncating remote name to: %s\n", filename );
    return( filename );
}
char *todos( char *filename )
{
    char *p, *q, *lastq = NULL;
    int i = 1;
    int comment = 0;    /* shall we comment ? */

    if ( (p = strchr( filename, ':' )) != NULL ) filename = p + 1;
    while (( p = strchr( filename, '\\' )) != NULL ) filename = p + 1;
    strcpy( dosbuf, filename );
//    strupr( dosbuf );
    if ( (p = strchr( dosbuf, '.' )) != NULL ) {
        p+= 2;          /* keep dot */
        lastq = q = p;  /* point to second char in case last extension */
        while ( (q = strchr( q, '.')) != NULL ) { /* find next extension */
            comment = 1;
            if ( *(++q) == '.' ) continue;
            *p++ = *q;
            lastq = ++q;
            i++;
        }
        while ( i < 3 ) {
            if ( *lastq == '.' ) continue;
            if ( (*p++ = *lastq++) == 0 ) break;
            i++;
        }
        *p = 0; /* terminate */
        if ( *lastq ) comment = 1;
    }

    if ( comment )
        lprintf( commandbox,"Using truncated local filename %s\n", dosbuf );
    return( dosbuf );
}

/*
 * kbuffers is our history of commands in a circular buffer
 * kindex is a pointer to the current
 */
#define KMAXCHAR 127
#define KMAXQ    15     /* must be 2^n - 1 */
typedef struct kedit {
    byte    inuse;
    byte    data[ KMAXCHAR ];
};
static struct kedit kbuffers[ KMAXQ ];
static int kindex = 0;

void editline( char *curstring, int *curpos, int newcode, int edit )
{
    static int initialized = 0;
    char *olds;
    int i;
    int oldlen;

    if ( !initialized ) {
#if 0
//        kbuffers = calloc( sizeof( kedit ) * KMAXQ );
//        if ( kbuffers == NULL ) {
//            puts("Insufficient memory buffers");
//            exit( 3 );
//        }
#endif
        memset( (char *)kbuffers, 0, sizeof( kbuffers ));
        kbuffers[ 0 ].inuse = 1;    /* must have one active one */
        initialized = 1;
    }

    if ( !edit ) {
        strncpy( kbuffers[ kindex ].data, curstring, KMAXCHAR - 1 );
        kbuffers[ kindex ].inuse = 1;
        kindex = ( kindex + 1 ) & KMAXQ;
    } else {
        switch ( newcode ) {
            case 71 : /* home */
                        /* save old line */
                        *curpos = 0;
                        break;
            case 72 : /* up */
            case 80 : /* down */
            case 61 : /* F3 */
                        do {
                            kindex = ( kindex +
                                ( ( newcode == 80 ) ? 1 : -1 )) & KMAXQ;
                        } while ( !kbuffers[ kindex ].inuse );

                        oldlen = strlen( olds = kbuffers[kindex].data );
                        strcpy( curstring, olds );
                        if ( *curpos > oldlen ) *curpos = oldlen;
                        break;
            case 79 : /* end */
                        *curpos = strlen( curstring ) - 1;
                        break;
            case 75 : /* left */
                        if ( (*curpos -= 1) < 0 ) *curpos = 0;
                        break;
            case 77 : /* right */
                        oldlen = strlen( curstring );
                        if ( (*curpos += 1) > oldlen )
                            *curpos = oldlen;
                        break;
            default : break;
        }
    }
}

/* ******************* */
/* FUNCTION PROTOTYPES */
/* ******************* */
static char Fgetchar( char * );


char *keygets( char *s, int echo )
{
    int len = 0;
    unsigned keycode, redraw;
    int curpos = 0, startpos, startline;
    char ch;

    startpos = wherex();
    startline = wherey();
    for ( curpos = len = 0; len < 512 ; ) {
        tcp_tick( NULL );
        if (kbhit()) {
            redraw = 0;
            keycode = bioskey( 0 );
            ch = ( char )( keycode & 0x7f );
            switch ( ch ) {
                case 27  : // clear the line
                        s[ len = 0 ] = 0;
                        if ( echo ) redraw = 1;
                        break;
                case 13  :
                case 10  :
                        /* remember this line for a future time */
                        if ( echo ) editline( s, &len, 0, 0 );
                        lprintf( commandbox, "\n");
                        s[len] = 0;
                        return( s );
                case 0  :
                        if ( echo ) {
                            editline( s, &curpos, keycode >> 8, 1 );
                            redraw = 1;
                        }
                        break;
                case 8 :
                case 127 :
                        if ( echo ) {
                            if ( curpos > 0 ) {
                                redraw = 1;
                                s[ --curpos ] = ' ';
                                if ( curpos == (len - 2 ))
                                    s[ --len ] = 0;
                            }
                            break;
                        }
                        // if in password mode, fall through to next
                        // section
                default :
                        if ( echo && !(isprint(ch))) break;
                        if ( curpos > MAXSTRING ) { 
                            printf("\7");   /* string too long */
                            break;
                        }
                        s[curpos++] = ch;
                        if ( curpos >= len ) len = curpos + 1;
                        s[len] = 0;
                        if (echo) lprintf( commandbox, "%c", ch );
                        else lprintf( commandbox, "\260" );
                        break;
            }
        }
        if ( redraw ) {
            if ( curpos > len ) curpos = len;
            gotoxy( startpos, startline );
            clreol();
            len = strlen( s );
            if ( echo ) lprintf( commandbox, "%s", s );
            else {
                for ( ch = 1; ch < len ; ++ch )
                    lprintf( commandbox,"\260");
            }
            gotoxy( startpos + curpos, startline );
            redraw = 0;
        }
    }
    return( NULL );
}

char *linegets( char *prompt, char *s, int echo )
{
    char *p;
    do {
        if ( infile ) {
            if ( !fgets( s, 255, infile )) {
                if ( !quiet )
                    lprintf( commandbox,"Script file done, returning to command mode\n");
                infile = (FILE*) NULL;
                continue;
            } else {
                if ((p = strchr( s, '\n'))!= NULL) *p = 0;
                if (( p = strchr( s, '\r'))!=NULL) *p = 0;
                if ( !quiet && (*s != '@') && echo )
                    lprintf( commandbox,"%s%s\n", prompt, s );
                if ( *s == '@' ) movmem( s+1, s, strlen( s ));
            }
        } else {
            lprintf( commandbox,"%s", prompt );
            s = keygets( s, echo );
        }
        break;
    } while ( 1 );
    return( s );
}

void ldir( char *p )
{
    struct ffblk ffblk;
    int status;
    char *ptr;
    unsigned min, hour, day, mon, year;
    int lines;

    if ( !quiet ) lprintf( commandbox, "DIR %s:\n", p );
    if ( (status = findfirst( p, &ffblk, FA_DIREC )) != NULL ) {
        lprintf( commandbox,"  ERROR: %s\n", sys_errlist[ errno ] );
        return;
    }

    lines = 0;

    while (status == 0) {

        if (lines >= 20)
        {
            lprintf(databox,"--- more ---  [ <SPACE> <RETURN>  <Q>uit ]");
            switch (Fgetchar( " \rQ" ))
            {
            case    ' ' :
            case    '\r':
                lprintf(databox,"\r%-79.79s\r", " ");
                lines = 0;
                break;

            case    'Q' :
                lprintf(databox,"\r%-79.79s\r", " ");
                goto EXIT;
            }
        }


        min = (ffblk.ff_ftime >> 5) & 63;
        hour = (ffblk.ff_ftime >> 11) % 12;
        day = ffblk.ff_fdate & 31;
        mon = (ffblk.ff_fdate >> 5) & 15;
        year = ( ffblk.ff_fdate >> 9 ) + 1980;

        if (( ptr = strchr( ffblk.ff_name, '.' )) != NULL ) *ptr++ = 0;
        else ptr = "";

        if (ffblk.ff_attrib & FA_DIREC)
            lprintf(databox," %-8s.%-3s %8s ", ffblk.ff_name, ptr, "<DIR>");
        else
            lprintf(databox," %-8s %-3s %8lu ", ffblk.ff_name, ptr, ffblk.ff_fsize );

        lprintf(databox," %2u-%02u-%4u   %2u:%02u%c\n",
            mon, day, year % 100,
            ( hour > 12 ) ? hour - 12 : hour , min,
            ( hour > 11 ) ? 'p' : 'a');

        lines++;

        status = findnext( &ffblk );
    }

EXIT:

    return;
}

int getresponse( tcp_Socket *s, void (*fn)(char *) )
{
    int code, i;
    int status;

    outstanding = 0;
    if ( !connected )
        return( 600 );
    sock_mode( s, TCP_MODE_ASCII );
    sock_wait_input( s, sock_delay, (sockfunct_t)NULL, &status );
    sock_gets( s, responsebuf, sizeof(responsebuf));
    code = atoi( responsebuf);
    do {
        (*fn)( responsebuf);

        if ( atoi(responsebuf) == code ) {
            for ( i = 0; i < 5; ++i ) {
                if ( isdigit( responsebuf[i] )) continue;
                if ( responsebuf[i] == ' ' ) {
                    sock_mode( s, TCP_MODE_BINARY );
                    return( code );
                }
                if ( responsebuf[i] == '-' ) break;
            }
        }
        sock_wait_input( s, sock_delay, NULL, &status );
        sock_gets( s, responsebuf, sizeof(responsebuf));
    } while(1);
sock_err:
    switch (status) {
        case 1 : /* foreign host closed */
                 break;
        case -1: /* timeout */
                 lprintf( commandbox,"ERROR: %s\n", sockerr(s));
                 break;
    }
    connected = 0;
    return( 221 );
}

/*
 * sendport - negotiate port, return 0 on success
 */
int sendport( tcp_Socket *s, longword hisip )
{
    longword ip;
    byte *p1, *p2;
    time_t now;
    static int dataport = 0;

    if ( dataport == 0 ) {
        time( &now );
        dataport = (word)(now >> 16) + (word)(now);
    }

    tcp_listen( &ftpdata, ++dataport, hisip, 0, NULL, 0 );
    sock_sturdy( &ftpdata, 100 );

    p1 = (byte *)&dataport;
    p2 = (byte *)&ip;
    ip = gethostid();
    sock_printf( s, "PORT %hu,%hu,%hu,%hu,%hu,%hu\r\n",
        p2[3],p2[2],p2[1],p2[0],p1[1],p1[0] );
    if (getresponse(s, eputs) > 299 ) {
        sock_close( &ftpdata );
        return( -1 );
    }
    return( 0 );
}

void sendcommand( tcp_Socket *s, char *cmd, char *param )
{
    if ( ! connected )
        lprintf( commandbox,"You are not connected to anything right now\n");
    else {
        sock_printf( s, (param) ? "%s %s\r\n" : "%s\r\n", cmd, param );
        outstanding = 1;
    }
}

int remotecbrk(void *s)
{
    char ch;
    if ( watcbroke ) {
        /* we received a control break - first reset the flag */
        watcbroke = 0;
        /* attempt to do something about it */
        lprintf( commandbox,"\rUser pressed control break.  Do you wish to disconnect [Y/N] ");
        do {
            ch = toupper( getch() );
            if ( ch == 'Y' ) {
                lprintf( commandbox,"Y\nBreaking connection\n");
                sock_abort( s );
                break;
            }
            if ( ch == 'N' ) {
                lprintf( commandbox,"N\nIgnoring control break\n");
                break;
            }
        } while ( 1 );
    }
    return( 0 );
}


typedef struct {
    unsigned datalength;
    byte    *data;
} bfcollection;

#define MAXBUFS 15
bfcollection bufcollection[ MAXBUFS ];
int lastbuf;

int write2file( int handle, byte *data, unsigned length )
{
    bfcollection *b;
    int i, status = 1;
    byte *p;

    /* zero length indicates we must flush all buffers */
    if ( length ) {
        if ( lastbuf != MAXBUFS ) {
            /* try to allocate some memory for this */
            if ((p = malloc( length )) != NULL ) {
                /* send it to a buffer for now */
                b = bufcollection + lastbuf++;

                b->datalength = length;
                b->data = p;
                memcpy( p, data, length );
                return( 0 );            /* success */
            }
        }
    }


    /* things just don't fit anymore - write out existing buffers */
    for ( i = 0, b = bufcollection ; i < lastbuf ; ++i, ++b ) {

        if ( (p = b->data) == NULL ) continue;

        if ( (b->datalength != 0) && status && handle ) {
#ifdef BACK
            backgroundon();
#endif

            status = write( handle, p, b->datalength );
if ( status < b->datalength )
lprintf( commandbox,"WARNING: only wrote %u / %u bytes\n\n", status, b->datalength );
            status = status == b->datalength;

#ifdef BACK
        backgroundoff();
#endif BACK
        }
        free( p );          /* don't need memory any more */
        b->datalength = 0;     /* clean these out */
        b->data = NULL;
    }

    /* and write out passed data */
    if ( status && data && length && handle )
#ifdef BACK
            backgroundon();
#endif
        status = write( handle, data, length ) == length;

#ifdef BACK
        backgroundoff();
#endif BACK

    return( status == 0 );
}

/*
 * remoteop
 *      cmd =  "RETR"
 *      data = "RemoteFileName"
 *      file = "LOCALNAM.FIL"
 */
int remoteop( tcp_Socket *s, longword hisip, char *cmd, char *data, char *file,
            int get, int ascii, int screen, void (*upcall)(), int hash )
{
    struct stat statbuf;
    int f = -1, len, status, writestatus = 0;
    longword totallen = 0, lasthash = 0;
    longword starttime, endtime, totaltime, speed, skipping = 0;
    long now, last = 0;
    word offlen = 0;


    memset( bufcollection, 0, sizeof( bufcollection ));
    lastbuf = 0;


    if ( sendport( s, hisip ))
        return( -1 );

    if ( screen == 0 ) {
        if ( get )
        {
            if ( access( file, 0 ) == 0 )
            {
                lprintf( commandbox,"File '%s' already exists, do you wish to replace it? (Y/N) ",file);
                if ( Fgetchar( "YN" ) == 'N' )
                {
                    lprintf( commandbox, "N\nSkipping...\n");
                    return( 500 );
                }
                else
                {
                    int status;
                    lprintf( commandbox, "Y\nErasing file");

                    status = unlink( file );/**/

                    if (status)
                    {
                        lprintf( commandbox,"...failed\n");
                        return( 500 );
                    }
                    lprintf( commandbox,"\n");
                }
            }
        }
        f = open( file,
                ((get) ? (O_CREAT|O_TRUNC) : 0) | O_RDWR |
                ((ascii)? O_TEXT : O_BINARY),
                S_IWRITE|S_IREAD);
        if ( f == -1 ) {
            sock_close( &ftpdata );
            lprintf( commandbox,"Unable to open file: %s\n", sys_errlist[ errno ]);

            return( -1 );
        }

    }


    if (get && screen)
    {
        /* ************************************** */
        /* THIS MUST BE DONE BEFORE sendcommand() */
        /* ************************************** */
        sock_setbuf( &ftpdata, bigbuf, BIGBUFLEN );/**/
        sock_mode( &ftpdata, TCP_MODE_ASCII );
    }


    sendcommand( s, cmd, data );

    if ( getresponse( s, eputs ) >= 200 ) {
        sock_close( &ftpdata );
        if (f != -1) close( f );
        return( -1 );
    }

    /* openning connection */

    sock_wait_established(&ftpdata, sock_delay, NULL, &status);


    if ( get )
    {
        int lines = 0;

        if ( screen )
        {
#define SM_PAUSE  1
#define SM_QUIT   2
#define SM_NOSTOP 3 /* do not pause - continuous */
            int mode = SM_PAUSE;

            do {
                /* ********************************* */
                /* THIS GETS US OUT OF INFINITE LOOP */
                /* ********************************* */
                sock_wait_input( &ftpdata, sock_delay, NULL, &status );

                sock_gets( &ftpdata, bigbuf2, BIGBUF2LEN );
                if (!totallen) starttime = set_ttimeout(0);
                totallen += (longword)strlen( bigbuf2 );
                endtime = set_ttimeout( 0 );

                /* either do malloc or dump to screen */
                if ( upcall )
                    (*upcall)( bigbuf2 );
                else
                {
                    if (mode != SM_QUIT)
                        lputs( bigbuf2 );

                    if (mode == SM_PAUSE)
                    {
                        lines++;
                        if (lines >= 20)
                        {
                            lprintf(databox,"--- more ---  [ <SPACE> <CR>  <C>ontinuous  <Q>uit ]");
                            switch (Fgetchar( " \rCQ" ))
                            {
                            case    ' ' :
                            case    '\r':
                                    lprintf(databox,"\r%-79.79s\r", " ");
                                    lines = 0;
                                    break;

                            case    'Q' :
                                lprintf(databox,"\r%-79.79s\r(flushing data stream...)\n", " ");
                                mode = SM_QUIT;
                                break;
                            case    'C' :
                                lprintf(databox,"\r%-79.79s\r", " ");
                                mode = SM_NOSTOP;
                                break;
                            }
                        }
                    }
                }

            } while ( 1 );
        }
        else
        {
            /* ************* */
            /* GET TO A FILE */
            /* ************* */

            do {
                sock_wait_input( &ftpdata, sock_delay, remotecbrk, &status );
                len = sock_fastread( &ftpdata, bigbuf2 + offlen, BIGBUF2LEN - offlen );
                offlen += len;
                if (!totallen) starttime = set_ttimeout(0);
                totallen += (longword)len;
                endtime = set_ttimeout( 0 );
                if ( hash ) {
                    lasthash += len;
                    while ( lasthash > hash ) {
                        /* printf("#");/**/
                        lprintf(databox,"\rGot %8ld bytes\r", (long)totallen);
                        lasthash -= hash;
                    }
                }
                if (  offlen > BIGBUF2LEN - 512 ) {
                    if ( skipping )
                        offlen = 0;
                    else {
/*
                        printf("writing %u bytes\n", offlen );
*/
                        writestatus = write2file( f, bigbuf2, offlen );
                        offlen = 0;
                        if ( writestatus != 0 ) {
                            if ( writestatus == -1 ) {
                                write2file( 0, NULL, 0 );
                                dosmessage( 1, "get file");
                                close( f );
                                sock_close( &ftpdata );
                                return(-1);
                            } else {
                                lprintf( commandbox,"disk full...\n");
                                sendcommand( s, "ABOR", NULL);
                                skipping = 1;
                            }
                        }
                    }
                }
            } while( 1 );
        }
    } else {

        /* ******************* */
        /* GET INFO ABOUT FILE */
        /* ******************* */
        fstat( f, &statbuf );

        /* put to remote end */
        do {
            if ( watcbroke ) {
                /* got a control break - let's kill this baby */
                lprintf(commandbox,"\rUser pressed control break, aborting send\n");
                close( f );
                sock_close( &ftpdata );
                sock_wait_closed( &ftpdata, sock_delay, NULL, &status );
            }

            if ( hash ) {

//                lasthash += len;
//                while ( lasthash > hash ) {
//                  printf("#");
//                  lasthash -= hash;
//                }
                time( &now );
                if ( now != last ) {
                    last = now;
                    lprintf(databox,"\rPut %8ld bytes of %ld (%d%%)\r",
                        (long)totallen - (long)sock_tbused( &ftpdata),
                        (long)statbuf.st_size,
                        (int)(((double)totallen/(double)statbuf.st_size) * 100.0));
                }
            }

            if ( sock_tbused( &ftpdata ) == 0 ) {
#ifdef BACK
                backgroundon();
#endif
                len = read( f, bigbuf, BIGBUFLEN );
#ifdef BACK
                backgroundoff();
#endif
                if (!totallen) starttime = set_ttimeout(0);
                totallen += (longword) len;
                endtime = set_ttimeout( 0 );
                if ( len == -1 ) {
                    dosmessage( 1, "write file");
                    close( f );
                    sock_close( &ftpdata );
                    return(-1);
                }
                if ( len == 0 ) {
                    sock_close( &ftpdata );
                    sock_wait_closed( &ftpdata, sock_delay, NULL, &status );
                }
                sock_enqueue( &ftpdata, bigbuf, len );
            }
            sock_tick( &ftpdata, &status );
        } while ( 1 );
    }

EXIT:
    sock_close( &ftpdata );

    // only if it is a file and we are getting it and we haven't had errors
    if (( screen == 0 ) && ( get != 0 ) && ( writestatus == 0 )) {
        if ( writestatus = write2file( f, bigbuf2, offlen ) )
            lprintf( commandbox,"disk full...\n");
        else if ( writestatus = write2file( f, NULL, 0 ) )
            lprintf( commandbox,"disk full...\n");
    }
    if ( f != -1 ) close( f );

    status = (getresponse( s, eputs ) < 300 ) ? 0 : -1;

    totaltime = ((endtime - starttime) * 6400L) / 1165L;
    speed = ( 39 * totallen / 4 ) / (totaltime + 1);
    if (hash) lprintf(databox,"\n");
    lprintf(databox, "Transfer of %0lu Kbytes (%ld bytes) in %0lu.%02lu seconds ( %0lu.%02lu Kbytes/s ).\n",
        totallen / 1024L,
        totallen,
        totaltime / 100L, totaltime % 100L,
        speed / 100L,
        speed % 100L );
    return( status );


sock_err:
    if (status == -1) /**/
        lprintf( commandbox,"WATTCP socket error: %d\n", status);/**/

    goto EXIT;
}



text2dos( char *p )
{
    while (*p ) {
        if ( !isprint(*p) ) {
            *p = 0;
            break;
        }
        ++p;
    }
}

/* ******************************************* */
/* BUILD LIST OF FILE NAMES TO BE USED BY MGET */
/* ******************************************* */
char **dumplist;
int dumplast, dumpcur;
void dumper( char *s )
{
    char *p;

/*printf("dumper: s = [%s]\n", s);/**/

    if ( dumpcur < dumplast ) {

        if (s[0] == ' ')        /* blank line */
        {
            /* printf("SKIP: (space char) [%s]\n", s);/**/
            return;
        }

        if (!s[0])              /* NULL string */
        {
            /* printf("SKIP: (NULL string) [%s]\n", s);/**/
            return;
        }

        if (strchr(s, ':'))     /* directory? */
        {
            /* printf("SKIP: (dir?) [%s]\n", s);/**/
            return;
        }

        if (( p = (dumplist[ dumpcur ] = malloc( strlen( s ) + 1))) != NULL ) {
            strcpy( p, s );
            text2dos( p );
            dumpcur++;
        } else {
            lprintf( commandbox,"out of memory... ignoring %s\n", s );
        }
    }
}

static char Fgetchar( As_allowed )

    char *As_allowed;
{
    char *cptr;
    char  ch;


    while ( 1 )
    {
#ifdef BACK
    backgroundon();
#endif
        ch = (char) toupper(getch());
#ifdef BACK
    backgroundoff();
#endif

        cptr = As_allowed;

        while (*cptr)
        {
            if (*cptr == ch)
                goto EXIT;

            cptr++;
        }
    }

EXIT:

    return(ch);
}



/* ************************************************* */
/* GET LIST OF LOCAL FILES MATCHING PATTERN FOR MPUT */
/* ************************************************* */
static char *Ffilelist( char *As_pattern, int Ai_state )
{
    static struct ffblk ffblk;
    int status;
    char *ptr;

    if (Ai_state == 1)
        status = findfirst( As_pattern, &ffblk, 0 );
    else
        status = findnext( &ffblk );

    if ( status )
    {
        /* printf("  ERROR: %s\n", sys_errlist[ errno ] ); /**/
        return((char *)NULL);
    }
    else {
        strlwr( ffblk.ff_name );
        return (strlwr(ffblk.ff_name));
    }
}





static int Fmget(  tcp_Socket *s, longword hisip, char *data,
        int ascii, int interactive, int hash )
{
    char *p, ch;
    int i;

    dumplast = 2000;    /* max no. files in list for an "mget" */
    dumpcur = 0;
    dumplist = calloc( dumplast, sizeof( char * ));
    if (dumplist == NULL ) {
        lprintf( commandbox,"Insufficient memory to start multifile operation\n");
        return( 500 );
    }

    /* ***************** */
    /* GET LIST OF FILES */
    /* ***************** */
#if 1
    if (ascii != DATA_ASCII ) {
        sendcommand(s,"TYPE","A" );
        if ( getresponse( s, eputs ) != 200 )
            goto EXIT;
    }


    remoteop( s, hisip, "NLST", data, NULL, 1, 1, 1, dumper, 0);


    if (ascii != DATA_ASCII ) {
        sendcommand( s,"TYPE","I");
        if (getresponse( s, eputs ) != 200 ) {
            /* datamode = DATA_ASCII; /**/
            lprintf( commandbox,"WARNING: Host left connection in ASCII mode");
            goto EXIT;
        }
    }
#endif

    lprintf( databox, "There are %u matching files\n", dumpcur );

    for ( i = 0; i < dumpcur ; ++i )
    {
        p = dumplist[i];

        if (interactive)
        {

            lprintf( commandbox, "GET [%s] (Yes,No,Quit,Rest) > ", p );

            switch (Fgetchar( "YNQR" ))
            {
            case    'N':
                lprintf( commandbox,"\n");
                continue;

            case    'Y':
                lprintf( commandbox,"\n");
                break;

            case    'Q':
                lprintf( commandbox,"\n");
                goto EXIT;
                break;
            case    'R':
                lprintf(commandbox,"\nWill GET rest without prompting\n");
                interactive = 0;
                break;
            }
        }

        remoteop( s, hisip, "RETR", p, todos(p), 1, ascii, 0, NULL, hash);
    }


EXIT:

    /* clean up memory */
    for ( i = 0; i < dumpcur ; ++i ) {
        if (p = dumplist[i])
            free( p );
    }

    free( dumplist );
}


static int Fmput( tcp_Socket *s, longword hisip, char *data,
        int ascii, int interactive, int hash )
{
    char *p, ch;
    int i;

    p = (char *)NULL;

    while (p = Ffilelist( data, p == (char *)NULL ? 1 : 0 ))
    {
        if (interactive)
        {
            lprintf( commandbox,"PUT [%s] (Yes,No,Quit,Rest) > ", p );

            switch (Fgetchar( "YNQR" ))
            {
            case    'N':
                lprintf( commandbox,"\n");
                continue;

            case    'Y':
                lprintf( commandbox,"\n");
                break;

            case    'Q':
                lprintf( commandbox,"\n");
                goto EXIT;
                break;
            case    'R':
                lprintf( commandbox,"\nWill PUT rest without prompting\n");
                interactive = 0;
                break;
            }
        }

        /* ********* */
        /* SEND FILE */
        /* ********* */
        remoteop( s, hisip, "STOR", tounix(p), p, 0, ascii, 0, NULL, hash);
    }

EXIT:

    return(0);

}   /* END Fmput() */



void wait( char *string )
{
    int a,b;
    time_t now, when, diff;
    struct time t;
    struct date d;

    getdate( &d );

    if ( sscanf( string, "%u:%u", &a, &b ) != 2 ) {
        lprintf( commandbox,"wait hh:mm with hours in 24 hour format\n");
        return;
    }

    t.ti_hour = a;
    t.ti_min = b;

    when = dostounix( &d, &t );
    time( &now );
    if ( now > when ) when += 24L * 60L * 60L;

    unixtodos( when, &d, &t );

    lprintf( commandbox,"Waiting until %u:%02u\n", t.ti_hour, t.ti_min );

    watcbroke = 0;
    do {
        if ( watcbroke ) {
            watcbroke = 0;
            lprintf( commandbox,"wait broken by control-break\n");
            break;
        }
        time( &now );
        diff = when - now;
        a = diff % 60;
        diff /= 60;
        lprintf( commandbox, "    %lu:%02lu:%02u left\r", diff / 60L, diff % 60L, a );
    } while ( now < when );
    lprintf( commandbox,"done waiting         \n");
}

dorun( char *fname )
{
    if ( infile = fopen( fname, "rt"))
        lprintf( commandbox,"running commands from: %s\n", fname );
    else
        lprintf( commandbox,"ERROR: unable to read '%s'\n", fname );
}
typedef struct {
     char *cmd;
     int   safety;  /* if 1, does not need to be connected to work */
     int   cmdnum;
     char *help;
} cmdtype;

enum {
     STARTLIST,
     DIR,    LS,    GET,   PUT,    DEL,    CHDIR,     RMDIR,     MKDIR,
     MPUT,   MGET,  BGET,  BPUT,
     LDIR,                         LDEL,   LCHDIR,    LRMDIR,    LMKDIR,
     ASCII, BIN,    EBC,   MODE,   OPEN,   CLOSE,     USER,      PASS,
     QUIT,  HELP,   HASH,  INT,    NONINT, STATUS,    SHELL,     DEBUG,
     WAIT,  RUN,    PWD,   LPWD,   TYPE,   RHELP,     ECHO,      QUIET,
     QUOTE,
     ENDLIST };

cmdtype cmds[] = {
       { "STATUS",  1, STATUS,"show current status"               } ,
       { "OPEN",    1, OPEN,  "connect to host"                   } ,
       { "CLOSE",   0, CLOSE, "disconnect from host"              } ,
       { "USER",    0, USER,  "new user name"                     } ,
       { "PASS",    0, PASS,  "enter password"                    } ,
       { "DIR",     0, DIR,   "list remote directory"             } ,
       { "LS",      0, LS,    "list remote files"                 } ,
       { "GET",     0, GET,   "get a remote file"                 } ,
       { "REC",     0, GET,   "get a remote file"                 } ,
//       { "BGET",    0, BGET,  "get a binary file"                 } ,
//       { "BPUT",    0, BPUT,  "put a binary file"                 } ,
       { "PUT",     0, PUT,   "put a local file"                  } ,
       { "SEND",    0, PUT,   "put a local file"                  } ,
       { "MGET",    0, MGET,  "get several files"                 } ,
       { "MPUT",    0, MPUT,  "put multiple files"                } ,
       { "DEL",     0, DEL,   "delete a remote file"              } ,
       { "RM",      0, DEL,   "delete a remote file"              } ,
       { "CD",      0, CHDIR, "change remote directory"           } ,
       { "CHDIR",   0, CHDIR, "change remote directory"           } ,
       { "RD",      0, RMDIR, "remove remote directory"           } ,
       { "RMDIR",   0, RMDIR, "remove remote directory"           } ,
       { "MD",      0, MKDIR, "make remote directory"             } ,
       { "MKDIR",   0, MKDIR, "make remote directory"             } ,
       { "ASCII",   0, ASCII, "set ASCII mode"                    } ,
       { "BINARY",  0, BIN,   "set binary mode"                   } ,
       { "EBCDIC",  0, EBC,   "set EBCDIC mode"                   } ,
       { "MODE",    0, MODE,  "advanced set mode"                 } ,
       { "LDIR",    1, LDIR,  "list local directory"              } ,
       { "LLS",     1, LDIR,  "list local directory"              } ,
       { "LDEL",    1, LDEL,  "delete local file"                 } ,
       { "LCD",     1, LCHDIR,"change local directory"            } ,
       { "LCHDIR",  1, LCHDIR,"change local directory"            } ,
       { "LRD",     1, LRMDIR,"remove local directory"            } ,
       { "LRMDIR",  1, LRMDIR,"remove local directory"            } ,
       { "LMD",     1, LMKDIR,"make local directory"              } ,
       { "LMKDIR",  1, LMKDIR,"make local directory"              } ,
       { "LPWD",    1, LPWD  ,"print local directory"             } ,
       { "TYPE",    0, TYPE  ,"display remote *text* file"        } ,
       { "QUIT",    1, QUIT,  "quit program"                      } ,
       { "BYE",     1, QUIT,  "quit program"                      } ,
       { "PWD",     0, PWD ,  "print current directory"           } ,
       { "HELP",    1, HELP,  "list instructions"                 } ,
       { "?",       1, HELP,  "list instructions"                 } ,
       { "REMOTEHELP",    0, RHELP,  "list cmds remote supports"  } ,
       { "HASH",    1, HASH,  "toggle/set hash markings"          } ,
       { "PROMPT",  1, INT,   "(ON/OFF) set prompt mode"          } ,
       { "WAIT",    1, WAIT,  "wait until hh:mm"                  } ,
       { "RUN",     1, RUN,   "run a script file"                 } ,
       { "ECHO",    1, ECHO,  "echo some text to screen"          } ,
       { "QUIET",   1, QUIET, "ON | OFF for quiet scripts"        } ,
       { "QUOTE",   1, QUOTE, "send arbitrary remote command"     } ,
       { "!",       1, SHELL, "DOS shell"                         } ,
#ifdef DEBUG
       { "DEBUG",   1, DEBUG, "toggle debug mode"                 } ,
#endif DEBUG
       { "x",       1, ENDLIST, NULL } };

int text2cmd( char *line, char *linebuf, char **pp, char **qp )
{
    char cmdnum;
    int temp, matches;
    cmdtype *closeto, *cur;
    char *command, *parm1, *parm2;

    *qp = *pp = NULL;
    command = strtok(line, " ");
    if (!command)
        return( STARTLIST );     /* nothing there */

    parm1 = strtok((char *)NULL, " ");

    if (parm1) {
        parm2 = strtok((char *)NULL, " ");
        strcpy( linebuf, parm1 );
    } else {
        parm2 = (char *)NULL;
        *linebuf = (char) 0;
    }

    cmdnum = matches = 0;
    closeto = NULL;

    do {
        cur = &cmds[cmdnum];
        if ( * cur->cmd > 'Z' ) break;

        if ( strnicmp( command, cur->cmd, strlen( cur->cmd )) == 0) {
            *pp = parm1;
            *qp = parm2;
            if ( !(cur->safety || connected) ) {
                lprintf( commandbox,"You cannot use %s until you are connected to another system\n",
                    cur->cmd );
                lprintf( commandbox, "Use OPEN machinename to connect to a machine, or QUIT to exit\n");
                return( ENDLIST );
            }
            return( cur->cmdnum );
        }
        temp = strnicmp( command, cur->cmd, strlen( command ));
        if ( temp == 0 ) {
            if (matches == 0 ) {
                closeto = cur;
                ++matches;
            } else {
                if ( matches == 1 ) lprintf( commandbox,"Ambiguous command, could be %s",closeto->cmd);
                lprintf( commandbox,", %s", cur->cmd );
                closeto = NULL;
                ++ matches;
            }
        }
        ++cmdnum;
    } while(1);
    if ( closeto != NULL ) {
        *pp = parm1;
        *qp = parm2;
        lprintf( commandbox, ">>>> %s %s %s\n", closeto->cmd, parm1?parm1:" ", parm2?parm2:" " );
        return( closeto->cmdnum );
    }
    if (matches != 0) lprintf( commandbox, "\n");
    return( ENDLIST );
}


void doshell(void)
{
    static int have_done = 0;
    char *newstring, *oldstring;
    char *cptr;

    if (!(cptr = getenv("COMSPEC")))
        return;

    if (!have_done)
    {
//        newstring = calloc( 512, 1 );
//        oldstring = getenv( "PROMPT" );
//        sprintf( newstring, "PROMPT=Enter EXIT to return to FTP$_%s", oldstring );

//        if (putenv( newstring )) lprintf( commandbox,"putenv() failed\n");
//        free( newstring );/**/
//        have_done = 1;
    }


#ifdef BACK
    backgroundon();
#endif

    gotoxy( 1, 25 );
    spawnl(P_WAIT, cptr, cptr, (char *)NULL);


#ifdef BACK
    backgroundoff();
#endif
}


dohelp( char *cmd )
{
    cmdtype *p[2], *q[2];
    char buffer[ 512 ];
    int addspace, i, found;
    int addnote = 0;    /* set to 1 if we have to add the textual note */

    for ( i = 0 ; cmds[i].cmdnum != ENDLIST ; ++i );
    p[0] = cmds;
    p[1] = cmds + ++i/ 2;

    if ( cmd != NULL ) {
        found = 0;
        i = toupper( *cmd );
        while ( p[0]->cmdnum != ENDLIST ) {
            if ( i == toupper( p[0]->cmd[0] )) {
                if ( !(p[0]->safety || connected )) addnote = 1;
                lprintf(databox,"%c %-8s : %s\n",
                    (p[0]->safety || connected) ? ' ' : '*',
                    p[0]->cmd, p[0]->help );
                ++ found;
            }
            p[0] ++;
        }
        if (!found) lprintf( commandbox,"No similar functions found\n");
        if (addnote) lprintf( commandbox,"\n* indicates commands not applicable because you are not connected yet\n");
        lprintf( commandbox,"\n");
        return( 0 );
    }

    lprintf(databox,"FTP Program (%s) ", version);
    lprintf(databox,"by Erick Engelke and Dean Roth\n");
    do {
        *buffer = 0;
        for ( i = 0 ; i < 2 ; ++i ) {
            if ( p[i]->cmdnum == ENDLIST ) {
                if ( i ) lprintf( databox,"%s\n", buffer );
                lprintf(databox,"\n");
                if (addnote) lprintf(databox,"* indicates commands not applicable at the moment\n");
                else lprintf(databox,"\n");
                lprintf(databox,"To invoke with a command file, use command line option: -f fname\n");
                return;
            }
            if ( p[i]->safety || connected)
                strcat( buffer, "  ");
            else {
                strcat( buffer, "* ");
                addnote = 1;
            }

            strcat( buffer, p[i]->cmd );
            q[i] = p[i];
            while ((++q[i])->cmdnum == p[i]->cmdnum ) {
                strcat( buffer, ", ");
                strcat( buffer, q[i]->cmd );
            }
            memset( strchr( buffer, 0 ), ' ',
                sizeof( buffer ) - 1 - strlen( buffer ));
            buffer[ 40 * i + 10 ] = 0;
            strcat( buffer, ": ");
            strcat( buffer, p[i]->help );
            memset( strchr( buffer, 0 ), ' ',
                sizeof( buffer ) - 1 - strlen( buffer ));
            p[i] = q[i];
            buffer[ (i+1) * 40 - 1 ] = 0;
        }
        lprintf(databox,"%s\n", buffer );
    } while ( 1 );
}

ftpconnect( tcp_Socket *s, longword host, int port )
{
    int status;
    outstanding = 0;
    if (!tcp_open( s, 0, host, port, NULL )) {
        lprintf( commandbox,"Sorry, unable to connect to that machine right now!");
        return( -1 );
    }
    sock_sturdy( s, 100 );
    sock_mode( s, TCP_MODE_ASCII );
    sock_mode( s, TCP_MODE_NONAGLE );/**/

    lprintf(databox,"waiting...\r");
    sock_wait_established(s, sock_delay, NULL, &status);
    connected = 1;
    outstanding = 1;
    return( 0 );

sock_errE:
    lprintf(databox,"connection could not be established\n");

sock_err:
    lprintf(databox,"connection attempt timed out\n");
    sock_close( s );
    return( -1 );
}

// tempmode - set DATA_ASCII / DATA_BINARY mode or reset if 0
/*
tempmode( int mode )
{
    static intmode = -1;
    if ( ! mode ) mode = intmode;

    if ( intmode != mode )
        sendcommand( s, "TYPE", (mode == DATA_ASCII)?"A":"I" );
    intmode = mode;
}
*/

int ftp(char *userid, longword host, char *hoststring)
{
    tcp_Socket *s;
    int status, len, temp;
    char tempbuf[20];
    char *p, *q;
    int hashon = 4096, exitnow = 0, datamode = DATA_ASCII, interactive = 1;

    s = &ftpctl;
    if ( host )
        ftpconnect( s, host, ftpctl_port);

    do {
process:
        if (outstanding) switch (getresponse( s, &eputs )) {
            case  220 : /* opening message */
                        *buffer = 0;
user:                   if ( *buffer == 0 )
                            linegets( "Userid : ", buffer, 1 );
                        if ((q = strrchr( buffer, ' ')) != NULL ) q++;
                        else q = buffer;
                        sendcommand( s, "USER", q );
                        goto process;
            case  221 : /* closing message */
                        sock_close( s );
                        connected = 0;
                        sock_wait_closed( s, sock_delay, NULL, &status );
                        break;
            case  230 : /* successfully logged in */
                        break;
            case  226 : /* file successfully transferred */
                        break;
            case  331 : /* needs a password */
pass:                   *buffer = 0;
                        linegets( "Password : ", buffer,0);
                        if ( (q = strchr( buffer, ' ')) != NULL ) q++;
                        else q = buffer;
                        sendcommand( s, "PASS", q );
                        memset( buffer, 0, sizeof( buffer ));
                        goto process;
            default   : break;
        }
        if ( exitnow )
            break;

        wathndlcbrk = 1;        /* turn onintelligent cbreak handling */
        watcbroke = 0;          /* but clear last ^C */
        *buffer = 0;
        linegets( "FTP> ", buffer, 1 );
        switch ( text2cmd( buffer, linebuffer, &p, &q )) {
            case    STARTLIST : break;
            case    ENDLIST   : lprintf( commandbox, "    COMMAND NOT UNDERSTOOD\n");
                                break;
            case    STATUS    : if ( connected ) {
                                    lprintf(databox,"Connected\n");
                                    lprintf(databox," mode            : %s\n",
                                        (datamode)? "ASCII":"BINARY" );
                                    lprintf(databox," local directory : %s\n",
                                        getcwd( buffer, sizeof( buffer )));
                                    lprintf(databox," connected to    : %s\n",
                                        hoststring );
                                    lprintf(databox,"\nRemote Status:\n");
                                    sendcommand( s, "STAT", NULL );
                                } else
                                    lprintf( commandbox,"Not connected\n");
                                break;
            case    OPEN      : if ( connected )
                                    lprintf( commandbox,"still connected, must CLOSE current session first\n");
                                else {
                                    lprintf( commandbox, "resolving '%s'\r", p );
                                    if (( host = resolve( p )) == NULL )
                                        lprintf( commandbox,"unable to resolve '%s'\n", p );
                                    else {
                                        lprintf( commandbox,"trying '%s' [%s]\n",
                                            p, inet_ntoa(tempbuf, host) );
                                        temp = ftpctl_port;
                                        if ( q && (temp = atoi( q )))
                                            lprintf( commandbox,"using port %u for this connection\n",temp);
                                        ftpconnect( s, host, temp );
                                    }
                                    datamode = DATA_ASCII;
                                }
                                break;

            case    USER      : *buffer = 0;
                                if ( p )
                                    movmem( p, buffer, strlen( p ) + 1 );
                                goto user;
            case    PASS      : goto pass;
            case    QUOTE     : if ( p ) {
                                    if ( q ) {  // remove the \0 separator
                                        q = strchr( p, 0 );
                                        *q = ' ';
                                    }
                                    sendcommand(s, p, NULL );
                                }
                                else
                                    lprintf(commandbox, "ERROR: nothing to quote\n");
                                break;
            case    CLOSE     : sendcommand( s, "QUIT", NULL );
                                break;
            case    DIR       :
                                if (datamode != DATA_ASCII ) {
                                    sendcommand(s,"TYPE","A" );
                                    if ( getresponse( s, eputs ) != 200 )
                                        break;
                                }
                                remoteop( s, host, "LIST", p,
                                    NULL, 1, 1, 1, NULL, 0);

                                if (datamode != DATA_ASCII ) {
                                    sendcommand( s,"TYPE","I");
                                    if (getresponse( s, eputs ) != 200 ) {
                                        datamode = DATA_ASCII;
                                        lprintf( commandbox,"WARNING: Host left connection in ASCII mode");
                                    }
                                }
                                break;


            case    LS        : if (datamode != DATA_ASCII ) {
                                    sendcommand(s,"TYPE","A" );
                                    if ( getresponse( s, eputs ) != 200 )
                                        break;
                                }
                                remoteop( s, host, "NLST", p,
                                    NULL, 1, 1, 1, NULL, 0);

                                if (datamode != DATA_ASCII ) {
                                    sendcommand( s,"TYPE","I");
                                    if (getresponse( s, eputs ) != 200 ) {
                                        datamode = DATA_ASCII;
                                        lprintf( commandbox,"WARNING: Host left connection in ASCII mode");
                                    }
                                }
                                break;

            case    TYPE      :
                                if (datamode != DATA_ASCII ) {
                                    sendcommand(s,"TYPE","A" );
                                    if ( getresponse( s, eputs ) != 200 )
                                        break;
                                }

                                remoteop( s, host, "RETR", p,
                                    p,1, 1, 1, NULL, 0);

                                if (datamode != DATA_ASCII ) {
                                    sendcommand( s,"TYPE","I");
                                    if (getresponse( s, eputs ) != 200 ) {
                                        datamode = DATA_ASCII;
                                        lprintf( commandbox,"WARNING: Host left connection in ASCII mode");
                                    }
                                }
                                break;
            case    GET       :
                                remoteop( s, host, "RETR", p,
                                    (q)?q:todos(p),1, 0, 0, NULL, hashon);
                                break;

            case    PUT       : remoteop( s, host, "STOR", (q)?q:tounix(p),
                                    p, 0, 0, 0, NULL, hashon);
                                break;

//          case    BGET      : tempmode( DATA_BINARY );
//                              remoteop( s, host, "RETR", p,
//                                  (q)?q:todos(p),1, 0, 0, NULL, hashon);
//                              tempmode( 0 );
//                              break;

//          case    BPUT      : tempmode( BINARY );
//                              remoteop( s, host, "STOR", (q)?q:tounix(p),
//                                  p, 0, 0, 0, NULL, hashon);
//
//                              break;
//                              tempmode( 0 );

            case    MGET      :
                                Fmget( s, host, p, datamode, interactive, hashon);
                                break;

            case    MPUT      :
                                Fmput( s, host, p, datamode, interactive, hashon);
                                break;

            case    DEL       : sendcommand( s, "DELE", p );
                                break;
            case    CHDIR     : sendcommand(s,"CWD", p );
                                break;
            case    RMDIR     : sendcommand(s,"XRMD", p );
                                if ( getresponse( s, eputs ) != 250 )
                                {
                                    lprintf( commandbox,"Trying \"RMD\" instead of \"XRMD\"\n");
                                    sendcommand(s, "RMD", p );
                                }
                                break;
            case    MKDIR     :
                                sendcommand(s,"XMKD", p );
                                if ( getresponse( s, eputs ) != 257 )
                                {
                                    lprintf( commandbox,"Trying \"MKD\" instead of \"XMKD\"\n");
                                    sendcommand(s, "MKD", p );
                                }
                                break;

            case    LDIR      : if (!p) p = "*.*";
                                ldir(p);
                                break;
            case    LDEL      : dosmessage( unlink( p ),"delete file" );
                                break;
            case    PWD       :
                                sendcommand(s, "XPWD", p );
                                if ( getresponse( s, eputs ) != 257 )
                                {
                                    lprintf( commandbox, "Trying \"PWD\" instead of \"XPWD\"\n");
                                    sendcommand(s, "PWD", p );
                                }
                                break;

            case    LCHDIR    : if (p[1] == ':') {
                                    dosmessage( !setdisk( toupper(*p) - 'A' ),
                                        "change disk");
                                    if ( p[2] == 0) break;
                                    p+=2;
                                }
                                dosmessage( chdir( p ), "change directory");
                                break;
            case    LRMDIR    : dosmessage( rmdir( p ), "remove directory" );
                                break;
            case    LMKDIR    : dosmessage( mkdir( p ), "make directory" );
                                break;
            case    LPWD      : p = getcwd((char *)NULL, 128);
                                if (p)
                                {
                                    lprintf( commandbox,"%s\n", p);
                                    free(p);
                                }
                                break;
            case    QUIT      : if (!connected) return(0);
                                sendcommand(s,"QUIT", NULL );
                                exitnow = 1;
                                break;
            case    HELP      : dohelp( p );
                                break;

            case    RHELP     :
                                if (datamode != DATA_ASCII ) {
                                    sendcommand(s,"TYPE","A" );
                                    if ( getresponse( s, eputs ) != 200 )
                                        break;
                                }

                                remoteop( s, host, "HELP", p,
                                    p,1, 1, 1, NULL, 0);

                                if (datamode != DATA_ASCII ) {
                                    sendcommand( s,"TYPE","I");
                                    if (getresponse( s, eputs ) != 200 ) {
                                        datamode = DATA_ASCII;
                                        lprintf( commandbox,"WARNING: Host left connection in ASCII mode");
                                    }
                                }
                                break;

            case    ASCII     : sendcommand(s,"TYPE","A" );
                                if ( getresponse( s, eputs ) == 200 )
                                    datamode = DATA_ASCII;
                                break;
            case    BIN       : sendcommand(s,"TYPE","I" );
                                if ( getresponse(s, eputs ) == 200 )
                                    datamode = DATA_BINARY;
                                break;
            case    INT       : if (!p)
                                    interactive = !interactive;
                                else {
                                    if ( !stricmp( p, "OFF")) interactive = 0;
                                    else interactive = 1;
                                }
                                lprintf( commandbox, "Interactive mode %s\n",
                                    (interactive) ? "enabled" : "disabled");
                                break;
            case    HASH      : if (p) {
                                    if ( !stricmp( p, "on" )) hashon = 1024;
                                    else hashon = atoi( p );
                                } else {
                                    if ( hashon ) hashon = 0;
                                    else hashon = 1024;
                                }
                                if (hashon) lprintf( commandbox,"Hash mark for every %u bytes\n", hashon );
                                else lprintf( commandbox,"Hash marks disabled\n");
                                break;
            case    WAIT      : wait( p );
                                break;
            case    SHELL     : doshell();
                                break;
            case    RUN       : dorun( p );
                                break;
            case    ECHO      : lprintf( commandbox, p );
                                break;
            case    QUIET     : if ( !stricmp( p, "ON" ) ) quiet = 1;
                                else if (!stricmp( p, "OFF" )) quiet = 0;
                                else lprintf( commandbox,"QUIET requires either ON or OFF parameter");
                                break;
            case    DEBUG     : lprintf( commandbox,"Debug mode %s\n",
                                    (debug_on ^= 2)?"enabled":"disabled");
                                break;
            default           : lprintf( commandbox," ** NOT IMPLEMENTED **\n");
                                break;
        }
    } while ( 1 );
    sock_close( s );
    sock_wait_closed( s, sock_delay, NULL, &status);

sock_err:
    switch (status) {
        case 1 : /* foreign host closed */
                 break;
        case -1: /* timeout */
                 lprintf( commandbox,"ERROR: %s\n", sockerr(s));
                 break;
    }
    connected = 0;
    sock_close( s );
    lprintf( commandbox,"\n");
    if ( !exitnow ) goto process;
    return( 0 );
}


main(int argc, char *argv[] )
{
    char *server = NULL;
    longword host = 0;
    int status, i;

    iscolour = peekb( 0x40, 0x49 ) != 7;
    if ( iscolour ) {
        textbackground( BLUE );
        textcolor( WHITE );
    }
    clrscr();

    bigbuf = malloc( BIGBUFLEN );
    bigbuf2 = malloc( BIGBUF2LEN );
    if ( bigbuf == NULL || bigbuf2 == NULL ) {
        lprintf( commandbox,"Insufficient memory to run FTP.");
        if (bigbuf  != NULL ) free( bigbuf );
        if (bigbuf2 != NULL ) free( bigbuf2 );
    }

    lprintf( commandbox,"FTP Transfer Program\n%s\nType HELP for more information\n",
        version);

#ifndef BACK
    // cannot have debug stuff in effect with background TCP i/o

    dbug_init(); /**/
#endif // BACK
    /*tcp_set_debug_state(5);/**/
    sock_init();
    tcp_cbrk( 1 );      /* don't allow control breaks */

    for ( i = 1; i < argc ; ++i ) {
        if ( !stricmp( argv[ i ], "-f")) {
            if ( ++i == argc ) {
                lprintf( commandbox,"ERROR: missing name for command file");
                exit( 3 );
            }
            if ( infile = fopen( argv[ i ], "rt"))
                lprintf( commandbox,"running commands from: %s\n", argv[i] );
            else {
                lprintf( commandbox,"ERROR: unable to read '%s'\n", argv[i] );
                exit( 3 );
            }
            continue;
        }
        if ( server == NULL ) {
            server = argv[i];
            lprintf( commandbox,"looking up '%s'...\r", server );
            if ((host = resolve( server )) == 0L) {
                lprintf( commandbox,"Could not resolve host '%s'\n", server );
                exit( 3 );
            }
        } else if ( atoi( argv[ i ] )) {
            ftpctl_port = atoi( argv[ i ] );
        }
    }

    status = ftp( NULL, host, server );
    if (bigbuf  != NULL ) free( bigbuf );
    if (bigbuf2 != NULL ) free( bigbuf2 );

    gotoxy( 1, 25 );
    printf( "\n");

    return( status );
}





