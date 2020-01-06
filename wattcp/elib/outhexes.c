/*
 * outhexes - dump n hex bytes to stdio
 *
 */

#include <elib.h>

void outhexes( char far *p, int n )
{
    while ( n-- > 0) {
        outhex( *p++);
        outch(' ');
    }
}
