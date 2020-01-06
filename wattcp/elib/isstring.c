#include <stdio.h>
#include <ctype.h>
#include <string.h>

int isstring( char *string, unsigned stringlen )
{
    if ( (unsigned) strlen( string ) > stringlen - 1 ) return( 0 );

    while ( *string ) {
        if ( !isprint( *string++ )) {
            string--;
            if ( !isspace( *string++ ))
                return( 0 );
        }
    }
    return( 1 );
}

