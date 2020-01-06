#include <copyright.h>
#include <wattcp.h>

#include <io.h>
#include <dos.h>	/* for _argv */
#include <fcntl.h>	/* open modes */
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define MY_IP		"MY_IP"
#define IP              "IP"
#define NETMASK 	"NETMASK"
#define COOKIE		"COOKIE"
#define NAMESERVER	"NAMESERVER"
#define GATEWAY 	"GATEWAY"
#define DOMAINS		"DOMAINSLIST"
#define HOSTNAME	"HOSTNAME"
#define SOCKDELAY       "SOCKDELAY"
#define ETHIP		"ETHIP"
#define MSS             "MSS"
#define BOOTP		"BOOTP"
#define BOOTPTO		"BOOTPTO"
#define DOMTO           "DOMAINTO"
#define PRINT		"PRINT"
#define INACTIVE        "INACTIVE"
#define INCLUDE         "INCLUDE"
#define MULTIHOMES      "MULTIHOMES"
#define DATATIMEOUT     "DATATIMEOUT"       /* 99.08.23 EE */


#define is_it( x ) if (!strcmp(name,x))

#ifndef SKIPINI		// S. Lawson
/*
 * _inet_atoeth - read src, dump to ethernet buffer
 *		  and return pointer to end of text string
 */
char *_inet_atoeth( char *src, byte *eth )
{
    word count, val;
    byte ch, icount;

    val = count = icount = 0;
    while ((ch = toupper(*src++)) != 0 ) {
	if (ch == ':' ) continue;
	if (ch == ',' ) break;

	if ((ch -= '0') > 9) ch -= 7;
	val = (val << 4) + ch;

	if ( icount ) {
	    eth[ count++ ] = val;
	    if (count > 5) break;
	    val = icount = 0;
	} else
	    icount = 1;
    }
    if ( *src == ',' ) ++src;
    return( src );
}

static void ethip( char *s )
{
    eth_address temp_eth;
    char *temp;

    if (( temp = _inet_atoeth( s, (byte*)&temp_eth )) != NULL ) {
	if (!memcmp( &temp_eth, &_eth_addr, sizeof( eth_address ))) {
	    my_ip_addr = inet_addr( temp );
	}
    }
}
#endif // SKIPINI   S. Lawson

void _add_server( int *counter, int max, longword *array, longword value )
{
    int i,duplicate=0;

    if ( value && ( *counter < max )) {
	for (i=0;i<*counter; i++) {
	    if ( array[i] == value)
		duplicate=1;
	}
	if (!duplicate)
	     array[ (*counter)++ ] = value;
    }
}

word sock_delay = 30;
word sock_inactive = 0;  /* defaults to forever */
word multihomes = 0;
char defaultdomain[ 80 ];
longword _cookie[ MAX_COOKIES ];
int _last_cookie;
extern _domaintimeout;
void (*usr_init)(char *name, char *value) = NULL;

