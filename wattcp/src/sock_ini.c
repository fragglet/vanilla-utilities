/*
 * sock_init - easy way to guarentee:
 *	- card is ready
 *	- shutdown is handled
 *	- cbreaks are handled
 *      - config file is read
 *	- bootp is run
 *
 * 0.1 : May 2, 1991  Erick - reorganized operations
 */

#include <copyright.h>
#include <wattcp.h>
#include <stdlib.h>

int _survivebootp = 0;
static char _initialized = 0;		// S. Lawson

void sock_exit( void )
{
    if (_initialized)			// S. Lawson
       tcp_shutdown();
    _initialized = 0;			// S. Lawson
}

// S. Lawson - keep an exiting sock_init()
void sock_init(void)
{
    int r;
    r=sock_init_noexit();
    if (r) exit(r);
}

int sock_init_noexit(void )			// S. Lawson
{
    int r;					// S. Lawson

    if (_initialized) return 0;			// S. Lawson
// S. Lawson    tcp_init();	/* must precede tcp_config because we need eth addr */
    r=tcp_init_noexit();	/* (S. Lawson) must precede tcp_config because we need eth addr */
    if (r) return r;		// S. Lawson
    _initialized=1;		// S. Lawson
    atexit(sock_exit);	/* must not precede tcp_init() incase no PD */
    tcp_cbrk( 0x10 );	/* allow control breaks, give message */

#ifndef SKIPINI // S. Lawson
    if (tcp_config( NULL )) {	/* if no config file use BOOTP w/broadcast */
#endif // SKIPINI  S. Lawson
	_bootpon = 1;
	outs("Configuring through BOOTP/DHCP\r\n");    // S. Lawson
#ifndef SKIPINI // S. Lawson
    }
#endif // SKIPINI  S. Lawson

    if (_bootpon)	/* non-zero if we use bootp */
	if (_dobootp()) {
	    outs("BOOTP/DHCP failed\r\n");             // S. Lawson
	    if ( !_survivebootp )
	       return 3;			       // S. Lawson
// S. Lawson		exit( 3 );
	}
    return 0;					       // S. Lawson
}
