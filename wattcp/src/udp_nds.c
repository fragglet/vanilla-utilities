#include <copyright.h>
#include <wattcp.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/*
 * Name Domain Service
 *
 * V
 *  0.0 : Jan 11, 1991 : E. Engelke
 */

/*
 * aton()
 *	- converts [a.b.c.d] or a.b.c.d to 32 bit long
 *	- returns 0 on error (safer than -1)
 */

longword aton( char *text )
{
/*    char *p; */
    int i, cur;
    longword ip;

    ip = 0;

    if ( *text == '[' )
	++text;
    for ( i = 24; i >= 0; i -= 8 ) {
	cur = atoi( text );
	ip |= (longword)(cur & 0xff) << i;
	if (!i) break;

	if ((text = strchr( text, '.')) == NULL)
	    return( 0 );	/* return 0 on error */
	++text;
    }
    return(ip);
}

/*
 * isaddr
 *	- returns nonzero if text is simply ip address
 */
int isaddr( char *text )
{
    char ch;
    while ( (ch = *text++) != '\0' ) {
	if ( isdigit(ch) ) continue;
	if ( ch == '.' || ch == ' ' || ch == '[' || ch == ']' )
	    continue;
	return( 0 );
    }
    return( 1 );
}