#ifndef SKIPINI		// S. Lawson
static void set_values(char *name, char *value )
{
    char *p;
/*    longword temp; */
    word i;

    strupr(name);
    is_it( MY_IP ) {
	if ( toupper( *value ) == 'B' ||
	     toupper( *value ) == 'D') _bootpon = 1;	// S. Lawson
	else my_ip_addr = resolve( value );
    } else is_it( IP ) {
	if ( toupper( *value ) == 'B' ||
	     toupper( *value ) == 'D') _bootpon = 1;	// S. Lawson
	else my_ip_addr = resolve( value );
    }
    else is_it( NETMASK) sin_mask = resolve( value );
    else is_it( GATEWAY)
	/* accept gateip[,subnet[,mask]]  */
	_arp_add_gateway( value , 0L );
    else is_it( NAMESERVER )  _add_server( &_last_nameserver,
		MAX_NAMESERVERS, def_nameservers, resolve(value));
    else is_it( COOKIE ) _add_server( &_last_cookie, MAX_COOKIES,
		_cookie, resolve( value ));
    else is_it( DOMAINS ) def_domain = strcpy( defaultdomain, value );
    else is_it( HOSTNAME ) strncpy(_hostname, value, MAX_STRING );
    else is_it( SOCKDELAY ) sock_delay = atoi( value );
    else is_it( ETHIP )  ethip( value );
    else is_it( MSS ) _mss = atoi( value );
    else is_it( BOOTP ) _bootphost = resolve( value );
    else is_it( BOOTPTO) _bootptimeout = atoi( value );
    else is_it( DOMTO ) _domaintimeout = atoi( value );
    else is_it( INACTIVE ) sock_inactive = atoi( value );
    else is_it( MULTIHOMES ) multihomes = atoi( value );
    else is_it( DATATIMEOUT) sock_data_timeout = atoi( value ); /* EE 99.08.23 */
    else is_it( PRINT ) {
	outs( value );
	outs( "\n\r" );
    }
    else is_it( INCLUDE ) {
        if ( *(p = value) == '?') p++;
        if ((i = _open( p, O_RDONLY | O_TEXT )) != 0xffff ) {
	    _close( i );
            tcp_config( p );
        } else if ( *value != '?' ) {
            outs("\n\rUnable to open '");
            outs( p );
            outs("'\n\r");
        }
    }
    else {
	if ( usr_init )
	    (*usr_init)(name, value);
    }
}

static char *watfname = "WATTCP.CFG";
static char *genfname = "TCP.CFG";

// S. Lawson - set the config filename, or clear to skip
void tcp_config_file(const char *fname)
{
    watfname=(char *) fname;
}

int tcp_config( char *path )
{
    char name[80], *temp, *namepart;
    char value[80], ch[2];
    int  quotemode;
    int f, mode;

    namepart = NULL;
    if (!path) {
	if (watfname==NULL) return (-1);	// S. Lawson
	if ((path = getenv( watfname )) != NULL ) {
	    path = strcpy( name, path );
	    strcat( name, "\\");
	} else {
	    strcpy( name, _argv[0] );
	    path = ( *name && (name[1] == ':')) ? &name[2] : name;
	    if ((temp = strrchr( path, '\\' )) == NULL) temp = path;
	    *(++temp) = 0;
	}
        namepart = temp;
        strcat( name, watfname );
    } else
        strcpy( name, path );

    if ( ( f = _open( name, O_RDONLY | O_TEXT )) == -1 ) {
	/* try local subdirectory */
        if (( f = _open( watfname, O_RDONLY | O_TEXT )) == -1 ){
            if (( f = _open( genfname, O_RDONLY | O_TEXT )) == -1 ){
                strcpy( namepart, genfname );
                if (( f = _open( name, O_RDONLY | O_TEXT )) == -1 ){
                    outs("Config file not found\n\r");
                    return( -1 );
                }
            }
	}
    }
    *name = *value = mode = ch[1] = quotemode = 0;
    while ( _read( f, &ch, 1 ) == 1) {
	switch( *ch ) {
	    case  '\"': quotemode ^= 1;
			break;
	    case  ' ' :
	    case  '\t': if (quotemode) goto addit;
			break;

	    case  '=' : if (quotemode) goto addit;
			if (!mode) mode = 1;
			break;
	    case  '#' :
	    case  ';' : if (quotemode) goto addit;
			mode = 2;
			break;
	    case  '\n':
	    case  '\r': if (*name && *value)
			    set_values(name, value);
			*name = *value = quotemode = mode = 0;
			break;
	    default   :
addit:
      			switch (mode ) {
			case 0 : strcat(name, ch);
				 break;
			case 1 : strcat(value, ch);
				 break;
			}
			break;
	}
    }
    _close(f );
    return( 0 );
}
#endif // SKIPINI  S. Lawson
