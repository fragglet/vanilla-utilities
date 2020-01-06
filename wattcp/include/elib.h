void outch( char ch );		/* print character to stdio */
void outs( char far *s);	/* print a ASCIIZ string to stdio */
void outhex( char ch );
void outhexes( char far *ch, int n );
void qmove( void far * src, void far * dest, int len );
int  sem_up( void far * ptr );
unsigned long set_timeout( unsigned int seconds );
unsigned long set_ttimeout( unsigned int ticks );
int chk_timeout( unsigned long timeout );

unsigned long intel( unsigned long );
unsigned short intel16( unsigned short );

/*
 * qmsg.c
 */
void dputch( char x );
void dmsg( char *s );
void dhex1int( int x );
void dhex2int( int x );
void dhex4int( int x );
