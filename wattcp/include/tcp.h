
/*
 * Waterloo TCP
 *
 * Copyright (c) 1990, 1991, 1992, 1993 Erick Engelke
 *
 * Portions copyright others, see copyright.h for details.
 *
 * This library is free software; you can use it or redistribute under
 * the terms of the license included in LICENSE.H.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * file LICENSE.H for more details.
 *
 */
#ifndef _wattcp_tcp_h
#define _wattcp_tcp_h

#ifndef WTCP_VER

#define __WATTCP_USER__  /* Not compiling the kernel itself, hide some stuff */

#include <wattcp.h>

/* handle some early dumb naming conventions */
#define dbuginit()      dbug_init()

/* Kernal version (major major minor minor) */
#define WTCP_VER 0x0105

/*
 * Typedefs and constants
 */

typedef struct in_addr {
    longword    s_addr;
};

/*
 * BSD-style socket info routines -- bsdname.c
 */
int getpeername( sock_type *s, void *dest, int *len );
int getsockname(  sock_type *s, void *dest, int *len );
char *getdomainname( char *name, int length );
char *setdomainname( char *string );
char *gethostname( char *name, int len );
char *sethostname( char *name );
void psocket( sock_type *s );

/*
 * ICMP-related stuff -- pcicmp.c
 */
/*
 * Send an icmp echo request using specified ttl and tos.
 * if(icmp_id != NULL) store copy of the id there
 */
int _send_ping( longword host, longword countnum, byte ttl,
                                            byte tos, longword *theid );
/* backward compatibility */
#define _ping( h, c ) _send_ping( h, c, 250, 0, NULL )
longword _chk_ping( longword host , longword *ptr);

/*
 * Daemons -- wattcpd.c
 */
int addwattcpd( void (*p)( void ) );
int delwattcpd( void (*p)( void ) );

/*
 * Background net I/O processing -- netback.c
 */
void backgroundon( void );
void backgroundoff(void);
void backgroundfn( void (*fn)() );

/*
 * More background processing -- pcintr.c
 */
void wintr_enable( void );
void wintr_disable( void );
void wintr_shutdown( void );
void wintr_init( void );

/*
 * Socket stats -- pcstat.c
 */
int sock_stats( sock_type *s, word *days, word *inactive, word *cwindow, word *avg, word *sd );

/*
 * Debug test routine -- test.c
 */
void debugpsocketlen( void );

#endif /* WTCP_VER */

#endif /* ndef _wattcp_tcp_h */
