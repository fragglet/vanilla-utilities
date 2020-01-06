#include <copyright.h>
#include <wattcp.h>
#include <dos.h>


word wathndlcbrk = 0;    /* changes operation of the
                         * break stuff if in resolve
                         * or something
                         */
word watcbroke = 0;      /* set to non-zero if wathndlcbrk happenned */

/*
 * tcp_cbreak( mode )
 * 	- mode is composed of the following flags
 *	   0x01 - disallow breakouts
 *	   0x10 - display a message upon Cbreak
 *
 */

static char *msgs[] = {
    "\n\rTerminating program\n\r",
    "\n\rCtrl-Breaks ignored\n\r" };

static cbrkmode = 0;

static int handler( void )
{
    if ( wathndlcbrk ) {
        watcbroke = 1;
        if (cbrkmode & 0x10 ) outs("\n\rInterrupting\n\r");
        return( 1 );
    }
    if (cbrkmode & 0x10 )
	outs( msgs[ cbrkmode & 1 ]);
    if (cbrkmode & 1)
	return( 1 );
    tcp_shutdown();
    return( 0 );
}

void tcp_cbrk( int mode )
{
    cbrkmode = mode;
    ctrlbrk(handler);
}
